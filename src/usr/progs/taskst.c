#include "../libs/meow_libc.h"

#define MODULE "TASKST"

DESCRIPTION("taskst.elf: Scheduler and memory subsystem stress test");

extern void log_trace(const char *module, const char *fmt, ...);

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    static const int pass_count = 20;
    static const int page_count = 8;
    static const size_t heap_alloc_size = 2048;

    log_trace(MODULE, "Starting scheduler & memory stress test (%d passes)", pass_count);

    printf("=====================================================\n");
    printf("        MeowMeowOS Scheduler & Memory Stress         \n");
    printf("=====================================================\n");

    for (int pass = 0; pass < pass_count; pass++) {
        log_trace(MODULE, "Beginning pass %d/%d", pass + 1, pass_count);

        void *pages[page_count];
        void *heap_chunks[page_count];

        // 1. Direct Page and Dynamic Heap allocations
        for (int i = 0; i < page_count; i++) {
            pages[i] = sys_alloc_page();
            heap_chunks[i] = malloc(heap_alloc_size);

            if (pages[i] == NULL || heap_chunks[i] == NULL) {
                printf("[FAIL] Allocation failed at pass %d, index %d\n", pass + 1, i);
                return 1;
            }

            char *p = (char *)pages[i];
            char *h = (char *)heap_chunks[i];
            for (int j = 0; j < 4096; j++) {
                p[j] = (char)((pass + i + j) & 0xFF);
            }
            for (size_t j = 0; j < heap_alloc_size; j++) {
                h[j] = (char)((pass ^ i ^ j) & 0xFF);
            }
        }

        // 2. Pattern Verification
        for (int i = 0; i < page_count; i++) {
            char *p = (char *)pages[i];
            char *h = (char *)heap_chunks[i];

            for (int j = 0; j < 4096; j++) {
                if (p[j] != (char)((pass + i + j) & 0xFF)) {
                    printf("[CORRUPT] Page data mismatch at pass %d!\n", pass + 1);
                    return 2;
                }
            }

            for (size_t j = 0; j < heap_alloc_size; j++) {
                if (h[j] != (char)((pass ^ i ^ j) & 0xFF)) {
                    printf("[CORRUPT] Heap data mismatch at pass %d!\n", pass + 1);
                    return 2;
                }
            }
        }

        // 3. Cleanup & Yield
        for (int i = 0; i < page_count; i++) {
            sys_free_page(pages[i]);
            free(heap_chunks[i]);
        }

        sys_yield();

        if ((pass + 1) % 5 == 0 || pass == pass_count - 1) {
            int progress = ((pass + 1) * 100) / pass_count;
            printf(" [OK] Pass %d/%d completed (%d%%)\n", pass + 1, pass_count, progress);
        }
    }

    printf("=====================================================\n");
    printf("[PASS] Scheduler & memory stress test succeeded.\n");
    log_trace(MODULE, "Stress test finished cleanly");
    return 0;
}