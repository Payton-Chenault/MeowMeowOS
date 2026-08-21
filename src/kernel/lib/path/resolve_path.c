#include "resolve_path.h"
#include "../string/string.h"

bool resolve_path(const char *cwd, const char *input, char *output,
                  size_t output_size) {
    if (cwd == NULL || input == NULL || output == NULL || output_size == 0)
        return false;

    char temp[512];
    size_t len;

    if (input[0] == '\0')
        return false;

    if (input[0] == '/') {
        len = strlen(input);
        if (len >= sizeof(temp))
            return false;
        memcpy(temp, input, len + 1);
    } else {
        len = strlen(cwd);
        if (len >= sizeof(temp))
            return false;
        memcpy(temp, cwd, len);
        temp[len] = '\0';

        if (len > 0 && temp[len - 1] != '/') {
            if (len + 1 >= sizeof(temp))
                return false;
            temp[len++] = '/';
            temp[len] = '\0';
        }

        size_t input_len = strlen(input);
        if (len + input_len >= sizeof(temp))
            return false;
        memcpy(temp + len, input, input_len + 1);
    }

    char *tokens[128];
    int token_count = 0;

    char *token = strtok(temp, "/");
    while (token != NULL && token_count < 128) {
        if (strcmp(token, ".") != 0) {
            if (strcmp(token, "..") == 0) {
                if (token_count > 0) {
                    token_count--;
                } else {
                    return false;
                }
            } else {
                tokens[token_count++] = token;
            }
        }
        token = strtok(NULL, "/");
    }

    if (token_count == 0) {
        if (output_size < 2)
            return false;
        memcpy(output, "/", 2);
        return true;
    }

    output[0] = '\0';
    size_t used = 0;

    for (int i = 0; i < token_count; i++) {
        size_t tok_len = strlen(tokens[i]);
        if (used + tok_len + 2 > output_size)
            return false;

        memcpy(output + used, "/", 1);
        used += 1;
        memcpy(output + used, tokens[i], tok_len);
        used += tok_len;
        output[used] = '\0';
    }

    return true;
}