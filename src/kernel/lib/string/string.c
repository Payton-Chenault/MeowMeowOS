#include "string.h"


#include <stdbool.h>

size_t strlen(const char* str) {
    size_t len = 0;
    while (str[len]) len++;
    return len;
}

int strcmp(const char* str1, const char* str2) {
    while (*str1 && (*str1 == *str2)) {
        str1++;
        str2++;
    }
    return *(const unsigned char*)str1 - *(const unsigned char*)str2;
}

char* strtok(char* str, const char* delimiters) {
    static char* last_str= NULL;
    if (str != NULL) {
        last_str = str;
    }

    if (last_str == NULL || *last_str == '\0') {
        return NULL; 
    }

    char* token_start = last_str;
    while (*token_start != '\0') {
        bool is_delim = false;
        for(int i = 0; delimiters[i] != '\0'; i++) {
            if (*token_start == delimiters[i]) {
                is_delim = true;
                break;
            }
        }
        if (!is_delim) break;
        token_start++;
    }

    if (*token_start == '\0') {
        last_str = NULL;
        return NULL;
    }

    char* token_end = token_start;
    while (*token_end != '\0') {
        bool is_delim = false;
        for (int i = 0; delimiters[i] != '\0'; i++) {
            if (*token_end == delimiters[i]) {
                is_delim = true;
                break;
            }
        }
        if (is_delim) {
            *token_end = '\0';
            last_str = token_end + 1;
            return token_start;
        }
        token_end++;
    }

    last_str = token_end;
    return token_start;
}

char* strcpy(char* dest, const char* src) {
    char* saved = dest;
    while(*src) {
        *dest++ = *src++;
    }
    *dest = '\0';
    return saved;
}

void* memset(void* ptr, int value, size_t num) {
    unsigned char* p = (unsigned char*)ptr;
    for (size_t i = 0; i < num; i++) {
        p[i] = (unsigned char)value;
    }
    return ptr;
}

void* memcpy(void* dest, const void* src, size_t num) {
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    for (size_t i = 0; i < num; i++) {
        d[i] = s[i];
    }
    return dest;
}

int memcmp(const void* s1, const void* s2, size_t n) {
    const unsigned char* p1 = (const unsigned char*)s1;
    const unsigned char* p2 = (const unsigned char*)s2;

    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] - p2[i];
        }
    }
    return 0;
}