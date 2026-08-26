#include "../libs/meow_libc.h"

#define MODULE "REDIR"

DESCRIPTION("redir.elf: Test stdout redirection via dup2");

extern void log_trace(const char *module, const char *fmt, ...);

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    const char *outfile = "redirect.txt";
    log_trace(MODULE, "Beginning stdout redirection test to %s", outfile);

    printf("redir: Creating '%s'...\n", outfile);

    int fd = sys_create(outfile);
    if (fd < 0) {
        perror("redir: create failed");
        return 1;
    }

    log_trace(MODULE, "Created %s with FD=%d. Redirecting stdout...", outfile, fd);

    // Duplicate our file descriptor onto stdout (fd 1)
    if (dup2(fd, 1) < 0) {
        perror("redir: dup2 failed");
        close(fd);
        return 1;
    }

    close(fd);

    printf("[MeowMeowOS Redirection Test]\n");
    puts("Line written via puts() after dup2() call.");
    printf("Formatted integer output: %d (0x%X)\n", 1337, 1337);

    log_trace(MODULE, "Redirection test complete");
    return 0;
}