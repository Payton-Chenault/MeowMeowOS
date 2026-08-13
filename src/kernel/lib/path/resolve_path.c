#include "../string/string.h"

void resolve_path(const char* cwd, const char* input, char* output) {
    char temp[512]; 
    
    if (input[0] == '/') {
        size_t len = strlen(input);
        if (len >= sizeof(temp)) len = sizeof(temp) - 1;
        memcpy(temp, input, len);
        temp[len] = '\0';
    } else {
        size_t cwd_len = strlen(cwd);
        if (cwd_len >= sizeof(temp)) cwd_len = sizeof(temp) - 1;
        memcpy(temp, cwd, cwd_len);
        temp[cwd_len] = '\0';
        
        if (temp[strlen(temp) - 1] != '/') {
            if (strlen(temp) + 1 < sizeof(temp)) {
                strcat(temp, "/");
            }
        }
        
        if (strlen(temp) + strlen(input) < sizeof(temp)) {
            strcat(temp, input);
        }
    }
    
    char* tokens[64];
    int token_count = 0;
    
    char* token = strtok(temp, "/");
    while (token != NULL && token_count < 64) {
        if (strcmp(token, ".") == 0) {
        } else if (strcmp(token, "..") == 0) {
            if (token_count > 0) token_count--; 
        } else {
            tokens[token_count++] = token; 
        }
        token = strtok(NULL, "/");
    }
    
    output[0] = '\0';
    if (token_count == 0) {
        strcpy(output, "/");
        return;
    }
    
    for (int i = 0; i < token_count; i++) {
        if (strlen(output) + strlen(tokens[i]) + 2 < 256) {
            strcat(output, "/");
            strcat(output, tokens[i]);
        }
    }
}