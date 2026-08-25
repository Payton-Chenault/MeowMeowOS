#include "../libs/meow_libc.h"

DESCRIPTION("date.elf: Display current system date and time");

static void print_padded(unsigned int val, int digits) {
    char buf[16];
    itoa((int)val, buf, 10);
    int len = strlen(buf);
    if (len < digits) {
        for (int i = 0; i < digits - len; i++) {
            putchar('0');
        }
    }
    printf("%s", buf);
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    sys_time_t t;
    if (sys_get_time(&t) != 0) {
        printf("date: failed to read system time\n");
        return 1;
    }

    print_padded(t.year, 4);
    putchar('-');
    print_padded(t.month, 2);
    putchar('-');
    print_padded(t.day, 2);
    putchar(' ');
    print_padded(t.hour, 2);
    putchar(':');
    print_padded(t.minute, 2);
    putchar(':');
    print_padded(t.second, 2);
    putchar('\n');

    return 0;
}