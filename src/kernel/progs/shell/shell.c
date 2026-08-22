#include "shell.h"
#include <stdbool.h>
#include <stdint.h>

#include "../../lib/integer_ascii_converters/atoi.h"
#include "../../lib/string/string.h"
#include "../../utils/logging/logger.h"


#include "../../arch/x86/task/task.h"
#include "../../kernel_services/kernel_services.h"
#include "../../utils/console_print/kconsole.h"

#include "../../fs/fat_16/fat16.h"

#include "../elf/elf.h"

#define MODULE "SHELL"
#define SHELL_MAX_ARGS 16

static const char SYSTEM_PROGRAM_PREFIX[] = "/system/bin/usr/commands/";
static const char ROOT_PROGRAM_PREFIX[] = "/";

static bool has_elf_suffix(const char *name) {
  size_t length = strlen(name);
  return length >= 4 && strcmp(name + length - 4, ".elf") == 0;
}

void command_help(int argc, char **argv) {
  (void)argc;
  (void)argv;

  kprintf("MeowMeowOS Shell Commands\n");
  kprintf("Built-ins: help, clear, cd\n\n");

  kprintf("File/Directory Commands:\n");
  kprintf("  ls [dir]           List directory contents\n");
  kprintf("  cat <file>         Print file contents\n");
  kprintf("  mkdir <dir>        Create directory\n");
  kprintf("  rm <file>          Remove file\n");
  kprintf("  rmdir <dir>        Remove empty directory\n");
  kprintf("  touch <file>       Create empty file\n");
  kprintf("  stat <file>        Show file metadata\n");
  kprintf("  head [-n n] <file> Print first lines\n");
  kprintf("  tail <file> [n]    Print last bytes\n");
  kprintf("  pwd                Print working directory\n\n");

  kprintf("System Utilities:\n");
  kprintf("  echo [text...]     Print text\n");
  kprintf("  uptime             Show system uptime\n");
  kprintf("  format             Format the disk\n");
  kprintf("  install            Install command files\n");
  kprintf("  taskst             Scheduler stress test\n");
  kprintf("  testdsk            Disk read/write test\n");
  kprintf("  testmem            Memory test\n");
  kprintf("  memtest2           Heap allocator test\n");
  kprintf("  redir              Test stdout redirection\n");
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

    // First try current working directory
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

      // Fresh images keep injected commands in the FAT16 root until install runs.
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