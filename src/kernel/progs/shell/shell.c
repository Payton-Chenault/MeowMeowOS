#include "shell.h"
#include <stdint.h>

#include "../../lib/string/string.h"
#include "../../lib/integer_ascii_converters/atoi.h"

#include "../../arch/x86/pit/pit.h"

#include "../../kernel_services/kernel_services.h"
#include "../../utils/console_print/kconsole.h"
#include "../beep/beep.h"

void command_help(int argc, char** argv);
void command_echo(int argc, char** argv);
void command_clear(int argc, char** argv);
void command_beep(int argc, char** argv);
void command_get_uptime(int argc, char** argv);

static shell_cmd_t builtin_commands[] = {
    {"help", "Shows this menu", command_help},
    {"echo", "Repeats <text> back to you", command_echo},
    {"clear", "Clears the terminal", command_clear},
    {"beep", "Beeps for a <frequency> and [duration_ms] given", command_beep},
    {"uptime", "Gets how long the system has been on (in seconds)", command_get_uptime},
    {NULL, NULL, NULL}
};

void command_help(int argc, char** argv) {
    kprintf("MeowMeowOS Shell - Available Commands:\n");
    for (int i = 0; builtin_commands[i].name != NULL; i++) {
        kprintf(" %s \t- %s\n", builtin_commands[i].name, builtin_commands[i].desc);
    }
}

void command_echo(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        kprintf("%s ", argv[i]);
    }
    kprintf("\n");
}

void command_clear(int argc, char** argv) {
    kclear_screen();
}

void command_beep(int argc, char** argv) {
    if (argc < 2) {
        kprintf("Usage: beep [frequency] <duration_ms>\n");
        return;
    } 

    int freq = atoi(argv[1]);

    int duration = (argc > 2) ? atoi(argv[2]) : 100;

    if (freq <= 0) {
        kprintf("Error: Frequency must be greater then 0\n");
        return;
    }

    kprintf("Playing %d Hz for %d ms...\n", freq, duration);
    beep(freq, duration);
}

void command_get_uptime(int argc, char** argv) {
    uint32_t total_seconds = get_ticks() / 1000;
    
    kprintf("MeowMeowOS has been on for %d seconds\n", get_ticks() / 1000);
}

void kshell_main(void) {
    char line[128];
    char* argv[16];

    while(1) {
        kprintf("meow> ");
        kconsole_read_line(line, 128);
        kprintf("\n");

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