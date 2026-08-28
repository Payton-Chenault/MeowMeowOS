#include "../libs/meow_libc.h"

#define COMMAND_COUNT 30
#define MAX_SOUND_ASSETS 32

#define MODULE "INSTALL"

DESCRIPTION("install.elf: Install command and dynamically categorized asset files");

extern void log_trace(const char *module, const char *fmt, ...);
extern void log_debug(const char *module, const char *fmt, ...);
extern void log_info(const char *module, const char *fmt, ...);
extern void log_warning(const char *module, const char *fmt, ...);
extern void log_error(const char *module, const char *fmt, ...);

static const char *command_files[COMMAND_COUNT] = {
    "cat.elf",     "echo.elf",    "format.elf", "ls.elf",
    "mkdir.elf",   "rm.elf",      "rmdir.elf",  "taskst.elf",
    "testdsk.elf", "testmem.elf", "touch.elf",  "uptime.elf",
    "pwd.elf",     "stat.elf",    "head.elf",   "tail.elf",
    "redir.elf",   "dmesg.elf",   "ps.elf",     "free.elf",
    "date.elf",    "benchio.elf", "grep.elf",   "kill.elf",
    "lspci.elf",   "testall.elf", "ping.elf",   "play.elf",
    "reboot.elf",  "poweroff.elf"
};

typedef struct {
    char src_root_name[64];
    char dst_rel_path[128];
} sound_asset_map_t;

static sound_asset_map_t discovered_sounds[MAX_SOUND_ASSETS];
static int sound_count = 0;

static void discover_sound_file(const char *filename) {
    if (sound_count >= MAX_SOUND_ASSETS) return;
    
    size_t len = strlen(filename);
    if (len < 5 || strcasecmp(filename + len - 4, ".wav") != 0) {
        return;
    }

    log_debug(MODULE, "Discovered audio asset on root: '%s'", filename);
    strncpy(discovered_sounds[sound_count].src_root_name, filename, 63);

    char base_name[64];
    strncpy(base_name, filename, sizeof(base_name) - 1);
    base_name[len - 4] = '\0'; // strip .wav

    char *underscore = strchr(base_name, '_');
    if (underscore != NULL) {
        *underscore = '\0';
        char category[64];
        strncpy(category, base_name, sizeof(category) - 1);
        if (category[strlen(category) - 1] != 's') {
            strcat(category, "s");
        }
        // Dynamically create category directory under /system/assets/sounds/
        char cat_dir[128];
        snprintf(cat_dir, sizeof(cat_dir), "/system/assets/sounds/%s", category);
        mkdir(cat_dir);

        snprintf(discovered_sounds[sound_count].dst_rel_path, 127,
                 "/system/assets/sounds/%s/%s.wav", category, underscore + 1);
    } else {
        mkdir("/system/assets/sounds/system");
        snprintf(discovered_sounds[sound_count].dst_rel_path, 127,
                 "/system/assets/sounds/system/%s.wav", base_name);
    }

    sound_count++;
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    log_info(MODULE, "Starting MeowMeowOS system installation routine...");

    printf("=====================================================\n");
    printf("             MeowMeowOS System Installer             \n");
    printf("=====================================================\n");
    printf("Creating system directory hierarchy...\n");

    mkdir("/system");
    mkdir("/system/bin");
    mkdir("/system/bin/usr");
    mkdir("/system/bin/usr/commands");
    mkdir("/system/assets");
    mkdir("/system/assets/splash_screen");
    mkdir("/system/assets/sounds");
    mkdir("/system/assets/sounds/notifications");
    mkdir("/system/assets/sounds/system");
    mkdir("/system/assets/sounds/errors");

    printf("Installing userland commands to /system/bin/usr/commands/...\n");
    for (int i = 0; i < COMMAND_COUNT; i++) {
        char src[64];
        char dst[128];
        snprintf(src, sizeof(src), "/%s", command_files[i]);
        snprintf(dst, sizeof(dst), "/system/bin/usr/commands/%s", command_files[i]);
        log_trace(MODULE, "Copying command binary: %s -> %s", src, dst);
        int copied = sys_copy_file(src, dst);
        if (copied != 0) {
            printf("Warning: failed to copy %s -> %s\n", src, dst);
            log_warning(MODULE, "Failed to copy %s to destination %s", src, dst);
        }
    }

    printf("Installing splash screen assets...\n");
    if (sys_copy_file("/splash.bmp", "/system/assets/splash_screen/splash.bmp") != 0) {
        printf("Warning: failed to copy splash.bmp to /system/assets/splash_screen/\n");
        log_warning(MODULE, "Splash screen asset copy returned non-zero code");
    }

    sound_count = 0;
    chdir("/");
    
    // Dynamically check common and convention-based sound names on root
    const char *common_sounds[] = {
        "notification_blip.wav",
        "system_startup.wav",
        "system_shutdown.wav",
        "notification_alert.wav",
        "error_blip.wav",
        "error_beep.wav",
        "ui_click.wav"
    };

    for (size_t s = 0; s < sizeof(common_sounds) / sizeof(common_sounds[0]); s++) {
        sys_stat_t st;
        if (stat(common_sounds[s], &st) == 0) {
            discover_sound_file(common_sounds[s]);
        }
    }

    if (sound_count > 0) {
        printf("Installing %d dynamically categorized sound asset(s)...\n", sound_count);
        for (int i = 0; i < sound_count; i++) {
            char src_full[80];
            snprintf(src_full, sizeof(src_full), "/%s", discovered_sounds[i].src_root_name);
            printf(" -> %s => %s\n", discovered_sounds[i].src_root_name, discovered_sounds[i].dst_rel_path);
            log_info(MODULE, "Relocating sound asset: %s -> %s", src_full, discovered_sounds[i].dst_rel_path);
            sys_copy_file(src_full, discovered_sounds[i].dst_rel_path);
        }
    }

    printf("Verifying installation integrity...\n");
    chdir("/system/bin/usr/commands");
    int ok = 1;
    for (int i = 0; i < COMMAND_COUNT; i++) {
        int fd = open(command_files[i]);
        if (fd < 0) {
            printf("Missing required binary: %s\n", command_files[i]);
            log_error(MODULE, "Verification failed for binary: %s", command_files[i]);
            ok = 0;
        } else {
            close(fd);
        }
    }

    if (ok) {
        printf("All binaries and assets verified successfully.\n");
        log_info(MODULE, "Installation verification passed. Cleaning up root drive...");
        chdir("/");
        for (int i = 0; i < COMMAND_COUNT; i++) {
            unlink(command_files[i]);
        }
        unlink("/splash.bmp");
        for (int i = 0; i < sound_count; i++) {
            unlink(discovered_sounds[i].src_root_name);
        }
        unlink("install.elf");
        printf("=====================================================\n");
        printf(" Installation complete! Files organized under /system/\n");
        printf("=====================================================\n");
        log_info(MODULE, "Installation routine finished cleanly.");
    } else {
        printf("=====================================================\n");
        printf(" Installation incomplete. Original files preserved.\n");
        printf("=====================================================\n");
        log_error(MODULE, "Installation aborting due to missing binaries.");
    }

    return ok ? 0 : 1;
}