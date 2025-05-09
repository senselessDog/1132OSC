#include <stdint.h>
#include <string.h>

void uart_send_string(const char *str);
void uart_send_hex(uint32_t value);
void uart_send(char c);
char uart_recv();
void uart_send_int(int value);
struct cpio_newc_header
{
    char c_magic[6];
    char c_ino[8];
    char c_mode[8];
    char c_uid[8];
    char c_gid[8];
    char c_nlink[8];
    char c_mtime[8];
    char c_filesize[8];
    char c_devmajor[8];
    char c_devminor[8];
    char c_rdevmajor[8];
    char c_rdevminor[8];
    char c_namesize[8];
    char c_check[8];
};
static int hex_to_int(char *p, int len)
{
    int val = 0;
    int tmp;

    for (int i = 0; i < len; i++)
    {
        tmp = *(p + i);
        if (tmp >= 'A')
            tmp = tmp - 'A' + 10;
        else
            tmp -= '0';
        val <<= 4;
        val |= tmp;
    }
    return val;
}
void parse_cpio_archive(char *archive)
{

    char filename[1024];
    int index = 0;
    // Receive the filename from the user
    uart_send_string("\r\n");
    uart_send_string("Filename: ");
    while (1)
    {

        char c = uart_recv();
        uart_send(c); // Echo the received character
        if (c == '\r' || c == '\n')
        {
            filename[index] = '\0'; // Null-terminate the string
            index = 0;
            uart_send_string("\r\n");
            break;
        }
        else
        {
            filename[index] = c;
            index++;
        }
    }
    // start parsing the archive
    struct cpio_newc_header *header;
    char *ptr = archive;
    while (1)
    {

        header = (struct cpio_newc_header *)ptr;
        ptr += sizeof(*header);

        if (strcmp(ptr, "TRAILER!!!") == 0)
        {
            uart_send_string("End of CPIO archive\r\n");
            return;
        }

        // Get the namesize and filesize
        int namesize = hex_to_int(header->c_namesize, 8);
        int filesize = hex_to_int(header->c_filesize, 8);
        if (strcmp(ptr, filename) == 0)
        {
            ptr += namesize;
            if ((ptr - (char *)header) % 4 != 0)
            {
                ptr += 4 - ((ptr - (char *)header) % 4);
            }
            for (int i = 0; i < filesize; i++)
            {
                uart_send(ptr[i]);
            }
            uart_send_string("\r\n");
            uart_send_string("File found and read successfully\r\n");
            return;
        }
        ptr += namesize;
        if ((ptr - (char *)header) % 4 != 0)
        {
            ptr += 4 - ((ptr - (char *)header) % 4);
        }

        ptr += filesize;

        if (filesize % 4 != 0)
        {
            ptr += 4 - (filesize % 4);
        }
    }
}

void list_cpio_files(char *archive)
{
    char *ptr = archive;
    while (1)
    {
        struct cpio_newc_header *header;
        header = (struct cpio_newc_header *)ptr;
        ptr += sizeof(*header);
        if (strcmp(ptr, "TRAILER!!!") == 0)
        {
            uart_send_string("End of CPIO archive\r\n");
            return;
        }
        uart_send_string("[list_cpio_files] parse\r\n");
        int namesize = hex_to_int(header->c_namesize, 8);
        int filesize = hex_to_int(header->c_filesize, 8);

        uart_send_string(ptr);
        uart_send_string("\r\n");
        ptr += namesize;
        if ((ptr - (char *)header) % 4 != 0)
        {
            ptr += 4 - ((ptr - (char *)header) % 4);
        }
        ptr += filesize;

        if ((ptr - (char *)header) % 4 != 0)
        {
            ptr += 4 - ((ptr - (char *)header) % 4);
        }
    }
}
struct file_information
{
    void *filecontext;
    int filesize;
};
struct file_information find_program_in_initramfs(char *archive, const char *filename)
{

    // 開始解析檔案
    struct cpio_newc_header *header;
    char *ptr = archive;
    while (1)
    {
        header = (struct cpio_newc_header *)ptr;
        ptr += sizeof(*header);

        // 檢查是否到達尾部
        if (strcmp(ptr, "TRAILER!!!") == 0)
        {
            uart_send_string("Program not found in initramfs\r\n");
            struct file_information file_info = {NULL, 0};
            return file_info;
        }

        // 取得檔案名稱長度與檔案大小
        int namesize = hex_to_int(header->c_namesize, 8);
        int filesize = hex_to_int(header->c_filesize, 8);

        uart_send_string("file name address: ");
        uart_send_hex(ptr);
        uart_send_string("\r\n");
        uart_send_string("file: ");
        uart_send_string(ptr);
        uart_send_string("\r\n");
        // 檢查檔案名稱是否匹配
        if (strcmp(ptr, filename) == 0)
        {
            // 找到了匹配的檔案
            uart_send_string("Program found: ");
            uart_send_string(filename);
            uart_send_string("\r\n");

            // 計算檔案內容的位置
            char *file_content = ptr + namesize;

            // 對齊到4位元組邊界
            if ((file_content - (char *)header) % 4 != 0)
            {
                file_content += 4 - ((file_content - (char *)header) % 4);
            }
            struct file_information file_info;
            file_info.filecontext = (void *)file_content;
            file_info.filesize = filesize;
            // 返回檔案內容的指針
            return file_info;
        }

        // 移動到下一個檔案
        ptr += namesize;

        // 對齊到4位元組邊界
        if ((ptr - (char *)header) % 4 != 0)
        {
            ptr += 4 - ((ptr - (char *)header) % 4);
        }

        // 跳過檔案內容
        ptr += filesize;

        // 對齊到4位元組邊界
        if (filesize % 4 != 0)
        {
            ptr += 4 - (filesize % 4);
        }
    }
}