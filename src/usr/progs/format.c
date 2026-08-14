#include "../libs/meow_libc.h"

int _start(int argc, char** argv) {
    (void)argc; (void)argv;
    char confirm;
    
    sys_print("Are you sure you want to format the drive? This will erase all data. <y/N>: ");
    sys_read(0, &confirm, 1);
    sys_write(1, "\n", 1);

    if (confirm == 'y' || confirm == 'Y') {
        sys_format();
    }

    sys_exit();
    return 0;
}