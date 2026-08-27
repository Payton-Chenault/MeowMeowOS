#include "../libs/meow_libc.h"

#define MODULE "BENCHIO"

#define BENCH_FILE "io_bench.txt"
#define BENCH_LINES 2000
#define LINE_PAYLOAD "MeowMeowOS 4KB Buffered I/O Performance Benchmark Line Data.\n"

DESCRIPTION("benchio.elf: Benchmark file I/O and streaming throughput");

extern void log_trace(const char *module, const char *fmt, ...);

static int generate_test_file(const char *filename, int num_lines) {
    printf("Generating %s with %d lines...\n", filename, num_lines);
    
    int fd = sys_create(filename);
    if (fd < 0) {
        perror("benchio: create failed");
        return -1;
    }
    close(fd);

    fd = open(filename);
    if (fd < 0) {
        perror("benchio: open write failed");
        return -1;
    }

    size_t payload_len = strlen(LINE_PAYLOAD);
    for (int i = 0; i < num_lines; i++) {
        if (write(fd, LINE_PAYLOAD, payload_len) <= 0) {
            perror("benchio: write error");
            close(fd);
            return -1;
        }
    }
    
    close(fd);
    printf("Test file created successfully.\n\n");
    return 0;
}

static void benchmark_buffered_head(const char *filename, int lines_to_read) {
    printf("--- Benchmark 1: Buffered head (%d lines) ---\n", lines_to_read);
    
    int fd = open(filename);
    if (fd < 0) {
        perror("benchio: head open failed");
        return;
    }

    char *buffer = (char *)malloc(1024);
    if (!buffer) {
        close(fd);
        return;
    }

    unsigned int start_tick = sys_uptime();
    int count = 0;
    while (count < lines_to_read && fgets(buffer, 1024, fd) != NULL) {
        count++;
    }
    unsigned int end_tick = sys_uptime();

    close(fd);
    free(buffer);

    unsigned int elapsed = end_tick - start_tick;
    printf("Read %d lines in %d ticks\n\n", count, elapsed);
}

static void benchmark_buffered_tail(const char *filename, int tail_count) {
    printf("--- Benchmark 2: Buffered tail circular ring (%d lines) ---\n", tail_count);
    
    int fd = open(filename);
    if (fd < 0) {
        perror("benchio: tail open failed");
        return;
    }

    char **ring = (char **)calloc(tail_count, sizeof(char *));
    if (!ring) {
        close(fd);
        return;
    }

    char line_buf[1024];
    unsigned int start_tick = sys_uptime();
    
    int total_lines = 0;
    while (fgets(line_buf, sizeof(line_buf), fd) != NULL) {
        int slot = total_lines % tail_count;
        if (ring[slot]) free(ring[slot]);
        ring[slot] = strdup(line_buf);
        total_lines++;
    }
    
    unsigned int end_tick = sys_uptime();

    for (int i = 0; i < tail_count; i++) {
        if (ring[i]) free(ring[i]);
    }
    free(ring);
    close(fd);

    unsigned int elapsed = end_tick - start_tick;
    printf("Streamed %d total lines, buffered last %d lines in %d ticks\n\n", 
           total_lines, tail_count, elapsed);
}

static void benchmark_raw_bulk_read(const char *filename) {
    printf("--- Benchmark 3: Bulk raw 4KB block read ---\n");
    
    int fd = open(filename);
    if (fd < 0) {
        perror("benchio: bulk open failed");
        return;
    }

    char *block = (char *)malloc(4096);
    if (!block) {
        close(fd);
        return;
    }

    unsigned int start_tick = sys_uptime();
    int bytes_read = 0;
    int total_bytes = 0;
    
    while ((bytes_read = read(fd, block, 4096)) > 0) {
        total_bytes += bytes_read;
    }
    unsigned int end_tick = sys_uptime();

    close(fd);
    free(block);

    unsigned int elapsed = end_tick - start_tick;
    printf("Streamed %d total bytes in %d ticks\n\n", total_bytes, elapsed);
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    sys_set_busy(true);

    printf("=====================================================\n");
    printf("         MeowMeowOS 4KB Buffered I/O Benchmark       \n");
    printf("=====================================================\n\n");

    if (generate_test_file(BENCH_FILE, BENCH_LINES) != 0) {
        sys_set_busy(false);
        return 1;
    }

    benchmark_buffered_head(BENCH_FILE, 1000);
    benchmark_buffered_tail(BENCH_FILE, 500);
    benchmark_raw_bulk_read(BENCH_FILE);

    unlink(BENCH_FILE);

    printf("=====================================================\n");
    printf("Benchmark suite finished cleanly.\n");

    sys_set_busy(false);
    return 0;
}