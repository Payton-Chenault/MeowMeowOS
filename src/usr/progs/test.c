#include "../libs/meow_libc.h"
 
void _start() {
    sys_print("Welcome to the MeowMeowOS Interactive Utility!\n");
    sys_print("Press any letter on your keyboard...\n\n");
   
    char key = sys_read_char(); 
    
    sys_print("You pressed the letter: ");
    sys_print_char(key);
    sys_print("\n\nGoodbye!\n");
    
    sys_exit();
}