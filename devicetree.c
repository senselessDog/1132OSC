// devicetree.c
#include "devicetree.h"
#include <string.h>
void uart_send_string(const char *str);
void uart_send_hex(uint32_t value);
// 保存 DTB 的地址
static void *g_fdt_addr = NULL;

uint64_t g_initramfs_addr = 0;
uint64_t g_initramfs_size = 0;

// 初始化 FDT 解析器
int fdt_init(void *fdt_addr)
{
    uart_send_string("[fdt_init] start DTB init\r\n");
    g_fdt_addr = fdt_addr;

    // 檢查魔術數字
    struct fdt_header *header = (struct fdt_header *)fdt_addr;
    uart_send_string("[fdt_init] change endian\r\n");
    uart_send_hex(header->magic);
    uart_send_string("\r\n");
    uint32_t magic = fdt32_to_cpu(header->magic);
    uart_send_string("[fdt_init] start DTB init check\r\n");
    if (magic != 0xd00dfeed)
    {
        uart_send_string("Invalid DTB magic number\r\n");
        return -1;
    }

    return 0;
}

// 大端序轉換函數
uint32_t fdt32_to_cpu(uint32_t val)
{
    uint32_t b0 = (val & 0xff) << 24;
    uint32_t b1 = (val & 0xff00) << 8;
    uint32_t b2 = (val & 0xff0000) >> 8;
    uint32_t b3 = (val & 0xff000000) >> 24;
    return b0 | b1 | b2 | b3;
}

// 遍歷 DTB 並對每個節點調用回調函數
int fdt_traverse(fdt_callback_t callback, void *arg)
{
    uart_send_string("start traverse \r\n");
    if (!g_fdt_addr || !callback)
    {
        return -1;
    }
    struct fdt_header *header = (struct fdt_header *)g_fdt_addr;
    uart_send_string("Found header: \r\n");
    uart_send_hex(header->magic);
    uart_send_string("\r\n");
    uint32_t struct_offset = fdt32_to_cpu(header->off_dt_struct);
    uint32_t strings_offset = fdt32_to_cpu(header->off_dt_strings);

    // 遍歷結構塊
    uint32_t *p = (uint32_t *)((char *)g_fdt_addr + struct_offset);
    char path[256] = {0};
    int path_len = 0;

    while (1)
    {
        uint32_t token = fdt32_to_cpu(*p++);

        switch (token)
        {
        case FDT_BEGIN_NODE:
        {
            char *name = (char *)p;
            int len = strlen(name);

            // 更新路徑
            if (path_len > 0 && path[path_len - 1] != '/')
            {
                path[path_len++] = '/';
            }
            strcpy(path + path_len, name);
            path_len += len;

            // 跳過名稱（包括末尾的 null 字節）並保持 4 字節對齊
            p = (uint32_t *)((char *)p + ((len + 4) & ~3));
            break;
        }

        case FDT_END_NODE:
        {
            // 回溯路徑
            while (path_len > 0 && path[--path_len] != '/')
                ;
            path[path_len] = '\0';
            break;
        }

        case FDT_PROP:
        {
            struct fdt_property *prop = (struct fdt_property *)p;
            uint32_t len = fdt32_to_cpu(prop->len);
            uint32_t nameoff = fdt32_to_cpu(prop->nameoff);

            // 獲取屬性名稱
            char *name = (char *)g_fdt_addr + strings_offset + nameoff;

            // 獲取屬性值
            void *data = (void *)(prop + 1);
            uart_send_string("Found property: \r\n");
            uart_send_string(path);
            uart_send_string("\r\n");
            uart_send_string(name);
            uart_send_string("\r\n");
            uart_send_string(data);
            uart_send_string("\r\n");
            // 調用回調函數
            if (callback(path, name, data, len, arg) != 0)
            {
                return 0; // 回調請求停止遍歷
            }

            // 跳過屬性結構和值（保持 4 字節對齊）
            p = (uint32_t *)((char *)data + ((len + 3) & ~3));
            break;
        }

        case FDT_END:
            return 0; // 遍歷完成

        case FDT_NOP:
            break; // 不操作，繼續

        default:
            uart_send_string("Unknown DTB token\r\n");
            return -1;
        }
    }

    return 0;
}

// 用於保存 initramfs 信息的結構
struct initramfs_info
{
    uint64_t start_addr;
    uint64_t size;
    int found;
};

// 回調函數，用於查找 initramfs 地址
int initramfs_callback(const char *path, const char *name, const void *data, uint32_t size, void *arg)
{
    struct initramfs_info *info = (struct initramfs_info *)arg;
    // uart_send_string("Found /chosen: \r\n");
    // 檢查路徑和屬性名稱
    if (strcmp(name, "linux,initrd-start") == 0)
    {
        g_initramfs_addr = (uint64_t)fdt32_to_cpu((uint32_t)data);
    }
    else if (strcmp(name, "linux,initrd-end") == 0)
    {
        uint32_t end_addr = fdt32_to_cpu((uint32_t)data);
        g_initramfs_size = (uint64_t)(end_addr - g_initramfs_addr);
    }

    return 0; // 繼續遍歷
}

// 獲取 initramfs 地址和大小
int get_initramfs_info(void *dtb_addr)
{
    struct initramfs_info info = {0};
    if (fdt_traverse(initramfs_callback, &info) == 0 && info.found)
    {
        g_initramfs_addr = info.start_addr;
        g_initramfs_size = info.size;
        uart_send_string("Found initramfs at address: 0x");
        uart_send_hex((uint32_t)g_initramfs_addr);
        uart_send_string(" with size: 0x");
        uart_send_hex((uint32_t)g_initramfs_size);
        uart_send_string("\r\n");
        return 0;
    }
    else
    {
        uart_send_string("Initramfs not found in DTB, using default address\r\n");
        // 如果無法從 DTB 中獲取，使用默認地址作為備選
        g_initramfs_addr = 0x20000000;
        return -1;
    }
}

void dtb_search()
{
    // 獲取 initramfs 地址
    struct initramfs_info info = {0};
    fdt_traverse(initramfs_callback, &info);
    if (info.found)
    {
        uart_send_string("Found initramfs at address: ");
        // 打印地址（需要實現整數到字符串的轉換函數）
        // ...
        uart_send_string("\r\n");

        // 使用找到的 initramfs 地址和大小
        // ...
    }
    else
    {
        uart_send_string("Initramfs not found in DTB\r\n");
    }
}