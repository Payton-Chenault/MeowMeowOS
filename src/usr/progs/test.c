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

    sys_print("Closing test.elf\n");
    int close_result = sys_close(fd);

    if(close_result == 0) {
        sys_print("TEST SUCCESS: sys closed returned 0\n");
    } else {
        sys_print("TEST FAILED: sys closed returned error code\n");
    }
    
    sys_print("TEST: Attempting to read from closed FD (should fail)...\n");
    int post_close_read = sys_read(fd, buffer, 1);

    if (post_close_read == -1) {
        sys_print("TEST SUCCESS: Kernel rejected read from closed FD.\n");
    } else {
        sys_print("TEST FAILED: Kernel allowed read from an invalid FD!\n");
    }
    
    sys_print("\nAll VFS Lifecycle tests complete.\n");

    sys_exit();
}