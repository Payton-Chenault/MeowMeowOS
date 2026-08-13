#ifndef SHELL_H
#define SHELL_H

typedef void (*command_handler_t)(int argc, char** argv);

typedef struct {
    const char* name;
    const char* desc;
    command_handler_t handler;
} shell_cmd_t;

void kshell_main(void); 

#endif