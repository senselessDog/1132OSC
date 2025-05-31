#ifndef STRING_H
#define STRING_H

#include <stdint.h>
#include <stddef.h>

// String comparison
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, uint32_t n);
char *strrchr(const char *str, int c);
char *strchr(const char *str, int c);
char* strncpy(char *dest, const char *src, size_t n);
// String conversion
long strtol(const char *nptr, char **endptr, int base);
int atoi(const char *str);
char *int2str(int num, char *str);

// String manipulation
size_t strlen(const char *str);
char *strcpy(char *dest, const char *src);
char *strtok(char *str, const char *delimiters);

// Memory operations
void *memcpy(void *dest, const void *src, uint32_t n);
void *memset(void *s, int c, size_t n);

#endif /* STRING_H */