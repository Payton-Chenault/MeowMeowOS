#include "meow_libc.h"

int opterr = 1;
int optind = 1;
int optopt = 0;
int optreset = 0;
char *optarg = NULL;

static int optpos = 1;

int getopt(int argc, char * const argv[], const char *opts)
{
    if (optreset || optind == 0) {
        optind = 1;
        optpos = 1;
        optreset = 0;
    }
    optarg = NULL;

    if (optind >= argc || argv[optind][0] != '-')
        return -1;

    char c = argv[optind][optpos];
    if (c == '\0') {
        optind++;
        optpos = 1;
        return getopt(argc, argv, opts);
    }

    const char *p = strchr(opts, c);
    if (p == NULL) {
        optopt = c;
        if (opterr) {
            write(2, "Unknown option: -", 17);
            write(2, &c, 1);
            write(2, "\n", 1);
        }
        optpos++;
        if (argv[optind][optpos] == '\0') {
            optind++;
            optpos = 1;
        }
        return '?';
    }

    if (*(p+1) == ':') {
        if (argv[optind][optpos+1] != '\0') {
            optarg = &argv[optind][optpos+1];
            optind++;
            optpos = 1;
        } else if (optind+1 < argc) {
            optarg = argv[optind+1];
            optind += 2;
            optpos = 1;
        } else {
            optopt = c;
            if (opterr) {
                write(2, "Option -", 8);
                write(2, &c, 1);
                write(2, " requires an argument\n", 22);
            }
            optind++;
            optpos = 1;
            return ':';
        }
    } else {
        optpos++;
        if (argv[optind][optpos] == '\0') {
            optind++;
            optpos = 1;
        }
    }
    return c;
}