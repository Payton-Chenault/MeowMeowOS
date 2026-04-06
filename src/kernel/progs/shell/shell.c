#include "shell.h"

void command_help(int argc, char** argv);
void command_echo(int argc, char** argv);
void command_clear(int argc, char** argv);

static shell_cmd_t builtin_commands[] = {
    {"help", "Shows this menu", command_help},
    {"echo", "Repeats text back to you", command_echo},
    {"clear", "Clears the terminal", command_clear},
    {NULL, NULL, NULL}
};

void command_help(int argc, char** argv) {
    kprintln("MeowMeowOS Shell - Available Commands:");
    for (int i = 0; builtin_commands[i].name != NULL; i++) {
        kprintf(" %s \t- %s\n", builtin_commands[i].name, builtin_commands[i].desc);
    }
}

void command_echo(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        kprintf("%s ", argv[i]);
    }
    kprint("\n");
}

void command_clear(int argc, char** argv) {
    kclear_screen();
}

void kshell_main(void) {
    char line[128];
    char* argv[16];

    while(1) {
        kprint("meow> ");
        keyboard_read_line(line, 128);
        kprint("\n");

        if (strlen(line) == 0) continue;

        int argc = 0;
        char* token = strtok(line, " ");
        while(token != NULL && argc < 16) {
            argv[argc++] = token;
            token = strtok(NULL, " ");
        }

        bool found = false;
        for (int i = 0; builtin_commands[i].name != NULL; i++) {
            if (strcmp(argv[0], builtin_commands[i].name) == 0) {
                builtin_commands[i].handler(argc, argv);
                found = true;
                break;
            }
        }

        if (!found) {
            kprintf("Command not recognized: %s\n", argv[0]);
        }
    }
}