#include <stdint.h>
#include <stddef.h>
int strcmp(const char *s1, const char *s2)
{
    while (*s1 && (*s1 == *s2))
    {
        s1++;
        s2++;
    }
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

void *memcpy(void *dest, const void *src, uint32_t n)
{
    char *d = dest;
    const char *s = src;
    while (n--)
    {
        *d++ = *s++;
    }
    return dest;
}

int strncmp(const char *s1, const char *s2, uint32_t n)
{
    while (n && *s1 && (*s1 == *s2))
    {
        s1++;
        s2++;
        n--;
    }
    if (n == 0)
    {
        return 0;
    }
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

long strtol(const char *nptr, char **endptr, int base)
{
    long result = 0;
    while (*nptr)
    {
        int digit;
        if (*nptr >= '0' && *nptr <= '9')
        {
            digit = *nptr - '0';
        }
        else if (*nptr >= 'a' && *nptr <= 'f')
        {
            digit = *nptr - 'a' + 10;
        }
        else if (*nptr >= 'A' && *nptr <= 'F')
        {
            digit = *nptr - 'A' + 10;
        }
        else
        {
            break;
        }

        if (digit >= base)
        {
            break;
        }

        result = result * base + digit;
        nptr++;
    }

    if (endptr)
    {
        *endptr = (char *)nptr;
    }

    return result;
}

// 計算字符串長度
size_t strlen(const char *str)
{
    size_t len = 0;
    while (str[len] != '\0')
    {
        len++;
    }
    return len;
}

// 字符串複製
char *strcpy(char *dest, const char *src)
{
    char *d = dest;
    while ((*d++ = *src++) != '\0')
        ;
    return dest;
}


char *int2str(int num, char *str)
{
    int i = 0;
    int isNegative = 0;

    // Handle 0 explicitly
    if (num == 0)
    {
        str[i++] = '0';
        str[i] = '\0';
        return str;
    }

    // Handle negative numbers
    if (num < 0)
    {
        isNegative = 1;
        num = -num;
    }

    // Process individual digits
    while (num != 0)
    {
        str[i++] = (num % 10) + '0';
        num /= 10;
    }

    // If the number is negative, append '-'
    if (isNegative)
        str[i++] = '-';

    str[i] = '\0'; // Null-terminate the string

    // Reverse the string
    for (int j = 0; j < i / 2; j++)
    {
        char temp = str[j];
        str[j] = str[i - j - 1];
        str[i - j - 1] = temp;
    }

    return str;
}

char *strtok(char *str, const char *delimiters)
{
    static char *last_token = NULL;
    char *token_start;

    // 如果 str 不為空，則從 str 開始
    // 如果 str 為空，則從上次的位置繼續
    if (str != NULL)
    {
        token_start = str;
    }
    else if (last_token != NULL)
    {
        token_start = last_token;
    }
    else
    {
        return NULL;
    }

    // 跳過開頭的分隔符
    while (*token_start != '\0')
    {
        const char *d = delimiters;
        int is_delimiter = 0;

        while (*d != '\0')
        {
            if (*token_start == *d)
            {
                is_delimiter = 1;
                break;
            }
            d++;
        }

        if (!is_delimiter)
        {
            break;
        }

        token_start++;
    }

    // 如果到了字串末尾，則返回 NULL
    if (*token_start == '\0')
    {
        last_token = NULL;
        return NULL;
    }

    // 找到下一個分隔符
    char *token_end = token_start;
    while (*token_end != '\0')
    {
        const char *d = delimiters;
        int is_delimiter = 0;

        while (*d != '\0')
        {
            if (*token_end == *d)
            {
                is_delimiter = 1;
                break;
            }
            d++;
        }

        if (is_delimiter)
        {
            break;
        }

        token_end++;
    }

    // 如果找到分隔符，則標記為 '\0' 並更新 last_token
    if (*token_end != '\0')
    {
        *token_end = '\0';
        last_token = token_end + 1;
    }
    else
    {
        last_token = NULL;
    }

    return token_start;
}

int atoi(const char *str)
{
    int result = 0;
    int sign = 1;

    // 跳過空白字符
    while (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r')
    {
        str++;
    }

    // 處理符號
    if (*str == '-')
    {
        sign = -1;
        str++;
    }
    else if (*str == '+')
    {
        str++;
    }

    // 處理數字
    while (*str >= '0' && *str <= '9')
    {
        result = result * 10 + (*str - '0');
        str++;
    }

    return sign * result;
}
//lab6(mmu.c)
void* memset(void *s, int c, size_t n) {
    unsigned char *p = (unsigned char *)s; // 將指標轉換為 unsigned char* 以便逐位元組操作

    // 迴圈 n 次，將每個位元組設定為 c
    for (size_t i = 0; i < n; i++) {
        p[i] = (unsigned char)c;
    }

    return s; // 返回原始指標
}

char *strrchr(const char *str, int c) {
    const char *last_occurrence = NULL;
    char char_to_find = (char)c; // Convert int to char

    // Iterate through the string until the null terminator is reached
    while (*str != '\0') {
        if (*str == char_to_find) {
            last_occurrence = str; // Update last_occurrence if character is found
        }
        str++; // Move to the next character
    }

    // Check if the character is the null terminator itself
    if (char_to_find == '\0') {
        last_occurrence = str; // 'str' now points to the null terminator
    }

    // Cast away constness before returning, as per standard library function signature
    return (char *)last_occurrence;
}

char *strchr(const char *str, int c) {
    char char_to_find = (char)c; // Convert int to char

    // Iterate through the string, including the null terminator
    while (1) {
        if (*str == char_to_find) {
            return (char *)str; // Found it, return pointer
        }
        if (*str == '\0') {
            break; // Reached end of string, character not found
        }
        str++; // Move to the next character
    }

    return NULL; // Character not found
}

char* strncpy(char *dest, const char *src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    // 如果 src 的長度小於 n，用 '\0' 填充 dest 的剩餘部分
    // for ( ; i < n; i++) {
    //     dest[i] = '\0';
    // }
    // 為了安全，我們通常希望即使 n 用完了，也要確保 dest 有一個結尾的 null (如果空間允許)
    // 但標準 strncpy 如果 src 長度 >= n，則不保證 null 結尾。
    // 在你的使用情境中，你通常會在之後手動設定 dest[MAX_PATHNAME_LEN] = '\0'，所以這裡可以簡化。

    // 實作一個更安全的版本，至少在 n > 0 時確保 dest 有 null 結尾（如果原始 src 較短）
    // 或者在 n 耗盡前，如果 src 結束了，就補一個 null
    if (i < n) { // src 比較短，或者剛好在第 n 個字元是 '\0'
        dest[i] = '\0'; // 確保 null 結尾
    }
    // 如果 i == n 且 src[n-1] 不是 '\0'，那麼 dest 就沒有 null 結尾
    // 這時需要調用者在外部處理，例如：
    // manual_strncpy(buffer, source, MAX_LEN);
    // buffer[MAX_LEN] = '\0'; // 或者 buffer[MAX_LEN-1] = '\0' 如果 MAX_LEN 是包含 null 的總長度

    return dest;
}