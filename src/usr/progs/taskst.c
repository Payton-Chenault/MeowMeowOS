#include "../libs/meow_libc.h"

DESCRIPTION("taskst.elf: Scheduler stress test");

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    static const int pass_count = 20;
    static const int page_count = 8;

    printf("Scheduler stress test starting...\n");

    for (int pass = 0; pass < pass_count; pass++) {
        void *pages[page_count];

        for (int i = 0; i < page_count; i++) {
            pages[i] = sys_alloc_page();
            if (pages[i] == NULL) {
                printf("allocation failed during scheduler stress test\n");
                return 1;
            }

            char *p = (char *)pages[i];
            for (int j = 0; j < 4096; j++) {
                p[j] = (char)((pass + i + j) & 0xFF);
            }
        }

        for (int i = 0; i < page_count; i++) {
            char *p = (char *)pages[i];
            for (int j = 0; j < 4096; j++) {
                if (p[j] != (char)((pass + i + j) & 0xFF)) {
                    printf("data corruption detected in scheduler stress test\n");
                    return 2;
                }
            }
        }

        for (int i = 0; i < page_count; i++) {
            if (sys_free_page(pages[i]) != 0) {
                printf("free failed during scheduler stress test\n");
                return 3;
            }
        }

        sys_yield();

        if ((pass + 1) % 5 == 0) {
            printf("pass %d/%d\n", pass + 1, pass_count);
        }
    }

    printf("scheduler stress test complete\n");
    return 0;
}