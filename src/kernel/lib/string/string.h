#ifndef STRING_H
#define STRING_H

#include <stddef.h>

size_t strlen(const char* str);
int strcmp(const char* s1, const char* s2);
char* strtok(char* str, const char* delimiters);
char* strcpy(char* dest, const char* src);

#endif