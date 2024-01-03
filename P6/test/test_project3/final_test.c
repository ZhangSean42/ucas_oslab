#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#define BUF_LEN 10

int main(int argc, char * argv[])
{
    assert(argc >= 1);
    int print_location = (argc == 1) ? 0 : atoi(argv[1]);

    // Launch 
    pid_t pid;

    char buf_location[BUF_LEN];
  
    sys_move_cursor(0, print_location);
    printf("final_test begins!\n");
    // Start fly
    assert(itoa(print_location + 2, buf_location, BUF_LEN, 10) != -1);
    char *argv_fly[2] = {"fly", buf_location};
    sys_exec(argv_fly[0], 2, argv_fly);

    // Start fine_grain_lock
    assert(itoa(print_location + 1, buf_location, BUF_LEN, 10) != -1);
    char *argv_fine_grain_lock[2] = {"fine_grain_lock", buf_location};
    pid = sys_exec(argv_fine_grain_lock[0], 2, argv_fine_grain_lock);

    // Wait fine_grain_lock processes to exit
    sys_waitpid(pid);

    return 0;    
}
