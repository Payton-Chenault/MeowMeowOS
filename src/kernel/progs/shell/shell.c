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

#define MODULE "SHELL"

void command_help(int argc, char** argv);
void command_echo(int argc, char** argv);
void command_clear(int argc, char** argv);
void command_beep(int argc, char** argv);
void command_uptime(int argc, char** argv);
void command_dsktest(int argc, char** argv);
void command_format(int argc, char** argv);
void command_touch(int argc, char** argv);
void command_ls(int argc, char** argv);
void command_cat(int argc, char** argv);
void command_run(int argc, char** argv);
void command_mkdir(int argc, char** argv);

extern bool elf_load_file(const char* filename);

static shell_cmd_t builtin_commands[] = {
    {"help",    "Shows this menu", command_help},
    {"echo",    "Repeats <text> back to you", command_echo},
    {"clear",   "Clears the terminal", command_clear},
    {"beep",    "Beep: <freq> [duration_ms]", command_beep},
    {"uptime",  "System uptime in seconds", command_uptime},
    {"testdsk", "Verify MBR signature", command_dsktest},
    {"format",  "Format drive to FAT-16", command_format}, 
    {"touch",   "Create an empty file: <name>", command_touch}, 
    {"ls",      "List files in directory", command_ls}, 
    {"cat",     "Display file contents: <name>", command_cat},
    {"run",     "Execute ELF file: <name>", command_run},
    {"mkdir",   "Create an empty directory: <name>", command_mkdir},
    {NULL,      NULL, NULL}
};

void command_help(int argc, char** argv) {
    (void)argc; (void)argv;
    kprintf("MeowMeowOS Shell - Available Commands:\n");
    for (int i = 0; builtin_commands[i].name != NULL; i++) {
        kprintf("  %-10s - %s\n", builtin_commands[i].name, builtin_commands[i].desc);
    }
}

void command_echo(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        kprintf("%s%s", argv[i], (i == argc - 1) ? "" : " ");
    }
    kprintf("\n");
}

void command_clear(int argc, char** argv) {
    (void)argc; (void)argv;
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
        kprintf("Error: Frequency must be > 0\n");
        return;
    }

    beep(freq, duration);
}

void command_uptime(int argc, char** argv) {
    (void)argc; (void)argv;
    kprintf("Uptime: %d seconds\n", get_ticks() / 1000);
}

void command_dsktest(int argc, char** argv) {
    bool verbose = (argc > 1 && strcmp(argv[1], "-v") == 0);

    uint8_t* buffer = (uint8_t*)kmem_zalloc(512);
    if (!buffer) return;

    kdisk_read_sector(0, buffer);

    if (verbose) {
        kprintf("MBR Hexdump: ");
        for (int i = 0; i < 16; i++) kprintf("%x ", buffer[i]);
        kprintf("...\n");
    }

    if (buffer[510] == 0x55 && buffer[511] == 0xAA) {
        kprintf("Disk Read Test: PASSED\n");
    } else {
        kprintf("Disk Read Test: FAILED (Sig: %x%x)\n", buffer[510], buffer[511]);
    }
    kmem_free(buffer);
}

void format_progress_callback(uint32_t current, uint32_t total) {
    static uint32_t last_percentage = 101; // Force first print
    uint32_t percentage = (current * 100) / total;

    // Only update the screen if the percentage has actually increased
    if (percentage == last_percentage) return;
    last_percentage = percentage;

    const int BAR_LENGTH = 30;
    uint32_t fill = (current * BAR_LENGTH) / total;

    kprintf("\rFormatting: [");
    for (int i = 0; i < BAR_LENGTH; i++) {
        if (i < (int)fill) kprintf("#");
        else kprintf("-");
    }
    kprintf("] %d%%", percentage);
}

void command_format(int argc, char** argv) {
    char confirm[8];
    kprintf("Are you sure? This will destroy all data! <y/N>: ");
    kconsole_read_line(confirm, 8);

    if (confirm[0] == 'y' || confirm[0] == 'Y') {
        // Pass the callback to the driver
        fat16_format_drive(format_progress_callback);
        kprintf("\nDrive formatted successfully.\n");
    }
}

void command_touch(int argc, char** argv) {
    if (argc < 2) {
        kprintf("Usage: touch <filename>\n");
        return;
    } 
    // Create empty file (one space as content)
    fat16_write_file(argv[1], (uint8_t*)" ", 1);
}

void kshell_ls_visitor(fat16_dir_entry_t* entry) {
    char name[9] = {0};
    char ext[4] = {0};
    
    int n_idx = 0;
    for (int i = 0; i < 8; i++) {
        if (entry->filename[i] != ' ') {
            name[n_idx++] = entry->filename[i];
        }
    }
    
    int e_idx = 0;
    for (int i = 0; i < 3; i++) {
        if (entry->extension[i] != ' ') {
            ext[e_idx++] = entry->extension[i];
        }
    }

    if (entry->attributes == 0x10) {
        kprintf("[DIR]       %s/\n", name);
    } else {
        if (e_idx > 0) {
            kprintf("%8d bytes    %s.%s\n", entry->file_size, name, ext);
        } else {
            kprintf("%d bytes    %s\n", entry->file_size, name);
        }
    }
}

void command_ls(int argc, char** argv) {
    (void)argc; (void)argv;
    fat16_list(kshell_ls_visitor);
}

void command_cat(int argc, char** argv) {
    if (argc < 2) return;
    
    uint32_t size = fat16_get_file_size(argv[1]);
    if (size == 0) {
        kprintf("File empty or not found.\n");
        return;
    }

    uint8_t* buffer = kmem_zalloc(size + 1);
    if (!buffer) return;

    fat16_read_file(argv[1], buffer);
    kprintf("%s\n", buffer);
    kmem_free(buffer);
}

void command_run(int argc, char** argv) {
    if (argc < 2) {
        kprintf("Usage: run <program.elf>\n");
        return;
    }
    
    if (!elf_load_file(argv[1])) {
        kprintf("Execution failed.\n");
    }
}

void command_mkdir(int argc, char** argv) {
    if (argc < 2) {
        kprintf("Usage: mkdir <name>\n");
        return;
    }

    if(strlen(argv[1]) > 8) {
        kprintf("Error: Directory name cannot exceed 8 characters\n");
        return;
    }

    fat16_create_dir(argv[1]);
}

void kshell_main(void) {
    char line[128];
    char* argv[16];

    kprintf("Welcome to MeowMeowOS!\n");

    while (1) {
        kprintf("meow /> ");
        kconsole_read_line(line, 128);

        if (strlen(line) == 0) continue;

        // Tokenize input
        int argc = 0;
        char* token = strtok(line, " ");
        while (token != NULL && argc < 16) {
            argv[argc++] = token;
            token = strtok(NULL, " ");
        }

        if (argc == 0) continue;

        // Command lookup
        bool found = false;
        for (int i = 0; builtin_commands[i].name != NULL; i++) {
            if (strcmp(argv[0], builtin_commands[i].name) == 0) {
                builtin_commands[i].handler(argc, argv);
                found = true;
                break;
            }
        }

        if (!found) {
            kprintf("Unknown command: %s\n", argv[0]);
        }
    }
}