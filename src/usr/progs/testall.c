#include "../libs/meow_libc.h"

#define MODULE "TESTALL"

DESCRIPTION("testall.elf: Comprehensive end-to-end OS regression test suite");

extern void log_trace(const char *module, const char *fmt, ...);
extern void log_debug(const char *module, const char *fmt, ...);
extern void log_info(const char *module, const char *fmt, ...);
extern void log_warning(const char *module, const char *fmt, ...);
extern void log_error(const char *module, const char *fmt, ...);

static int g_tests_passed = 0;
static int g_tests_failed = 0;

static void print_test_result(const char *name, bool ok, const char *detail) {
    if (ok) {
        log_debug(MODULE, "Test '%s': PASS", name);
    } else {
        log_error(MODULE, "Test '%s': FAIL (%s)", name, detail ? detail : "Unknown");
    }
    
    printf("  %-42s [ %s ]", name, ok ? "PASS" : "FAIL");
    if (!ok && detail) {
        printf(" (%s)", detail);
    }
    printf("\n");
    if (ok) {
        g_tests_passed++;
    } else {
        g_tests_failed++;
    }
}

static bool test_heap_allocator(void) {
    log_trace(MODULE, "Testing dynamic heap allocator (malloc, calloc, realloc, free)");
    void *p1 = malloc(256);
    if (!p1) {
        log_error(MODULE, "test_heap_allocator: malloc(256) returned NULL");
        return false;
    }
    memset(p1, 0xAA, 256);

    int *p2 = (int *)calloc(64, sizeof(int));
    if (!p2) {
        log_error(MODULE, "test_heap_allocator: calloc(64) returned NULL");
        free(p1);
        return false;
    }
    for (int i = 0; i < 64; i++) {
        if (p2[i] != 0) {
            log_error(MODULE, "test_heap_allocator: calloc memory not zeroed at index %d", i);
            free(p1);
            free(p2);
            return false;
        }
        p2[i] = i * 10;
    }

    p1 = realloc(p1, 1024);
    if (!p1) {
        log_error(MODULE, "test_heap_allocator: realloc(1024) returned NULL");
        free(p2);
        return false;
    }
    for (int i = 0; i < 256; i++) {
        if (((uint8_t *)p1)[i] != 0xAA) {
            log_error(MODULE, "test_heap_allocator: realloc corrupted memory at offset %d", i);
            free(p1);
            free(p2);
            return false;
        }
    }

    free(p1);
    free(p2);
    log_trace(MODULE, "test_heap_allocator: all allocations and frees verified");
    return true;
}

static bool test_direct_page_allocation(void) {
    log_trace(MODULE, "Testing direct virtual page allocation via sys_alloc_page");
    void *page = sys_alloc_page();
    if (!page) {
        log_error(MODULE, "test_direct_page_allocation: sys_alloc_page returned NULL");
        return false;
    }

    uint8_t *b = (uint8_t *)page;
    for (int i = 0; i < 4096; i++) {
        b[i] = (uint8_t)(i & 0xFF);
    }
    for (int i = 0; i < 4096; i++) {
        if (b[i] != (uint8_t)(i & 0xFF)) {
            log_error(MODULE, "test_direct_page_allocation: page data mismatch at index %d", i);
            sys_free_page(page);
            return false;
        }
    }
    int ret = sys_free_page(page);
    if (ret != 0) {
        log_error(MODULE, "test_direct_page_allocation: sys_free_page returned error code %d", ret);
        return false;
    }
    return true;
}

static bool test_vfs_file_io(void) {
    log_trace(MODULE, "Testing VFS file create, write, read, and lseek");
    const char *test_file = "test_run.tmp";
    unlink(test_file);

    int fd = sys_create(test_file);
    if (fd < 0) {
        log_error(MODULE, "test_vfs_file_io: sys_create failed for '%s'", test_file);
        return false;
    }
    close(fd);

    fd = open(test_file);
    if (fd < 0) {
        log_error(MODULE, "test_vfs_file_io: open for write failed");
        return false;
    }

    const char *payload = "Hello MeowMeowOS VFS Layer!";
    size_t len = strlen(payload);
    if (write(fd, payload, len) != (int)len) {
        log_error(MODULE, "test_vfs_file_io: write incomplete");
        close(fd);
        unlink(test_file);
        return false;
    }
    close(fd);

    fd = open(test_file);
    if (fd < 0) {
        log_error(MODULE, "test_vfs_file_io: open for read failed");
        unlink(test_file);
        return false;
    }

    char buf[64];
    memset(buf, 0, sizeof(buf));
    int bytes = read(fd, buf, sizeof(buf) - 1);
    if (bytes != (int)len || strcmp(buf, payload) != 0) {
        log_error(MODULE, "test_vfs_file_io: payload content mismatch (got '%s')", buf);
        close(fd);
        unlink(test_file);
        return false;
    }

    long cur_pos = lseek(fd, 6, SEEK_SET);
    if (cur_pos != 6) {
        log_error(MODULE, "test_vfs_file_io: lseek returned unexpected offset %ld", cur_pos);
        close(fd);
        unlink(test_file);
        return false;
    }

    char seek_buf[16];
    memset(seek_buf, 0, sizeof(seek_buf));
    read(fd, seek_buf, 10);
    close(fd);
    unlink(test_file);

    return strcmp(seek_buf, "MeowMeowOS") == 0;
}

static bool test_directory_lifecycle(void) {
    log_trace(MODULE, "Testing directory creation, traversal, and removal");
    const char *dir_name = "test_dir";
    rmdir(dir_name);

    if (mkdir(dir_name) != 0) {
        log_error(MODULE, "test_directory_lifecycle: mkdir failed for '%s'", dir_name);
        return false;
    }

    char original_cwd[256];
    if (!getcwd(original_cwd, sizeof(original_cwd))) {
        log_error(MODULE, "test_directory_lifecycle: getcwd failed");
        return false;
    }

    if (chdir(dir_name) != 0) {
        log_error(MODULE, "test_directory_lifecycle: chdir into created directory failed");
        rmdir(dir_name);
        return false;
    }

    char sub_cwd[256];
    getcwd(sub_cwd, sizeof(sub_cwd));
    bool entered = (strstr(sub_cwd, dir_name) != NULL);

    chdir(original_cwd);
    if (rmdir(dir_name) != 0) {
        log_error(MODULE, "test_directory_lifecycle: rmdir failed");
        return false;
    }

    return entered;
}

static bool test_file_copy(void) {
    log_trace(MODULE, "Testing kernel file copy syscall sys_copy_file");
    const char *src = "src_tmp.txt";
    const char *dst = "dst_tmp.txt";
    unlink(src);
    unlink(dst);

    int fd = sys_create(src);
    if (fd < 0) {
        log_error(MODULE, "test_file_copy: create source file failed");
        return false;
    }
    close(fd);

    fd = open(src);
    write(fd, "DataCopy123", 11);
    close(fd);

    if (sys_copy_file(src, dst) != 0) {
        log_error(MODULE, "test_file_copy: sys_copy_file returned error");
        unlink(src);
        unlink(dst);
        return false;
    }

    fd = open(dst);
    if (fd < 0) {
        log_error(MODULE, "test_file_copy: open destination file failed");
        unlink(src);
        unlink(dst);
        return false;
    }

    char buf[32];
    memset(buf, 0, sizeof(buf));
    int r = read(fd, buf, 31);
    close(fd);

    unlink(src);
    unlink(dst);
    return r == 11 && strcmp(buf, "DataCopy123") == 0;
}

static bool test_pipes(void) {
    log_trace(MODULE, "Testing anonymous IPC pipes (sys_pipe)");
    int pfd[2];
    if (pipe(pfd) != 0) {
        log_error(MODULE, "test_pipes: pipe() creation failed");
        return false;
    }

    const char *msg = "PipeStreamMessage";
    size_t len = strlen(msg);
    if (write(pfd[1], msg, len) != (int)len) {
        log_error(MODULE, "test_pipes: write() failed on pipe write fd");
        close(pfd[0]);
        close(pfd[1]);
        return false;
    }
    
    fflush(pfd[1]);

    char buf[32];
    memset(buf, 0, sizeof(buf));
    int bytes = read(pfd[0], buf, sizeof(buf) - 1);
    close(pfd[0]);
    close(pfd[1]);

    if (bytes != (int)len || strcmp(buf, msg) != 0) {
        log_error(MODULE, "test_pipes: read payload mismatch (got '%s', expected '%s')", buf, msg);
        return false;
    }
    return true;
}

static bool test_dup_operations(void) {
    log_trace(MODULE, "Testing file descriptor duplication via sys_dup");
    const char *temp_file = "dup_test.tmp";
    unlink(temp_file);

    int fd = sys_create(temp_file);
    if (fd < 0) {
        log_error(MODULE, "test_dup_operations: sys_create failed");
        return false;
    }
    close(fd);

    fd = open(temp_file);
    if (fd < 0) {
        log_error(MODULE, "test_dup_operations: open failed");
        return false;
    }

    int cloned_fd = dup(fd);
    if (cloned_fd < 0) {
        log_error(MODULE, "test_dup_operations: dup() failed");
        close(fd);
        unlink(temp_file);
        return false;
    }

    write(cloned_fd, "DUP_OK", 6);
    close(cloned_fd);
    close(fd);

    fd = open(temp_file);
    char buf[16];
    memset(buf, 0, sizeof(buf));
    int r = read(fd, buf, 15);
    close(fd);
    unlink(temp_file);

    return r == 6 && strcmp(buf, "DUP_OK") == 0;
}

static bool test_devfs_null(void) {
    log_trace(MODULE, "Testing /dev/null device");
    int fd = open("/dev/null");
    if (fd < 0) {
        log_error(MODULE, "test_devfs_null: failed to open /dev/null");
        return false;
    }

    char buf[16];
    int r = read(fd, buf, sizeof(buf));
    int w = write(fd, "test", 4);
    close(fd);

    return r == 0 && w == 4;
}

static bool test_devfs_zero(void) {
    log_trace(MODULE, "Testing /dev/zero device");
    int fd = open("/dev/zero");
    if (fd < 0) {
        log_error(MODULE, "test_devfs_zero: failed to open /dev/zero");
        return false;
    }

    uint8_t buf[32];
    memset(buf, 0xFF, sizeof(buf));
    int r = read(fd, buf, sizeof(buf));
    close(fd);

    if (r != sizeof(buf)) {
        log_error(MODULE, "test_devfs_zero: read returned unexpected byte count %d", r);
        return false;
    }
    for (size_t i = 0; i < sizeof(buf); i++) {
        if (buf[i] != 0) {
            log_error(MODULE, "test_devfs_zero: non-zero byte found at offset %u", (unsigned int)i);
            return false;
        }
    }
    return true;
}

static bool test_devfs_random(void) {
    log_trace(MODULE, "Testing /dev/random device");
    int fd = open("/dev/random");
    if (fd < 0) {
        log_error(MODULE, "test_devfs_random: failed to open /dev/random");
        return false;
    }

    uint8_t b1[16];
    uint8_t b2[16];
    int r1 = read(fd, b1, sizeof(b1));
    int r2 = read(fd, b2, sizeof(b2));
    close(fd);

    if (r1 != sizeof(b1) || r2 != sizeof(b2)) {
        log_error(MODULE, "test_devfs_random: random read size mismatch");
        return false;
    }
    return memcmp(b1, b2, sizeof(b1)) != 0;
}

static bool test_procfs_meminfo(void) {
    log_trace(MODULE, "Testing /proc/meminfo read");
    int fd = open("/proc/meminfo");
    if (fd < 0) {
        log_error(MODULE, "test_procfs_meminfo: failed to open /proc/meminfo");
        return false;
    }

    char buf[256];
    memset(buf, 0, sizeof(buf));
    int r = read(fd, buf, sizeof(buf) - 1);
    close(fd);

    return r > 0 && strstr(buf, "total(KB)") != NULL;
}

static bool test_procfs_tasks(void) {
    log_trace(MODULE, "Testing /proc/tasks read");
    int fd = open("/proc/tasks");
    if (fd < 0) {
        log_error(MODULE, "test_procfs_tasks: failed to open /proc/tasks");
        return false;
    }

    char buf[512];
    memset(buf, 0, sizeof(buf));
    int r = read(fd, buf, sizeof(buf) - 1);
    close(fd);

    return r > 0 && strstr(buf, "PID") != NULL;
}

static bool test_procfs_uptime(void) {
    log_trace(MODULE, "Testing /proc/uptime read");
    int fd = open("/proc/uptime");
    if (fd < 0) {
        log_error(MODULE, "test_procfs_uptime: failed to open /proc/uptime");
        return false;
    }

    char buf[32];
    memset(buf, 0, sizeof(buf));
    int r = read(fd, buf, sizeof(buf) - 1);
    close(fd);

    return r > 0 && atoi(buf) > 0;
}

static bool test_signal_handling(void) {
    log_trace(MODULE, "Testing signal syscall API (sys_signal)");
    sighandler_t old = signal(SIGINT, SIG_IGN);
    if (old == SIG_ERR) {
        log_error(MODULE, "test_signal_handling: signal(SIGINT, SIG_IGN) failed");
        return false;
    }
    sighandler_t prev = signal(SIGINT, SIG_DFL);
    if (prev != SIG_IGN) {
        log_error(MODULE, "test_signal_handling: previous signal handler mismatch");
        return false;
    }
    return true;
}

static bool test_rtc_hardware_time(void) {
    log_trace(MODULE, "Testing RTC CMOS time reading");
    sys_time_t t;
    if (sys_get_time(&t) != 0) {
        log_error(MODULE, "test_rtc_hardware_time: sys_get_time returned error");
        return false;
    }
    return t.year >= 2024 && t.month >= 1 && t.month <= 12 &&
           t.day >= 1 && t.day <= 31 && t.hour < 24 && t.minute < 60;
}

static bool test_pci_bus(void) {
    log_trace(MODULE, "Testing PCI bus enumeration");
    pci_device_t devs[8];
    int count = get_pci_devices(devs, 8);
    if (count <= 0) {
        log_warning(MODULE, "test_pci_bus: no PCI devices returned");
        return false;
    }
    return true;
}

static bool test_scheduler_priorities(void) {
    log_trace(MODULE, "Testing dynamic scheduler priority update via sys_get_priority");
    sys_process_info_t procs[1];
    if (sys_get_process_info(procs, 1) < 1) {
        log_error(MODULE, "test_scheduler_priorities: sys_get_process_info failed");
        return false;
    }

    uint32_t my_pid = procs[0].pid;
    uint8_t old_prio = (uint8_t)sys_get_priority(my_pid);

    sys_set_priority(my_pid, 3);
    uint8_t new_prio = (uint8_t)sys_get_priority(my_pid);

    sys_set_priority(my_pid, old_prio);
    return new_prio == 3;
}

static bool test_network_stack(void) {
    log_trace(MODULE, "Testing Layer 2 stack via local ping to gateway 10.0.2.2");
    uint32_t latency;
    int ret = sys_ping(inet_addr("10.0.2.2"), &latency);
    if (ret != 0) {
        log_error(MODULE, "test_network_stack: local ping failed or unreachable (ret=%d)", ret);
        return false;
    }
    return latency < 500;
}

static bool test_mouse_driver(void) {
    log_trace(MODULE, "Testing PS/2 Mouse Subsystem & Cursor Telemetry");
    sys_mouse_state_t mstate;
    if (sys_get_mouse_state(&mstate) != 0) {
        log_error(MODULE, "test_mouse_driver: sys_get_mouse_state returned error code");
        return false;
    }
    log_trace(MODULE, "test_mouse_driver: Current pos=(%d, %d), buttons=0x%X",
              mstate.x, mstate.y, mstate.buttons);
    return mstate.x >= 0 && mstate.x < 1024 && mstate.y >= 0 && mstate.y < 768;
}

static bool test_dns_resolver(void) {
    log_trace(MODULE, "Testing UDP Transport & DNS Hostname Resolution for 'google.com'");
    uint32_t resolved_ip = 0;
    int ret = sys_dns_resolve("google.com", &resolved_ip);
    if (ret != 0 || resolved_ip == 0) {
        log_error(MODULE, "test_dns_resolver: DNS resolution failed (ret=%d)", ret);
        return false;
    }
    log_info(MODULE, "test_dns_resolver: google.com resolved to %u.%u.%u.%u",
             resolved_ip & 0xFF, (resolved_ip >> 8) & 0xFF,
             (resolved_ip >> 16) & 0xFF, (resolved_ip >> 24) & 0xFF);
    return true;
}

static bool test_ac97_audio(void) {
    log_trace(MODULE, "Testing AC'97 Audio Driver & DMA Stream Playback");
    int16_t test_pcm[256];
    for (int i = 0; i < 256; i++) {
        test_pcm[i] = (i % 2 == 0) ? (int16_t)1000 : (int16_t)-1000;
    }

    int ret = sys_sound_play(test_pcm, sizeof(test_pcm), 44100, 2, 16);
    if (ret != 0) {
        log_error(MODULE, "test_ac97_audio: sys_sound_play failed (ret=%d)", ret);
        return false;
    }
    return true;
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    log_info(MODULE, "Starting MeowMeowOS Regression Test Suite...");

    printf("=====================================================\n");
    printf("        MeowMeowOS Full Subsystem Test Suite         \n");
    printf("=====================================================\n\n");

    print_test_result("Dynamic Memory (malloc/realloc/free)", test_heap_allocator(), "Heap corruption");
    print_test_result("VMM Kernel Page Allocator (sys_alloc_page)", test_direct_page_allocation(), "Page fault");
    print_test_result("VFS File Read/Write/Seek (FAT16)", test_vfs_file_io(), "File I/O error");
    print_test_result("Directory Management (mkdir/chdir/rmdir)", test_directory_lifecycle(), "Path lookup error");
    print_test_result("File Duplication Engine (sys_copy_file)", test_file_copy(), "Copy mismatch");
    print_test_result("Anonymous IPC Pipes (sys_pipe)", test_pipes(), "Broken pipe");
    print_test_result("File Descriptor Cloning (sys_dup)", test_dup_operations(), "Bad FD mapping");
    print_test_result("DevFS Stream Discard (/dev/null)", test_devfs_null(), "Device error");
    print_test_result("DevFS Zero Generator (/dev/zero)", test_devfs_zero(), "Non-zero byte returned");
    print_test_result("DevFS Timing PRNG (/dev/random)", test_devfs_random(), "Low entropy / duplicate");
    print_test_result("ProcFS Memory Metrics (/proc/meminfo)", test_procfs_meminfo(), "Missing header");
    print_test_result("ProcFS Task Table Snapshot (/proc/tasks)", test_procfs_tasks(), "Process table failure");
    print_test_result("ProcFS PIT Timer Telemetry (/proc/uptime)", test_procfs_uptime(), "Zero ticks");
    print_test_result("POSIX Signal Dispatch (SIGINT)", test_signal_handling(), "Handler not registered");
    print_test_result("CMOS Real-Time Clock (sys_get_time)", test_rtc_hardware_time(), "Invalid RTC date");
    print_test_result("PCI Bus Controller Enumeration", test_pci_bus(), "No devices discovered");
    print_test_result("Dynamic Task Priorities (sys_set_priority)", test_scheduler_priorities(), "Priority rejected");
    print_test_result("RTL8139 Layer 2/3 Stack (sys_ping)", test_network_stack(), "ICMP Unreachable");
    print_test_result("PS/2 Mouse & GUI Cursor (sys_get_mouse_state)", test_mouse_driver(), "Mouse Driver Failure");
    print_test_result("DNS Hostname Resolution (sys_dns_resolve)", test_dns_resolver(), "DNS Lookup Failed");
    print_test_result("AC97 Audio Engine (sys_sound_play)", test_ac97_audio(), "Audio Playback Failed");

    printf("\n=====================================================\n");
    printf("Tests Run: %d | Passed: %d | Failed: %d\n",
           g_tests_passed + g_tests_failed, g_tests_passed, g_tests_failed);
    printf("=====================================================\n");

    if (g_tests_failed == 0) {
        log_info(MODULE, "Regression Test Suite completed successfully! (All %d passed)", g_tests_passed);
    } else {
        log_error(MODULE, "Regression Test Suite FAILED! (%d failed, %d passed)", g_tests_failed, g_tests_passed);
    }

    return (g_tests_failed == 0) ? 0 : 1;
}