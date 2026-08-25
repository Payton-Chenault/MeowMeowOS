#include "../libs/meow_libc.h"

#define COMMAND_COUNT 21

DESCRIPTION("install.elf: Install command and asset files");

static const char *command_files[COMMAND_COUNT] = {
    "cat.elf",     "echo.elf",    "format.elf", "ls.elf",
    "mkdir.elf",   "rm.elf",      "rmdir.elf",  "taskst.elf",
    "testdsk.elf", "testmem.elf", "touch.elf",  "uptime.elf",
    "pwd.elf",     "stat.elf",    "head.elf",   "tail.elf",
    "redir.elf", "dmesg.elf", "ps.elf", "free.elf", "date.elf"};

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    printf("MeowMeowOS Installation\n");
    printf("Creating system directories...\n");

    mkdir("/system");
    mkdir("/system/bin");
    mkdir("/system/bin/usr");
    mkdir("/system/bin/usr/commands");
    mkdir("/system/assets");
    mkdir("/system/assets/splash_screen");

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

    printf("Copying splash screen asset...\n");
    if (sys_copy_file("/splash.bmp", "/system/assets/splash_screen/splash.bmp") != 0) {
        printf("Warning: failed to copy splash.bmp to /system/assets/splash_screen/\n");
    }

    printf("Verifying installation...\n");
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

        unlink("/splash.bmp");
        unlink("install.elf");
        printf("Installation complete. System files organized under /system/.\n");
    } else {
        printf("Installation incomplete. Original files not removed.\n");
    }

    return 0;
}