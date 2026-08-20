#include "../libs/meow_libc.h"

#define COMMAND_COUNT 12

static const char *command_files[COMMAND_COUNT] = {
    "cat.elf",     "echo.elf",    "format.elf", "ls.elf",
    "mkdir.elf",   "rm.elf",      "rmdir.elf",  "taskst.elf",
    "testdsk.elf", "testmem.elf", "touch.elf",  "uptime.elf"};

static int copy_file(const char *src_path, const char *dst_path) {
  return sys_copy_file(src_path, dst_path);
}

int _start(int argc, char **argv) {
  (void)argc;
  (void)argv;

  sys_print("MeowMeowOS Installation\n");
  sys_print("Creating /system/bin/usr/commands...\n");

  // Use absolute paths throughout. The FAT16 helper functions are fragile under
  // relative-path changes, and this keeps the installation flow deterministic.
  sys_mkdir("/system");
  sys_chdir("/system");
  sys_mkdir("/system/bin");
  sys_chdir("/system/bin");
  sys_mkdir("/system/bin/usr");
  sys_chdir("/system/bin/usr");
  sys_mkdir("/system/bin/usr/commands");
  sys_chdir("/system/bin/usr/commands");

  sys_print("Copying command files...\n");
  for (int i = 0; i < COMMAND_COUNT; i++) {
    char src[64];
    char dst[128];
    snprintf(src, sizeof(src), "/%s", command_files[i]);
    snprintf(dst, sizeof(dst), "/system/bin/usr/commands/%s", command_files[i]);
    int copied = copy_file(src, dst);
    if (copied != 0) {
      char msg[128];
      snprintf(msg, sizeof(msg), "copy failed: %s -> %s\n", src, dst);
      sys_print(msg);
    }
  }

  sys_print("Verifying...\n");
  sys_chdir("/system/bin/usr/commands");
  int ok = 1;
  for (int i = 0; i < COMMAND_COUNT; i++) {
    int fd = sys_open(command_files[i]);
    if (fd < 0) {
      sys_print("Missing: ");
      sys_print(command_files[i]);
      sys_print("\n");
      ok = 0;
    } else {
      sys_close(fd);
    }
  }

  if (ok) {
    sys_print("All files verified successfully.\n");
    sys_chdir("/");
    for (int i = 0; i < COMMAND_COUNT; i++) {
      sys_remove(command_files[i]);
    }
    sys_remove("install.elf");
    sys_print("Installation complete. Command files moved to "
              "/system/bin/usr/commands.\n");
  } else {
    sys_print("Installation incomplete. Original files not removed.\n");
  }

  sys_exit();
  return 0;
}