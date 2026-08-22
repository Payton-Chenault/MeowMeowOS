#include "meow_libc.h"

int errno = 0;

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

typedef struct block {
    size_t size;
    int free;
    struct block *next;
} block_t;

static block_t *free_list = NULL;

#define PAGE_SIZE 4096
#define BLOCK_ALIGN 8
#define align_up(x) (((x) + BLOCK_ALIGN - 1) & ~(BLOCK_ALIGN - 1))

static block_t *request_space(size_t size) {
    (void)size;

    void *page = sys_alloc_page();
    if (!page) return NULL;

    block_t *blk = (block_t *)page;
    blk->size = PAGE_SIZE - sizeof(block_t);
    blk->free = 1;
    blk->next = free_list;
    free_list = blk;

    return blk;
}

void *malloc(size_t size) {
    if (size == 0) return NULL;
    size = align_up(size);

    block_t *curr = free_list;
    while (curr) {
        if (curr->free && curr->size >= size) {
            if (curr->size >= size + sizeof(block_t) + 16) {
                block_t *newblk = (block_t *)((char *)(curr + 1) + size);
                newblk->size = curr->size - size - sizeof(block_t);
                newblk->free = 1;
                newblk->next = curr->next;
                curr->size = size;
                curr->next = newblk;
            }

            curr->free = 0;
            return (void *)(curr + 1);
        }

        curr = curr->next;
    }

    block_t *blk = request_space(size);
    if (!blk) {
        errno = ENOMEM;
        return NULL;
    }

    if (blk->size >= size) {
        if (blk->size >= size + sizeof(block_t) + 16) {
            block_t *newblk = (block_t *)((char *)(blk + 1) + size);
            newblk->size = blk->size - size - sizeof(block_t);
            newblk->free = 1;
            newblk->next = blk->next;
            blk->size = size;
            blk->next = newblk;
        }

        blk->free = 0;
        return (void *)(blk + 1);
    }

    errno = ENOMEM;
    return NULL;
}

void free(void *ptr) {
    if (!ptr) return;

    block_t *blk = (block_t *)ptr - 1;
    blk->free = 1;

    // Coalesce with next
    if (blk->next && blk->next->free) {
        blk->size += sizeof(block_t) + blk->next->size;
        blk->next = blk->next->next;
    }

    // Coalesce with previous
    block_t *prev = NULL;
    block_t *curr = free_list;
    while (curr && curr != blk) {
        prev = curr;
        curr = curr->next;
    }

    if (prev && prev->free) {
        prev->size += sizeof(block_t) + blk->size;
        prev->next = blk->next;
    }
}

void *calloc(size_t count, size_t size) {
    size_t total = count * size;
    if (size && total / size != count) {
        errno = ENOMEM;
        return NULL;
    }

    void *ptr = malloc(total);
    if (ptr) memset(ptr, 0, total);
    return ptr;
}

void *realloc(void *ptr, size_t new_size) {
    if (!ptr) return malloc(new_size);
    if (new_size == 0) {
        free(ptr);
        return NULL;
    }

    block_t *blk = (block_t *)ptr - 1;
    if (blk->size >= new_size) {
        return ptr;
    }

    void *new_ptr = malloc(new_size);
    if (!new_ptr) return NULL;

    memcpy(new_ptr, ptr, blk->size);
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
    block_t *blk = (block_t *)ptr - 1;
    return blk->size;
}