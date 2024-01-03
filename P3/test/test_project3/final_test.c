#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#define BUF_LEN 10

int main(int argc, char * argv[])
{
    assert(argc >= 1);
    int print_location = (argc == 1) ? 0 : atoi(argv[1]);

    pid_t pid;

    char buf_location[BUF_LEN];
    char cpu_affinity[BUF_LEN];
  
    sys_move_cursor(0, print_location);
    printf("final_test begins!\n");
    // Start fly
    assert(itoa(print_location + 2, buf_location, BUF_LEN, 10) != -1);
    char *argv_fly[2] = {"fly", buf_location};
    sys_exec(argv_fly[0], 2, argv_fly);

    // Start fine_grain_lock on cpu0
    assert(itoa(print_location + 0, buf_location, BUF_LEN, 10) != -1);
    assert(itoa(1, cpu_affinity, BUF_LEN, 10) != -1);
    char *argv_fine_grain_lock_0[3] = {"fine_grain_lock", buf_location, cpu_affinity};
    pid = sys_exec(argv_fine_grain_lock_0[0], 3, argv_fine_grain_lock_0);

    // Wait fine_grain_lock processes to exit
    sys_waitpid(pid);
    
    // Start fine_grain_lock on cpu1
    assert(itoa(print_location + 1, buf_location, BUF_LEN, 10) != -1);
    assert(itoa(2, cpu_affinity, BUF_LEN, 10) != -1);
    char *argv_fine_grain_lock_1[3] = {"fine_grain_lock", buf_location, cpu_affinity};
    pid = sys_exec(argv_fine_grain_lock_1[0], 3, argv_fine_grain_lock_1);
    
    // Wait fine_grain_lock processes to exit
    sys_waitpid(pid);

    return 0;    
}