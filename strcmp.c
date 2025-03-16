#include <stdint.h>

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