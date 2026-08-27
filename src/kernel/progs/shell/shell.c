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
#include "../../fs/fat_16/fat16_vfs.h"
#include "../../fs/vfs/vfs.h"
#include "../../fs/pipe/pipe.h"
#include "../../lib/path/resolve_path.h"
#include "../../drivers/keyboard/keyboard.h"

#define MODULE "SHELL"
#define SHELL_MAX_ARGS 32
static const char SYSTEM_PROGRAM_PREFIX[] = "/system/bin/usr/commands/";
static const char ROOT_PROGRAM_PREFIX[] = "/";

/* --- Command History Ring Buffer --- */
#define HISTORY_SIZE 50
static char cmd_history[HISTORY_SIZE][128];
static int history_start = 0;
static int history_end = 0;
static int history_count = 0;

static void history_add(const char *cmd) {
    if (cmd[0] == '\0') return;
    int last_idx = (history_end == 0) ? HISTORY_SIZE - 1 : history_end - 1;
    if (history_count > 0 && strcmp(cmd_history[last_idx], cmd) == 0) return;

    strncpy(cmd_history[history_end], cmd, 127);
    cmd_history[history_end][127] = '\0';
    history_end = (history_end + 1) % HISTORY_SIZE;
    if (history_count < HISTORY_SIZE) history_count++;
    else history_start = (history_start + 1) % HISTORY_SIZE;
}

/* --- Tab Completion Variables --- */
static char comp_prefix[128];
static int comp_prefix_len;
static char comp_match[128];
static int comp_match_count;

static void tab_visitor(fat16_dir_entry_t *entry, const char *lfn_name) {
    (void)entry;
    if (strncmp(lfn_name, comp_prefix, comp_prefix_len) == 0) {
        if (comp_match_count == 0) {
            strncpy(comp_match, lfn_name, 127);
            comp_match[127] = '\0';
        } else {
            int i = comp_prefix_len;
            while (comp_match[i] && lfn_name[i] && comp_match[i] == lfn_name[i]) i++;
            comp_match[i] = '\0';
        }
        comp_match_count++;
    }
}

/* --- Advanced Line Reader --- */
static size_t shell_read_line(char *buffer, size_t max_size) {
    size_t cursor_pos = 0;
    int history_offset = 0;
    char typing_buf[128] = {0};
    buffer[0] = '\0';

    while (1) {
        uint16_t key = keyboard_read_keycode();
        if (key == 0) {
            task_sleep(1);
            continue;
        }

        uint8_t mods = keyboard_get_modifiers();
        bool ctrl_held = (mods & MODIFIER_CTRL) != 0;

        /* Backwards compatibility: Support CTRL+P, CTRL+N, CTRL+T alongside native keys */
        if (ctrl_held) {
            if (key == 'p' || key == 'P') {
                key = KEY_UP;
            } else if (key == 'n' || key == 'N') {
                key = KEY_DOWN;
            } else if (key == 't' || key == 'T' || key == 'i' || key == 'I') {
                key = KEY_TAB;
            }
        }

        if (key == KEY_ENTER || key == '\n' || key == '\r') {
            kput_char('\n');
            buffer[cursor_pos] = '\0';
            history_add(buffer);
            return cursor_pos;
        } else if (key == KEY_BACKSPACE || key == '\b' || key == 0x7F) {
            if (cursor_pos > 0) {
                cursor_pos--;
                buffer[cursor_pos] = '\0';
                kbackspace();
            }
        } else if (key == KEY_UP) {
            if (history_offset < history_count) {
                if (history_offset == 0) {
                    buffer[cursor_pos] = '\0';
                    strncpy(typing_buf, buffer, 127);
                }
                history_offset++;
                int idx = (history_end - history_offset + HISTORY_SIZE) % HISTORY_SIZE;

                for (size_t i = 0; i < cursor_pos; i++) kbackspace();
                strncpy(buffer, cmd_history[idx], max_size - 1);
                buffer[max_size - 1] = '\0';
                cursor_pos = strlen(buffer);
                kprint(buffer);
            }
        } else if (key == KEY_DOWN) {
            if (history_offset > 0) {
                history_offset--;
                for (size_t i = 0; i < cursor_pos; i++) kbackspace();

                if (history_offset == 0) {
                    strncpy(buffer, typing_buf, max_size - 1);
                } else {
                    int idx = (history_end - history_offset + HISTORY_SIZE) % HISTORY_SIZE;
                    strncpy(buffer, cmd_history[idx], max_size - 1);
                }
                buffer[max_size - 1] = '\0';
                cursor_pos = strlen(buffer);
                kprint(buffer);
            }
        } else if (key == KEY_TAB) {
            int word_start = cursor_pos;
            while (word_start > 0 && buffer[word_start - 1] != ' ') word_start--;

            int len = cursor_pos - word_start;
            if (len < 128) {
                strncpy(comp_prefix, buffer + word_start, len);
                comp_prefix[len] = '\0';
                comp_prefix_len = len;

                if (comp_prefix_len > 0) {
                    comp_match_count = 0;
                    comp_match[0] = '\0';

                    char old_cwd[256];
                    strncpy(old_cwd, fat16_get_current_path(), sizeof(old_cwd) - 1);
                    old_cwd[sizeof(old_cwd) - 1] = '\0';

                    if (word_start == 0) {
                        if (fat16_chdir("/system/bin/usr/commands") == 0) fat16_list(tab_visitor);
                        fat16_chdir(old_cwd);
                        fat16_list(tab_visitor);
                    } else {
                        char dir_path[256];
                        char file_prefix[128];
                        char *last_slash = strrchr(comp_prefix, '/');
                        if (last_slash) {
                            int dir_len = last_slash - comp_prefix;
                            if (dir_len == 0) {
                                strcpy(dir_path, "/");
                            } else {
                                strncpy(dir_path, comp_prefix, dir_len);
                                dir_path[dir_len] = '\0';
                            }
                            strcpy(file_prefix, last_slash + 1);

                            strcpy(comp_prefix, file_prefix);
                            comp_prefix_len = strlen(comp_prefix);

                            if (fat16_chdir(dir_path) == 0) fat16_list(tab_visitor);
                            fat16_chdir(old_cwd);
                        } else {
                            fat16_list(tab_visitor);
                        }
                    }

                    if (comp_match_count > 0) {
                        int match_len = strlen(comp_match);
                        for (int i = comp_prefix_len; i < match_len && cursor_pos < max_size - 1; i++) {
                            buffer[cursor_pos++] = comp_match[i];
                            kput_char(comp_match[i]);
                        }
                        buffer[cursor_pos] = '\0';
                    }
                }
            }
        } else if (key < 0x80 && !ctrl_held) { // Standard ASCII
            char c = (char)key;
            if (c >= 0x20 && c <= 0x7E && cursor_pos < max_size - 1) {
                buffer[cursor_pos++] = c;
                kput_char(c);
            }
        }
    }
}

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

void command_help(int argc, char **argv);
void command_clear(int argc, char **argv);
void command_cd(int argc, char **argv);

static uint32_t execute_command_block(int argc, char **argv, vfs_node_t *pipe_in, vfs_node_t *pipe_out) {
    vfs_node_t *in_node = pipe_in;
    vfs_node_t *out_node = pipe_out;
    bool append_out = false;
    bool should_close_in = false;
    bool should_close_out = false;

    int new_argc = 0;
    char *new_argv[SHELL_MAX_ARGS];

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "<") == 0 && i + 1 < argc) {
            char path[256];
            resolve_path(fat16_get_current_path(), argv[i + 1], path, sizeof(path));
            if (should_close_in) vfs_close(in_node);
            in_node = fat16_vfs_open(path);
            if (!in_node) {
                kprintf("shell: no such file: %s\n", argv[i + 1]);
                return 0;
            }
            should_close_in = true;
            i++;
        } else if (strcmp(argv[i], ">") == 0 && i + 1 < argc) {
            char path[256];
            resolve_path(fat16_get_current_path(), argv[i + 1], path, sizeof(path));
            fat16_delete_file(path);
            fat16_write_file(path, (uint8_t *)" ", 1);
            if (should_close_out) vfs_close(out_node);
            out_node = fat16_vfs_open(path);
            if (!out_node) {
                kprintf("shell: cannot create file: %s\n", argv[i + 1]);
                return 0;
            }
            append_out = false;
            should_close_out = true;
            i++;
        } else if (strcmp(argv[i], ">>") == 0 && i + 1 < argc) {
            char path[256];
            resolve_path(fat16_get_current_path(), argv[i + 1], path, sizeof(path));
            if (should_close_out) vfs_close(out_node);
            out_node = fat16_vfs_open(path);
            if (!out_node) {
                fat16_write_file(path, (uint8_t *)" ", 1);
                out_node = fat16_vfs_open(path);
            }
            if (!out_node) {
                kprintf("shell: cannot create file: %s\n", argv[i + 1]);
                return 0;
            }
            append_out = true;
            should_close_out = true;
            i++;
        } else {
            if (new_argc < SHELL_MAX_ARGS - 1) {
                new_argv[new_argc++] = argv[i];
            }
        }
    }
    new_argv[new_argc] = NULL;

    if (new_argc == 0) return 0;

    // Execute built-ins without tracking PIDs
    if (strcmp(new_argv[0], "help") == 0) {
        command_help(new_argc, new_argv);
        if (should_close_in) vfs_close(in_node);
        if (should_close_out) vfs_close(out_node);
        return 0;
    } else if (strcmp(new_argv[0], "clear") == 0) {
        command_clear(new_argc, new_argv);
        if (should_close_in) vfs_close(in_node);
        if (should_close_out) vfs_close(out_node);
        return 0;
    } else if (strcmp(new_argv[0], "cd") == 0) {
        command_cd(new_argc, new_argv);
        if (should_close_in) vfs_close(in_node);
        if (should_close_out) vfs_close(out_node);
        return 0;
    }

    uint32_t pid = shell_spawn_command(new_argc, new_argv, in_node, out_node);

    if (pid != 0) {
        task_t *t = task_get_by_pid(pid);
        if (t && out_node) {
            if (append_out) {
                t->fd_table[1].current_offset = out_node->length;
            } else if (should_close_out) {
                t->fd_table[1].current_offset = 0;
            }
        }
    } else {
        kprintf("shell: command not found: %s\n", new_argv[0]);
    }

    if (should_close_in) vfs_close(in_node);
    if (should_close_out) vfs_close(out_node);

    return pid;
}

static void shell_execute_pipeline(int argc1, char **argv1, int argc2, char **argv2) {
  vfs_node_t *read_node = NULL;
  vfs_node_t *write_node = NULL;
  
  if (pipe_create(&read_node, &write_node) < 0) {
    kprintf("shell: failed to create pipe\n");
    return;
  }

  uint32_t pid1 = execute_command_block(argc1, argv1, NULL, write_node);
  if (pid1 == 0) {
    vfs_close(read_node);
    vfs_close(write_node);
    return;
  }

  uint32_t pid2 = execute_command_block(argc2, argv2, read_node, NULL);
  if (pid2 == 0) {
    vfs_close(read_node);
    vfs_close(write_node);
    task_wait(pid1);
    return;
  }

  vfs_release(read_node);
  vfs_release(write_node);

  log_debug(MODULE, "Pipeline running: pid1=%u | pid2=%u", pid1, pid2);

  task_wait(pid1);
  task_wait(pid2);
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
  (void)argc;
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

  kprintf("Welcome to MeowMeowOS!\n\n");

  while (1) {
    const char *cwd = fat16_get_current_path();
    if (cwd == NULL) {
      cwd = "/";
    }

    kprintf("[root@shell: %s]> ", cwd);

    shell_read_line(line, 128);

    if (line[0] == '\0')
      continue;

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

    uint32_t pid = execute_command_block(argc, argv, NULL, NULL);

    if (pid != 0) {
      log_debug(MODULE, "Program spawned, pid=%u", pid);
      task_wait(pid);
    }
  }
}