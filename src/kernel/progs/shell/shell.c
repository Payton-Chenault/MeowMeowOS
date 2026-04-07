#include "shell.h"
#include <stdbool.h>
#include <stdint.h>

#include "../../lib/string/string.h"
#include "../../lib/integer_ascii_converters/atoi.h"

#include "../../arch/x86/pit/pit.h"

#include "../../kernel_services/kernel_services.h"
#include "../../utils/console_print/kconsole.h"
#include "../../fs/fat_16/fat16.h"
#include "../beep/beep.h"

#define MODULE "SHELL_PROG"
void command_help(int argc, char** argv);
void command_echo(int argc, char** argv);
void command_clear(int argc, char** argv);
void command_beep(int argc, char** argv);
void command_get_uptime(int argc, char** argv);
void command_test_drive_read(int argc, char** argv);
void command_format(int argc, char** argv);
void command_touch(int argc, char** argv);
void command_list(int argc, char** argv);


static shell_cmd_t builtin_commands[] = {
    {"help", "Shows this menu", command_help},
    {"echo", "Repeats <text> back to you", command_echo},
    {"clear", "Clears the terminal", command_clear},
    {"beep", "Beeps for a <frequency> and [duration_ms] given", command_beep},
    {"uptime", "Gets how long the system has been on (in seconds)", command_get_uptime},
    {"test-dskread", "Tests the ability of reading the disk (Reads the MBR)", command_test_drive_read},
    {"format-dskf16", "Formats the disk with FAT-16", command_format}, 
    {"touch", "Creates a new file caleld <name.*>. Can also provide a [size] in bytes", command_touch}, 
    {"ls", "Lists all files in current directory", command_list}, 
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
    kprintf("MeowMeowOS has been on for %d seconds\n", get_ticks() / 1000);
}

void command_test_drive_read(int argc, char** argv) {
    bool is_verbose = false;

    if (argc == 2 && strcmp(argv[2], "-v")) {
        is_verbose = true;
    }
    uint8_t* buffer = (uint8_t*)kmem_zalloc(512);
    kdisk_read_sector(0, buffer);

    if(is_verbose) {
        kprint("\nMBR Sector Dump: \n\n[");
        for (int i = 0; i < 512; i++) {
            kprintf("%x", buffer[i]);
        }
        kprint("]\n\n");
    }

    if (buffer[510] == 0x55 && buffer[511] == 0xAA) {
        kprintf("Test PASSED\n");
    } else {
        kprintf("Test FAILED, Read Signiture: %x %x\n", buffer[510], buffer[511]);
    }
}

void command_format(int argc, char** argv) {
    char line[8];
    kprintf("Are you sure you want to format? <y/N>: ");
    kconsole_read_line(line, 8);

    if(strlen(line) == 0) return;
    if(strcmp(line, "y") == 0) {
        kprintf("Formatting...");
        fat16_format_drive();
        kprintf("Drive Successfully Formatted\n");
    }
}

void command_touch(int argc, char** argv) {
    if (argc < 2) {
        kprintf("Usage: touch [file_name.*] -help for more info.\n");
        return;
    } 
    uint8_t* init_data = kmem_zalloc(0);

    fat16_write_file(argv[1], init_data, 0);

    kmem_free(init_data);
};

void kshell_ls_visitor(fat16_dir_entry_t* entry) {
    char name[9] = {0};
    char ext[4] = {0};
    memcpy(name, entry->filename, 8);
    memcpy(ext, entry->extension, 3);

    if(entry->attributes & 0x10) {
        kprintf("  [DIR]  %s.%s\n", name, ext);
    } else {
        kprintf("  %s.%s   %d bytes\n", name, ext, entry->file_size);
    }
}

void command_list(int argc, char** argv) {
    fat16_list(kshell_ls_visitor);
}


void kshell_main(void) {
    char line[128];
    char* argv[16];

    while(1) {
        kprintf("meow> ");
        kconsole_read_line(line, 128);

        if (strlen(line) == 0) continue;
        int argc = 0;
        char* token = strtok(line, " ");
        while(token != NULL && argc < 16) {
            argv[argc++] = token;
            token = strtok(NULL, " ");
        }

        if (argc == 0) {
            continue;
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
            kprintf("Command not recognized: [%s] (length: %d)\n", argv[0], strlen(argv[0]));
        }
    }
}