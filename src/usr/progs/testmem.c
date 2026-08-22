#include "../libs/meow_libc.h"

DESCRIPTION("testmem.elf: Memory test");

static int test_page_allocation(void) {
    char *ptr = (char *)sys_alloc_page();

    if (!ptr) {
        printf("Page allocation failed\n");
        return 1;
    }

    for (int i = 0; i < 256; i++) {
        ptr[i] = (char)(i % 256);
    }

    int valid = 1;
    for (int i = 0; i < 256; i++) {
        if (ptr[i] != (char)(i % 256)) {
            valid = 0;
            break;
        }
    }

    sys_free_page(ptr);
    return valid ? 0 : 1;
}

static int test_heap_stress(void) {
    const int block_count = 32;
    const size_t block_size = 128;

    void *blocks[block_count];

    for (int i = 0; i < block_count; i++) {
        blocks[i] = malloc(block_size);
        if (blocks[i] == NULL) {
            printf("Heap stress: malloc failed at block %d\n", i);
            return 1;
        }

        memset(blocks[i], (i + 1) & 0xFF, block_size);
    }

    for (int i = 0; i < block_count; i++) {
        unsigned char *p = (unsigned char *)blocks[i];
        for (size_t j = 0; j < block_size; j++) {
            if (p[j] != (unsigned char)((i + 1) & 0xFF)) {
                printf("Heap stress: data corruption at block %d, byte %d\n", i, j);
                return 1;
            }
        }
    }

    for (int i = 0; i < block_count; i++) {
        free(blocks[i]);
    }

    printf("Heap stress test passed\n");
    return 0;
}

static int test_reallocarray_growth(void) {
    const size_t initial_elems = 16;
    const size_t grown_elems = 64;
    const size_t elem_size = sizeof(int);

    int *arr = calloc(initial_elems, elem_size);
    if (!arr) {
        printf("reallocarray test: initial calloc failed\n");
        return 1;
    }

    for (size_t i = 0; i < initial_elems; i++) {
        arr[i] = (int)i;
    }

    arr = reallocarray(arr, grown_elems, elem_size);
    if (!arr) {
        printf("reallocarray test: reallocarray failed\n");
        return 1;
    }

    for (size_t i = 0; i < initial_elems; i++) {
        if (arr[i] != (int)i) {
            printf("reallocarray test: old data corrupted at %d\n", i);
            return 1;
        }
    }

    for (size_t i = initial_elems; i < grown_elems; i++) {
        arr[i] = (int)(i * 2);
    }

    for (size_t i = 0; i < grown_elems; i++) {
        int expected = (i < initial_elems) ? (int)i : (int)(i * 2);
        if (arr[i] != expected) {
            printf("reallocarray test: mismatch at %d\n", i);
            return 1;
        }
    }

    free(arr);
    printf("reallocarray test passed\n");
    return 0;
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    printf("Memory test suite starting...\n");

    if (test_page_allocation() != 0) {
        printf("Page test failed\n");
        return 1;
    }
    printf("Page allocation test passed\n");

    if (test_heap_stress() != 0) {
        printf("Heap stress test failed\n");
        return 2;
    }

    if (test_reallocarray_growth() != 0) {
        printf("reallocarray test failed\n");
        return 3;
    }

    printf("All memory tests passed\n");
    return 0;
}