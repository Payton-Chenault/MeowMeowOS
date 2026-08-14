#include "shell.h"
#include <stdbool.h>
#include <stdint.h>

#include "../../lib/string/string.h"
#include "../../lib/integer_ascii_converters/atoi.h"
#include "../../utils/logging/logger.h"


#include "../../arch/x86/task/task.h"
#include "../../kernel_services/kernel_services.h"
#include "../../utils/console_print/kconsole.h"

#include "../../fs/fat_16/fat16.h"

#include "../elf/elf.h"

#define MODULE "SHELL"


void command_help(int argc, char** argv) {
    (void)argc; (void)argv;
    kprintf("MeowMeowOS Shell Commands - External ELF Command Operated\n");
    kprintf("Type 'ls' to view available bins in the current directory.\n");
    kprintf("Built-ins: cd, clear, help\n");
}

void command_clear(int argc, char** argv) {
    (void)argc; (void)argv;
    kclear_screen();
}

void command_cd(int argc, char** argv) {
    if (argc < 2) {
        kprintf("Usage: cd <path>\n");
        return;
    }

    fat16_chdir(argv[1]);
}

void kshell_main(void) {
    log_debug(MODULE, "Entered kshell_main");
    __asm__ volatile ("sti");
    char line[128];
    char* argv[16];

    kprintf("Welcome to MeowMeowOS!\n");

    while (1) {
        kprintf("[root@shell:%s]> ", fat16_get_current_path());
        kconsole_read_line(line, 128);

        if (strlen(line) == 0) continue;


        int argc = 0;
        char* token = strtok(line, " ");
        while (token != NULL && argc < 16) {
            argv[argc++] = token;
            token = strtok(NULL, " ");
        }

        if (argc == 0) continue;

        if (strcmp(argv[0], "help") == 0) {
            command_help(argc, argv);
            continue;
        } else if (strcmp(argv[0], "clear") == 0) {
            command_clear(argc, argv);
            continue;
        } else if (strcmp(argv[0], "cd") == 0) {
            command_cd(argc, argv);
            continue;
        }

        // In shell.c, inside command execution:
        char program_path[32];
        if (strstr(argv[0], ".elf") == NULL) {
            snprintf(program_path, sizeof(program_path), "%s.elf", argv[0]);
        } else {
            strncpy(program_path, argv[0], sizeof(program_path));
            program_path[sizeof(program_path) - 1] = '\0';
        }

        // Build argument list to pass to the program:
        // argv[0] is the command name without .elf, followed by any extra args.
        char* cmd_argv[16];
        cmd_argv[0] = argv[0];
        for (int i = 1; i < argc; i++) {
        cmd_argv[i] = argv[i];
        }

        uint32_t pid = elf_load_and_spawn(program_path, argc, cmd_argv);

        if (pid != 0) {
            log_debug(MODULE, "Program spawned, pid=%u", pid);
            task_wait(pid);
        } else {
            log_debug(MODULE, "Unknown command: %s", argv[0]);
            kprintf("Unknown command: %s\n", argv[0]);
        }
    }
}