#include "meow_libc.h"
#include <stdint.h>
#include <stddef.h>

extern void _fini(void);

void exit(int status) {
    _fini();
    sys_exit(status);
    while(1);
}

int atoi(const char *str) {
    return (int)strtol(str, NULL, 10);
}

long strtol(const char *str, char **endptr, int base) {
    long result = 0;
    int sign = 1;
    const char *p = str;

    while (*p == ' ' || *p == '\t') p++;

    if (*p == '-') { sign = -1; p++; }
    else if (*p == '+') p++;

    if (base == 0) {
        if (*p == '0') {
            if (p[1] == 'x' || p[1] == 'X') { base = 16; p += 2; }
            else { base = 8; p++; }
        } else base = 10;
    }

    if (base == 16 && *p == '0' && (p[1] == 'x' || p[1] == 'X')) p += 2;

    while (*p) {
        int digit;
        if (*p >= '0' && *p <= '9') digit = *p - '0';
        else if (*p >= 'a' && *p <= 'f') digit = *p - 'a' + 10;
        else if (*p >= 'A' && *p <= 'F') digit = *p - 'A' + 10;
        else break;

        if (digit >= base) break;
        result = result * base + digit;
        p++;
    }

    if (endptr) *endptr = (char *)p;
    return sign * result;
}

char *itoa(int value, char *str, int base) {
    if (base < 2 || base > 36) {
        str[0] = '\0';
        return str;
    }

    char tmp[36];
    int i = 0;
    int negative = (value < 0 && base == 10);

    unsigned int v = negative ? (unsigned int)(-value) : (unsigned int)value;

    do {
        int rem = v % base;
        tmp[i++] = (rem > 9) ? (rem - 10 + 'a') : (rem + '0');
        v /= base;
    } while (v);

    if (negative) tmp[i++] = '-';

    int j = 0;
    while (i > 0) str[j++] = tmp[--i];
    str[j] = '\0';
    return str;
}

const char *strerror(int errnum) {
    switch (errnum) {
    case EPERM:
        return "Operation not permitted";
    case ENOENT:
        return "No such file or directory";
    case EIO:
        return "Input/output error";
    case EBADF:
        return "Bad file descriptor";
    case ENOMEM:
        return "Out of memory";
    case EACCES:
        return "Permission denied";
    case EFAULT:
        return "Bad address";
    case EINVAL:
        return "Invalid argument";
    default:
        return "Unknown error";
    }
}

void perror(const char *s) {
    if (s != NULL && *s != '\0') {
        write(2, s, strlen(s));
        write(2, ": ", 2);
    }

    const char *err = strerror(errno);
    write(2, err, strlen(err));
    write(2, "\n", 1);
}

/* --- Dynamic Heap Allocator via sbrk --- */

#define BLOCK_MAGIC 0xCAFEBABE
#define BLOCK_ALIGN 8
#define ALIGN_UP(x) (((x) + (BLOCK_ALIGN - 1)) & ~(BLOCK_ALIGN - 1))

typedef struct user_block {
    uint32_t magic;
    size_t size;
    int free;
    struct user_block *next;
    struct user_block *prev;
} user_block_t;

static user_block_t *heap_head = NULL;

void *sbrk(intptr_t increment) {
    void *res = sys_sbrk((int32_t)increment);
    if (res == (void *)-1) {
        errno = ENOMEM;
        return (void *)-1;
    }
    return res;
}

void *malloc(size_t size) {
    if (size == 0) return NULL;

    size_t aligned_size = ALIGN_UP(size);

    // 1. Search for a reusable free block
    user_block_t *curr = heap_head;
    while (curr) {
        if (curr->magic != BLOCK_MAGIC) {
            errno = EFAULT;
            return NULL;
        }

        if (curr->free && curr->size >= aligned_size) {
            // Split if the remainder can hold another block descriptor and payload
            if (curr->size >= aligned_size + sizeof(user_block_t) + BLOCK_ALIGN) {
                user_block_t *split = (user_block_t *)((uint8_t *)(curr + 1) + aligned_size);
                split->magic = BLOCK_MAGIC;
                split->size = curr->size - aligned_size - sizeof(user_block_t);
                split->free = 1;
                split->next = curr->next;
                split->prev = curr;

                if (curr->next) curr->next->prev = split;
                curr->next = split;
                curr->size = aligned_size;
            }

            curr->free = 0;
            return (void *)(curr + 1);
        }
        curr = curr->next;
    }

    // 2. No free block fit; allocate from kernel via sbrk
    size_t total_req = sizeof(user_block_t) + aligned_size;
    size_t alloc_chunk = total_req < 4096 ? 4096 : total_req;

    void *raw_mem = sbrk(alloc_chunk);
    if (raw_mem == (void *)-1 || raw_mem == NULL) {
        errno = ENOMEM;
        return NULL;
    }

    user_block_t *new_block = (user_block_t *)raw_mem;
    new_block->magic = BLOCK_MAGIC;
    new_block->size = alloc_chunk - sizeof(user_block_t);
    new_block->free = 0;
    new_block->next = NULL;
    new_block->prev = NULL;

    if (!heap_head) {
        heap_head = new_block;
    } else {
        user_block_t *tail = heap_head;
        while (tail->next) tail = tail->next;
        tail->next = new_block;
        new_block->prev = tail;
    }

    // Split leftover space in the page allocation
    if (new_block->size >= aligned_size + sizeof(user_block_t) + BLOCK_ALIGN) {
        user_block_t *split = (user_block_t *)((uint8_t *)(new_block + 1) + aligned_size);
        split->magic = BLOCK_MAGIC;
        split->size = new_block->size - aligned_size - sizeof(user_block_t);
        split->free = 1;
        split->next = new_block->next;
        split->prev = new_block;

        if (new_block->next) new_block->next->prev = split;
        new_block->next = split;
        new_block->size = aligned_size;
    }

    return (void *)(new_block + 1);
}

void free(void *ptr) {
    if (!ptr) return;

    user_block_t *block = (user_block_t *)ptr - 1;
    if (block->magic != BLOCK_MAGIC) {
        return;
    }

    block->free = 1;

    // Coalesce forward
    if (block->next && block->next->free && block->next->magic == BLOCK_MAGIC) {
        block->size += sizeof(user_block_t) + block->next->size;
        block->next = block->next->next;
        if (block->next) block->next->prev = block;
    }

    // Coalesce backward
    if (block->prev && block->prev->free && block->prev->magic == BLOCK_MAGIC) {
        user_block_t *prev = block->prev;
        prev->size += sizeof(user_block_t) + block->size;
        prev->next = block->next;
        if (block->next) block->next->prev = prev;
        block = prev;
    }
}

void *calloc(size_t count, size_t size) {
    if (size && count > (size_t)-1 / size) {
        errno = ENOMEM;
        return NULL;
    }

    size_t total = count * size;
    void *ptr = malloc(total);
    if (ptr) {
        memset(ptr, 0, total);
    }
    return ptr;
}

void *realloc(void *ptr, size_t new_size) {
    if (!ptr) return malloc(new_size);
    if (new_size == 0) {
        free(ptr);
        return NULL;
    }

    user_block_t *block = (user_block_t *)ptr - 1;
    if (block->magic != BLOCK_MAGIC) {
        errno = EINVAL;
        return NULL;
    }

    size_t aligned_size = ALIGN_UP(new_size);
    if (block->size >= aligned_size) {
        return ptr;
    }

    // Attempt in-place forward coalescing expansion
    if (block->next && block->next->free && block->next->magic == BLOCK_MAGIC &&
        (block->size + sizeof(user_block_t) + block->next->size >= aligned_size)) {
        size_t combined = block->size + sizeof(user_block_t) + block->next->size;
        block->next = block->next->next;
        if (block->next) block->next->prev = block;
        block->size = combined;

        if (block->size >= aligned_size + sizeof(user_block_t) + BLOCK_ALIGN) {
            user_block_t *split = (user_block_t *)((uint8_t *)(block + 1) + aligned_size);
            split->magic = BLOCK_MAGIC;
            split->size = block->size - aligned_size - sizeof(user_block_t);
            split->free = 1;
            split->next = block->next;
            split->prev = block;

            if (block->next) block->next->prev = split;
            block->next = split;
            block->size = aligned_size;
        }
        return ptr;
    }

    void *new_ptr = malloc(new_size);
    if (!new_ptr) return NULL;

    size_t copy_size = block->size < new_size ? block->size : new_size;
    memcpy(new_ptr, ptr, copy_size);
    free(ptr);
    return new_ptr;
}

void *reallocarray(void *ptr, size_t nmemb, size_t size) {
    if (size && nmemb > (size_t)-1 / size) {
        errno = ENOMEM;
        return NULL;
    }
    return realloc(ptr, nmemb * size);
}

size_t malloc_usable_size(void *ptr) {
    if (!ptr) return 0;
    user_block_t *block = (user_block_t *)ptr - 1;
    if (block->magic != BLOCK_MAGIC) return 0;
    return block->size;
}

uint32_t inet_addr(const char *ip_str) {
    uint32_t ip = 0;
    int octet = 0;
    int shift = 0;
    
    while (*ip_str) {
        if (*ip_str == '.') {
            ip |= (octet << shift);
            shift += 8;
            octet = 0;
        } else if (*ip_str >= '0' && *ip_str <= '9') {
            octet = octet * 10 + (*ip_str - '0');
        } else {
            return 0; // invalid format
        }
        ip_str++;
    }
    ip |= (octet << shift);
    return ip;
}