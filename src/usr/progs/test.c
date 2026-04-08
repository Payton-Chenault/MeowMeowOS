#include "../libs/meow_libc.h"

int main() {
    sys_print("Hello from an external ELF program!\n");
    sys_print("I am going to yield the CPU now...\n");
    
    sys_yield(); 
    
    sys_print("I am back! The OS gave me CPU time again.\n");
    
    return 0; // This will return to your task_exit() cleanup function!
}