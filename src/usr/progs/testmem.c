#include "../libs/meow_libc.h"

DESCRIPTION("testmem.elf: Comprehensive dynamic memory test suite");

static int test_sbrk_direct(void) {
    printf("[1/6] Testing direct sbrk expansion...\n");

    void *initial_break = sbrk(0);
    if (initial_break == (void *)-1) {
        printf("FAILED: Initial sbrk(0) query failed\n");
        return 1;
    }

    // Allocate 8192 bytes (2 physical pages) directly via sbrk
    void *new_mem = sbrk(8192);
    if (new_mem == (void *)-1 || new_mem != initial_break) {
        printf("FAILED: sbrk(8192) expansion failed\n");
        return 1;
    }

    uint8_t *byte_ptr = (uint8_t *)new_mem;
    for (int i = 0; i < 8192; i++) {
        byte_ptr[i] = (uint8_t)(i & 0xFF);
    }

    for (int i = 0; i < 8192; i++) {
        if (byte_ptr[i] != (uint8_t)(i & 0xFF)) {
            printf("FAILED: Data verification failed at offset %d\n", i);
            return 1;
        }
    }

    void *current_break = sbrk(0);
    if ((uint32_t)current_break != (uint32_t)initial_break + 8192) {
        printf("FAILED: Heap break mismatch after expansion\n");
        return 1;
    }

    printf("PASS: Direct sbrk expansion and multi-page write verified\n");
    return 0;
}

static int test_page_allocation(void) {
    printf("[2/6] Testing kernel direct page allocation...\n");

    char *ptr = (char *)sys_alloc_page();
    if (!ptr) {
        printf("FAILED: Page allocation failed\n");
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
    if (!valid) {
        printf("FAILED: Page data mismatch\n");
        return 1;
    }

    printf("PASS: Page allocation test verified\n");
    return 0;
}

static int test_large_allocation(void) {
    printf("[3/6] Testing large multi-page malloc & calloc...\n");

    const size_t large_size = 64 * 1024; // 64 KB
    uint8_t *large_buf = (uint8_t *)malloc(large_size);
    if (!large_buf) {
        printf("FAILED: malloc(64KB) failed\n");
        return 1;
    }

    for (size_t i = 0; i < large_size; i++) {
        large_buf[i] = (uint8_t)(i ^ 0xAA);
    }

    for (size_t i = 0; i < large_size; i++) {
        if (large_buf[i] != (uint8_t)(i ^ 0xAA)) {
            printf("FAILED: Large allocation data mismatch at index %d\n", (int)i);
            free(large_buf);
            return 1;
        }
    }
    free(large_buf);

    // Test calloc zero-initialization on large block
    uint32_t *zero_buf = (uint32_t *)calloc(4096, sizeof(uint32_t)); // 16 KB
    if (!zero_buf) {
        printf("FAILED: calloc(16KB) failed\n");
        return 1;
    }

    for (size_t i = 0; i < 4096; i++) {
        if (zero_buf[i] != 0) {
            printf("FAILED: calloc memory not zeroed at index %d\n", (int)i);
            free(zero_buf);
            return 1;
        }
    }
    free(zero_buf);

    printf("PASS: Large malloc and calloc verified\n");
    return 0;
}

static int test_coalescing_and_splitting(void) {
    printf("[4/6] Testing block splitting and bidirectional coalescing...\n");

    // Allocate 3 sequential blocks
    void *b1 = malloc(512);
    void *b2 = malloc(512);
    void *b3 = malloc(512);

    if (!b1 || !b2 || !b3) {
        printf("FAILED: Initial block allocation failed\n");
        return 1;
    }

    // Free edge blocks first, then middle block to trigger dual merge
    free(b1);
    free(b3);
    free(b2);

    // Allocate single block large enough to only fit into the combined A+B+C space
    void *big = malloc(1400);
    if (!big) {
        printf("FAILED: Coalesced block allocation failed\n");
        return 1;
    }

    memset(big, 0x55, 1400);
    free(big);

    printf("PASS: Bidirectional coalescing and re-splitting verified\n");
    return 0;
}

static int test_realloc_growth(void) {
    printf("[5/6] Testing realloc and in-place expansion...\n");

    const size_t initial_elems = 16;
    const size_t grown_elems = 64;
    const size_t elem_size = sizeof(int);

    int *arr = (int *)calloc(initial_elems, elem_size);
    if (!arr) {
        printf("FAILED: Initial calloc failed\n");
        return 1;
    }

    for (size_t i = 0; i < initial_elems; i++) {
        arr[i] = (int)i;
    }

    arr = (int *)reallocarray(arr, grown_elems, elem_size);
    if (!arr) {
        printf("FAILED: reallocarray failed\n");
        return 1;
    }

    for (size_t i = 0; i < initial_elems; i++) {
        if (arr[i] != (int)i) {
            printf("FAILED: Old data corrupted at index %d\n", (int)i);
            free(arr);
            return 1;
        }
    }

    for (size_t i = initial_elems; i < grown_elems; i++) {
        arr[i] = (int)(i * 2);
    }

    for (size_t i = 0; i < grown_elems; i++) {
        int expected = (i < initial_elems) ? (int)i : (int)(i * 2);
        if (arr[i] != expected) {
            printf("FAILED: Mismatch at index %d (got %d, expected %d)\n", (int)i, arr[i], expected);
            free(arr);
            return 1;
        }
    }

    free(arr);
    printf("PASS: realloc and reallocarray verified\n");
    return 0;
}

static int test_heap_stress(void) {
    printf("[6/6] Running fragmented heap stress test...\n");

    const int block_count = 64;
    void *blocks[64];
    size_t sizes[64];

    // Allocate varied block sizes
    for (int i = 0; i < block_count; i++) {
        sizes[i] = ((i % 8) + 1) * 64; // 64B to 512B
        blocks[i] = malloc(sizes[i]);
        if (!blocks[i]) {
            printf("FAILED: Stress malloc failed at block %d\n", i);
            return 1;
        }
        memset(blocks[i], (i + 1) & 0xFF, sizes[i]);
    }

    // Free alternate blocks to create fragmented holes
    for (int i = 0; i < block_count; i += 2) {
        free(blocks[i]);
        blocks[i] = NULL;
    }

    // Reallocate into the holes with smaller chunks
    for (int i = 0; i < block_count; i += 2) {
        sizes[i] = 32;
        blocks[i] = malloc(sizes[i]);
        if (!blocks[i]) {
            printf("FAILED: Hole reallocation failed at block %d\n", i);
            return 1;
        }
        memset(blocks[i], 0xEE, sizes[i]);
    }

    // Verify non-freed blocks maintain integrity
    for (int i = 1; i < block_count; i += 2) {
        uint8_t *p = (uint8_t *)blocks[i];
        for (size_t j = 0; j < sizes[i]; j++) {
            if (p[j] != (uint8_t)((i + 1) & 0xFF)) {
                printf("FAILED: Data corruption in persistent block %d\n", i);
                return 1;
            }
        }
    }

    // Clean up remaining allocations
    for (int i = 0; i < block_count; i++) {
        if (blocks[i]) {
            free(blocks[i]);
        }
    }

    printf("PASS: Heap stress & fragmentation test verified\n");
    return 0;
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    printf("\n=== MeowMeowOS Dynamic Memory Test Suite ===\n\n");

    if (test_sbrk_direct() != 0) return 1;
    if (test_page_allocation() != 0) return 2;
    if (test_large_allocation() != 0) return 3;
    if (test_coalescing_and_splitting() != 0) return 4;
    if (test_realloc_growth() != 0) return 5;
    if (test_heap_stress() != 0) return 6;

    printf("\n============================================\n");
    printf(" ALL DYNAMIC HEAP & MEMORY TESTS PASSED! \n");
    printf("============================================\n\n");
    return 0;
}