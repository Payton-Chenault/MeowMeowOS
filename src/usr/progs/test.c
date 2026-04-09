#include "../libs/meow_libc.h"

void _start() {    
    int fd = sys_open("test.txt");
    
    if (fd == -1) {
        sys_print("Failed to open file.\n");
        sys_exit();
    }
    
    char* msg = "Hello from User Space!";
    
    sys_write(fd, msg, 22);
    
    sys_close(fd);
    sys_print("Successfully wrote to test.txt on the hard drive!\n");
    sys_exit();
}