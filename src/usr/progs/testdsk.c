#include "../libs/meow_libc.h"

#define MODULE "TESTDSK"
#define TEST_PAYLOAD_SIZE 2048

DESCRIPTION("testdsk.elf: Disk I/O read/write verification test");

extern void log_trace(const char *module, const char *fmt, ...);

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    const char *test_file = "dsk_test.bin";
    log_trace(MODULE, "Starting disk read/write verification suite on %s", test_file);

    printf("==================================================\n");
    printf("        MeowMeowOS Disk Subsystem Benchmark       \n");
    printf("==================================================\n");

    uint8_t *write_buf = (uint8_t *)malloc(TEST_PAYLOAD_SIZE);
    uint8_t *read_buf = (uint8_t *)malloc(TEST_PAYLOAD_SIZE);

    if (!write_buf || !read_buf) {
        printf("testdsk: memory allocation failed\n");
        if (write_buf) free(write_buf);
        if (read_buf) free(read_buf);
        return 1;
    }

    for (int i = 0; i < TEST_PAYLOAD_SIZE; i++) {
        write_buf[i] = (uint8_t)((i * 7 + 13) & 0xFF);
    }

    printf("Creating target file %s...\n", test_file);
    int fd = sys_create(test_file);
    if (fd < 0) {
        perror("create failed");
        free(write_buf);
        free(read_buf);
        return 1;
    }
    close(fd);

    printf("Writing %d bytes of binary test patterns...\n", TEST_PAYLOAD_SIZE);
    fd = open(test_file);
    if (fd < 0) {
        perror("open for write failed");
        free(write_buf);
        free(read_buf);
        return 1;
    }

    int bytes_written = write(fd, write_buf, TEST_PAYLOAD_SIZE);
    log_trace(MODULE, "Wrote %d bytes to %s", bytes_written, test_file);
    close(fd);

    if (bytes_written != TEST_PAYLOAD_SIZE) {
        printf("[FAIL] Incomplete write (wrote %d/%d bytes)\n", bytes_written, TEST_PAYLOAD_SIZE);
        unlink(test_file);
        free(write_buf);
        free(read_buf);
        return 1;
    }

    printf("Reading back and validating payload...\n");
    fd = open(test_file);
    if (fd < 0) {
        perror("open for read failed");
        unlink(test_file);
        free(write_buf);
        free(read_buf);
        return 1;
    }

    int bytes_read = read(fd, read_buf, TEST_PAYLOAD_SIZE);
    log_trace(MODULE, "Read %d bytes from %s", bytes_read, test_file);
    close(fd);

    int corrupt = 0;
    for (int i = 0; i < TEST_PAYLOAD_SIZE; i++) {
        if (read_buf[i] != write_buf[i]) {
            printf("[MISMATCH] Byte %d (expected 0x%x, got 0x%x)\n",
                   i, (unsigned int)write_buf[i], (unsigned int)read_buf[i]);
            corrupt = 1;
            break;
        }
    }

    unlink(test_file);
    free(write_buf);
    free(read_buf);

    if (corrupt) {
        printf("==================================================\n");
        printf("[FAIL] Disk data verification failed!\n");
        return 2;
    }

    printf("==================================================\n");
    printf("[PASS] Disk write/read verification succeeded! (%d bytes)\n", TEST_PAYLOAD_SIZE);
    log_trace(MODULE, "Disk benchmark completed successfully");
    return 0;
}