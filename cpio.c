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
        uart_send_string("[cat] NameMatch\r\n");
        uart_send_int(strcmp(ptr, "file1.txt"));
        uart_send_string("\r\n");
        if (strcmp(ptr, filename) == 0)
        {
            ptr += namesize;
            if ((ptr - (char *)header) % 4 != 0)
            {
                ptr += 4 - ((ptr - (char *)header) % 4);
            }
            // buffer[filesize] = '\0';
            uart_send_string("Start\r\n");
            for (int i = 0; i < filesize; i++)
            {
                uart_send(ptr[i]);
            }
            uart_send_string("\r\n");
            uart_send_string("End\r\n");
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
        uart_send_string("[list_cpio_files] start\r\n");
        uart_send_string(ptr);
        uart_send_string("\r\n");
        header = (struct cpio_newc_header *)ptr;
        uart_send_string(header->c_magic);
        uart_send_string("\r\n");
        ptr += sizeof(*header);
        if (strcmp(ptr, "TRAILER!!!") == 0)
        {
            uart_send_string("End of CPIO archive\r\n");
            return;
        }

        // if (strncmp(header.c_magic, "070701", 6) != 0)
        // {
        //     uart_send_string("[list_cpio_files] debug\r\n");
        //     uart_send_hex(ptr);
        //     uart_send_string("\r\n");
        //     uart_send_string(header.c_magic);
        //     uart_send_string("\r\n");
        //     uart_send_string("Invalid CPIO magic number\r\n");
        //     return;
        // }
        uart_send_string("[list_cpio_files] parse\r\n");
        int namesize = hex_to_int(header->c_namesize, 8);
        int filesize = hex_to_int(header->c_filesize, 8);
        uart_send_int(namesize);
        uart_send_string("\r\n");
        uart_send_int(filesize);
        uart_send_string("\r\n");

        uart_send_string("[list_cpio_files] name\r\n");
        uart_send_string(ptr);
        uart_send_string("\r\n");

        // char name[namesize];
        uart_send_string("[list_cpio_files] namesize & filesize\r\n");
        ptr += namesize;
        if ((ptr - (char *)header) % 4 != 0)
        {
            ptr += 4 - ((ptr - (char *)header) % 4);
        }
        uart_send_string("[list_cpio_files] file context\r\n");
        uart_send_string(ptr);
        uart_send_string("\r\n");
        ptr += filesize;

        if ((ptr - (char *)header) % 4 != 0)
        {
            ptr += 4 - ((ptr - (char *)header) % 4);
        }
    }
}