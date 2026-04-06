#include "string.h"


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