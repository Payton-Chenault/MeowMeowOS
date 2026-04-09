#include "../libs/meow_libc.h"

void _start() {    
    int fd = sys_open("test.elf");
    
    if (fd == -1) {
        sys_print("Failed to open file. Does it exist?\n");
        sys_exit();
    }
    
    sys_print("Success! File Descriptor granted.\n");
    sys_print("Reading the first 4 bytes of the file...\n\n");

    char buffer[5];
    
    int bytes_read = sys_read(fd, buffer, 4);
    
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        
        sys_print("--- RAW DISK DATA ---\n");
        sys_print(buffer);
        sys_print("\n---------------------\n");
    } else {
        sys_print("File was empty or read failed.\n");
    }
    
    sys_exit();
}