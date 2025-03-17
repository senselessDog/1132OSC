// devicetree.h
#ifndef DEVICETREE_H
#define DEVICETREE_H

#include <stdint.h>

// FDT 標頭格式
struct fdt_header
{
    uint32_t magic;             // 魔術數字 (0xd00dfeed)
    uint32_t totalsize;         // DTB 檔案總大小
    uint32_t off_dt_struct;     // 結構塊偏移
    uint32_t off_dt_strings;    // 字符串塊偏移
    uint32_t off_mem_rsvmap;    // 保留記憶體塊偏移
    uint32_t version;           // 版本
    uint32_t last_comp_version; // 最後兼容版本
    uint32_t boot_cpuid_phys;   // 啟動 CPU ID
    uint32_t size_dt_strings;   // 字符串塊大小
    uint32_t size_dt_struct;    // 結構塊大小
};

// FDT 結構塊中的標記類型
#define FDT_BEGIN_NODE 0x1 // 開始節點
#define FDT_END_NODE 0x2   // 結束節點
#define FDT_PROP 0x3       // 屬性
#define FDT_NOP 0x4        // 空操作
#define FDT_END 0x9        // 結束標記

// 屬性結構
struct fdt_property
{
    uint32_t len;     // 值長度
    uint32_t nameoff; // 名稱在字符串塊中的偏移
    // 緊接著是屬性的值
};

// 回調函數類型，用於遍歷節點
typedef int (*fdt_callback_t)(const char *path, const char *name, const void *data, uint32_t size, void *arg);

// 初始化 FDT 解析器
int fdt_init(void *fdt_addr);

// 遍歷 DTB 並對每個節點調用回調函數
int fdt_traverse(fdt_callback_t callback, void *arg);

// 查找特定路徑的節點
int fdt_get_node(const char *path, fdt_callback_t callback, void *arg);

// 大端序轉換函數 (DTB 使用大端序)
uint32_t fdt32_to_cpu(uint32_t val);

#endif // DEVICETREE_H