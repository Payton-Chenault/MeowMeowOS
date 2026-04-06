#include "itoa.h"

void iota(int n, char *str) {
    int i = 0;
    bool is_negitive = false;

    if (n==0) {
        str[i++] = '0';
        str[i] = '\0';
        return;
    }

    if (n < 0) {
        is_negitive = true;
        n = -n;
    }

    while (n != 0) {
        int rem = n % 10;
        str[i++] = rem + '0';
        n = n / 10;
    }

    if (is_negitive) {
        str[i++] = '-';
    }

    str[i] = '\0';

    int start = 0;
    int end = i - 1;
    while (start < end) {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        start++;
        end--;
    }
}