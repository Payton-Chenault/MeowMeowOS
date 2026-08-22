#ifndef STRING_H
#define STRING_H

#include <stddef.h>

size_t strlen(const char *str);
int strcmp(const char *s1, const char *s2);
int strcasecmp(const char *s1, const char *s2);
char *strtok(char *str, const char *delimiters);
char *strcpy(char *dest, const char *src);
char *strcat(char *dest, const char *src);
char *strrchr(const char *s, int c);
char *strstr(const char *haystack, const char *needle);
char *strncpy(char *dest, const char *src, size_t n);
int snprintf(char *str, size_t size, const char *format, ...);

void *memset(void *ptr, int value, size_t num);
void *memcpy(void *dest, const void *src, size_t num);
int memcmp(const void *s, const void *c, size_t n);

#endif