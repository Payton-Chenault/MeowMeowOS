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
#include "../../fs/vfs/vfs.h"
#include "../../fs/pipe/pipe.h"

#define MODULE "SHELL"
#define SHELL_MAX_ARGS 32

static const char SYSTEM_PROGRAM_PREFIX[] = "/system/bin/usr/commands/";
static const char ROOT_PROGRAM_PREFIX[] = "/";

static bool has_elf_suffix(const char *name) {
  size_t length = strlen(name);
  return length >= 4 && strcasecmp(name + length - 4, ".elf") == 0;
}

static uint32_t shell_spawn_command(int argc, char **argv, vfs_node_t *in_node, vfs_node_t *out_node) {
  if (argc == 0 || argv[0] == NULL) {
    return 0;
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

  uint32_t pid = elf_load_and_spawn(program_path, argc, argv, in_node, out_node);

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

    pid = elf_load_and_spawn(system_program_path, argc, argv, in_node, out_node);

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

      pid = elf_load_and_spawn(root_program_path, argc, argv, in_node, out_node);
    }
  }

  return pid;
}

static void shell_execute_pipeline(int argc1, char **argv1, int argc2, char **argv2) {
  vfs_node_t *read_node = NULL;
  vfs_node_t *write_node = NULL;

  if (pipe_create(&read_node, &write_node) < 0) {
    kprintf("shell: failed to create pipe\n");
    return;
  }

  // Spawn command 1 with stdout mapped directly to pipe write end
  uint32_t pid1 = shell_spawn_command(argc1, argv1, NULL, write_node);
  if (pid1 == 0) {
    kprintf("shell: command not found: %s\n", argv1[0]);
    vfs_close(read_node);
    vfs_close(write_node);
    return;
  }

  // Spawn command 2 with stdin mapped directly to pipe read end
  uint32_t pid2 = shell_spawn_command(argc2, argv2, read_node, NULL);
  if (pid2 == 0) {
    kprintf("shell: command not found: %s\n", argv2[0]);
    vfs_close(read_node);
    vfs_close(write_node);
    task_wait(pid1);
    return;
  }

  // Release shell's local descriptor leases so EOF triggers when pid1 exits
  vfs_release(read_node);
  vfs_release(write_node);

  log_debug(MODULE, "Pipeline running: pid1=%u | pid2=%u", pid1, pid2);

  // Wait for both pipeline tasks to finish
  task_wait(pid1);
  task_wait(pid2);
}

static void shell_help_visitor(fat16_dir_entry_t *entry, const char *file_name);

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
    const char *cwd = fat16_get_current_path();
    if (cwd == NULL) {
      cwd = "/";
    }

    kprintf("[root@shell: %s]> ", cwd);
    kconsole_read_line(line, 128);

    if (line[0] == '\0')
      continue;

    // Argument Parser
    int argc = 0;
    char *p = line;

    size_t line_len = strlen(line);
    while (line_len > 0 && (line[line_len - 1] == '\n' || line[line_len - 1] == '\r')) {
      line[line_len - 1] = '\0';
      line_len--;
    }

    while (*p != '\0' && argc < SHELL_MAX_ARGS - 1) {
      while (*p == ' ' || *p == '\t') {
        p++;
      }
      if (*p == '\0')
        break;

      argv[argc++] = p;

      while (*p != '\0' && *p != ' ' && *p != '\t') {
        p++;
      }

      if (*p != '\0') {
        *p = '\0';
        p++;
      }
    }
    argv[argc] = NULL;

    if (argc == 0)
      continue;

    // Check for Pipeline '|' delimiter
    int pipe_index = -1;
    for (int i = 0; i < argc; i++) {
      if (strcmp(argv[i], "|") == 0) {
        pipe_index = i;
        break;
      }
    }

    if (pipe_index != -1) {
      if (pipe_index == 0 || pipe_index == argc - 1) {
        kprintf("shell: syntax error near unexpected token '|'\n");
        continue;
      }

      argv[pipe_index] = NULL;
      int argc1 = pipe_index;
      char **argv1 = &argv[0];
      int argc2 = argc - pipe_index - 1;
      char **argv2 = &argv[pipe_index + 1];

      shell_execute_pipeline(argc1, argv1, argc2, argv2);
      continue;
    }

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

    uint32_t pid = shell_spawn_command(argc, argv, NULL, NULL);

    if (pid != 0) {
      log_debug(MODULE, "Program spawned, pid=%u", pid);
      task_wait(pid);
    } else {
      log_debug(MODULE, "Unknown command: %s", argv[0]);
      kprintf("Unknown command: %s\n", argv[0]);
    }
  }
}

static void shell_help_visitor(fat16_dir_entry_t *entry, const char *file_name) {
  if (entry->attributes == 0x0F || (uint8_t)entry->filename[0] == 0xE5) {
    return;
  }

  if (!has_elf_suffix(file_name)) {
    return;
  }

  char full_path[256];
  const char *cwd = fat16_get_current_path();
  int cwd_len = strlen(cwd);

  if (cwd_len == 1 && cwd[0] == '/') {
    full_path[0] = '/';
    strcpy(full_path + 1, file_name);
  } else {
    strcpy(full_path, cwd);
    if (full_path[cwd_len - 1] != '/') {
      full_path[cwd_len] = '/';
      full_path[cwd_len + 1] = '\0';
    }
    strcat(full_path, file_name);
  }

  char desc[256];
  int ret = elf_get_description(full_path, desc, sizeof(desc));
  if (ret) {
    kprintf("  %s - %s\n", file_name, desc);
  } else {
    kprintf("  %s (no description)\n", file_name);
  }
}