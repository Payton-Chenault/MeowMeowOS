#include "shell.h"
#include <stdbool.h>
#include <stdint.h>

#include "../../lib/integer_ascii_converters/atoi.h"
#include "../../lib/string/string.h"
#include "../../utils/logging/logger.h"
#include "../../arch/x86/task/task.h"
#include "../../kernel_services/kernel_services.h"
#include "../../utils/console_print/kconsole.h"
#include "../elf/elf.h"

#include "../../fs/fat_16/fat16.h"

#define MODULE "SHELL"
#define SHELL_MAX_ARGS 16



static const char SYSTEM_PROGRAM_PREFIX[] = "/system/bin/usr/commands/";
static const char ROOT_PROGRAM_PREFIX[] = "/";

static bool has_elf_suffix(const char *name) {
  size_t length = strlen(name);
  return length >= 4 && strcasecmp(name + length - 4, ".elf") == 0;
}

static void shell_help_visitor(fat16_dir_entry_t *entry);

void command_help(int argc, char **argv) {
  (void)argc;
  (void)argv;

  kprintf("MeowMeowOS Commands\n");
  kprintf("Built-ins: help, clear, cd\n\n");

  char old_cwd[256];
  strcpy(old_cwd, fat16_get_current_path());

  if (fat16_chdir("/system/bin/usr/commands") == 0) {
    kprintf("Installed commands:\n");
    fat16_list(shell_help_visitor);

    fat16_chdir("/");

    kprintf("\nRoot commands:\n");
    fat16_list(shell_help_visitor);

    fat16_chdir(old_cwd);
    return;
  }

  // Fallback: current directory or root
  fat16_chdir("/");
  fat16_list(shell_help_visitor);
  fat16_chdir(old_cwd);
}

void command_clear(int argc, char **argv) {
  (void)argc;
  (void)argv;
  kclear_screen();
}

void command_cd(int argc, char **argv) {
  if (argc < 2) {
    kprintf("Usage: cd <path>\n");
    return;
  }

  fat16_chdir(argv[1]);
}

void kshell_main(void) {
  log_debug(MODULE, "Entered kshell_main");
  __asm__ volatile("sti");
  char line[128];
  char *argv[SHELL_MAX_ARGS];

  kprintf("Welcome to MeowMeowOS!\n");

  while (1) {
    kprintf("[root@shell:%s]> ", fat16_get_current_path());
    kconsole_read_line(line, 128);

    if (line[0] == '\0')
      continue;

    int argc = 0;
    char *token = strtok(line, " ");
    while (token != NULL && argc < SHELL_MAX_ARGS - 1) {
      argv[argc++] = token;
      token = strtok(NULL, " ");
    }
    argv[argc] = NULL;

    if (argc == 0)
      continue;

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

    char program_path[64];
    if (!has_elf_suffix(argv[0])) {
      size_t len = strlen(argv[0]);
      if (len > sizeof(program_path) - 5)
        len = sizeof(program_path) - 5;
      memcpy(program_path, argv[0], len);
      program_path[len] = '.';
      program_path[len + 1] = 'e';
      program_path[len + 2] = 'l';
      program_path[len + 3] = 'f';
      program_path[len + 4] = '\0';
    } else {
      strncpy(program_path, argv[0], sizeof(program_path) - 1);
      program_path[sizeof(program_path) - 1] = '\0';
    }

    uint32_t pid = elf_load_and_spawn(program_path, argc, argv);

    if (pid == 0) {
      char system_program_path[128];
      size_t prefix_len = sizeof(SYSTEM_PROGRAM_PREFIX) - 1;
      size_t plen = strlen(program_path);

      if (prefix_len + plen >= sizeof(system_program_path)) {
        plen = sizeof(system_program_path) - prefix_len - 1;
      }

      memcpy(system_program_path, SYSTEM_PROGRAM_PREFIX, prefix_len);
      memcpy(system_program_path + prefix_len, program_path, plen);
      system_program_path[prefix_len + plen] = '\0';

      pid = elf_load_and_spawn(system_program_path, argc, argv);

      if (pid == 0) {
        char root_program_path[64];
        size_t root_prefix_len = sizeof(ROOT_PROGRAM_PREFIX) - 1;
        size_t root_name_len = strlen(program_path);

        if (root_prefix_len + root_name_len >= sizeof(root_program_path)) {
          root_name_len = sizeof(root_program_path) - root_prefix_len - 1;
        }

        memcpy(root_program_path, ROOT_PROGRAM_PREFIX, root_prefix_len);
        memcpy(root_program_path + root_prefix_len, program_path, root_name_len);
        root_program_path[root_prefix_len + root_name_len] = '\0';

        pid = elf_load_and_spawn(root_program_path, argc, argv);
      }
    }

    if (pid != 0) {
      log_debug(MODULE, "Program spawned, pid=%u", pid);
      task_wait(pid);
    } else {
      log_debug(MODULE, "Unknown command: %s", argv[0]);
      kprintf("Unknown command: %s\n", argv[0]);
    }
  }
}

static void shell_help_visitor(fat16_dir_entry_t *entry) {
    if (entry->attributes == 0x0F || entry->filename[0] == 0xE5) {
        return;
    }

    char name[13];
    int pos = 0;

    for (int i = 0; i < 8; i++) {
        char c = entry->filename[i];
        if (c == ' ')
            break;
        name[pos++] = c;
    }

    bool has_ext = false;
    for (int i = 8; i < 11; i++) {
        if (entry->filename[i] != ' ') {
            has_ext = true;
            break;
        }
    }

    if (has_ext) {
        name[pos++] = '.';
        for (int i = 8; i < 11; i++) {
            char c = entry->filename[i];
            if (c == ' ')
                break;
            name[pos++] = c;
        }
    }
    name[pos] = '\0';

    log_info("visitor: file='%s'", name);

    if (!has_elf_suffix(name)) {
        log_error(" (not ELF)", NULL);
        return;
    }
    log_info(" (ELF)", NULL);

    char full_path[256];
    const char *cwd = fat16_get_current_path();
    int cwd_len = strlen(cwd);

    if (cwd_len == 1 && cwd[0] == '/') {
        full_path[0] = '/';
        strcpy(full_path + 1, name);
    } else {
        strcpy(full_path, cwd);
        if (full_path[cwd_len - 1] != '/') {
            full_path[cwd_len] = '/';
            full_path[cwd_len + 1] = '\0';
        }
        strcat(full_path, name);
    }
    char desc[256];
    int ret = elf_get_description(full_path, desc, sizeof(desc));
    log_info("SHELL", "elf_get_description returned %d for '%s'", ret, full_path);
    if (ret) {
        kprintf("%s - %s\n", name, desc);
    } else {
        kprintf("%s (no description)\n", name);
    }
}