#ifndef STRING_H
#define STRING_H

#include <stddef.h>

size_t strlen(const char* str);
int strcmp(const char* s1, const char* s2);
char* strtok(char* str, const char* delimiters);
char* strcpy(char* dest, const char* src);

void* memset(void* ptr, int value, size_t num);
void* memcpy(void* dest, const void* src, size_t num);

#endif