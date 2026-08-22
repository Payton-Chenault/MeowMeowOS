#include "../libs/meow_libc.h"

#define COMMAND_COUNT 17

DESCRIPTION("install.elf: Install command files");

static const char *command_files[COMMAND_COUNT] = {
    "cat.elf",     "echo.elf",    "format.elf", "ls.elf",
    "mkdir.elf",   "rm.elf",      "rmdir.elf",  "taskst.elf",
    "testdsk.elf", "testmem.elf", "touch.elf",  "uptime.elf",
    "pwd.elf",     "stat.elf",    "head.elf",   "tail.elf",
    "redir.elf"};

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    printf("MeowMeowOS Installation\n");
    printf("Creating /system/bin/usr/commands...\n");

    if (mkdir("/system") != 0 || chdir("/system") != 0) {
        perror("install");
        return 1;
    }

    if (mkdir("/system/bin") != 0 || chdir("/system/bin") != 0) {
        perror("install");
        return 1;
    }

    if (mkdir("/system/bin/usr") != 0 || chdir("/system/bin/usr") != 0) {
        perror("install");
        return 1;
    }

    if (mkdir("/system/bin/usr/commands") != 0 ||
        chdir("/system/bin/usr/commands") != 0) {
        perror("install");
        return 1;
    }

    printf("Copying command files...\n");
    for (int i = 0; i < COMMAND_COUNT; i++) {
        char src[64];
        char dst[128];
        snprintf(src, sizeof(src), "/%s", command_files[i]);
        snprintf(dst, sizeof(dst), "/system/bin/usr/commands/%s", command_files[i]);

        int copied = sys_copy_file(src, dst);
        if (copied != 0) {
            printf("copy failed: %s -> %s\n", src, dst);
        }
    }

    printf("Verifying...\n");
    chdir("/system/bin/usr/commands");

    int ok = 1;
    for (int i = 0; i < COMMAND_COUNT; i++) {
        int fd = open(command_files[i]);
        if (fd < 0) {
            printf("Missing: %s\n", command_files[i]);
            ok = 0;
        } else {
            close(fd);
        }
    }

    if (ok) {
        printf("All files verified successfully.\n");
        chdir("/");

        for (int i = 0; i < COMMAND_COUNT; i++) {
            unlink(command_files[i]);
        }

        unlink("install.elf");
        printf("Installation complete. Command files moved to /system/bin/usr/commands.\n");
    } else {
        printf("Installation incomplete. Original files not removed.\n");
    }

    return 0;
}