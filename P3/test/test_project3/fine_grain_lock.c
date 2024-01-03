#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>
#define LOCK_KEY 23

int main(int argc, char *argv[])
{
    assert(argc >= 1);
    int print_location = (argc == 1) ? 0 : atoi(argv[1]);
    int cpu_affinity = (argc == 1) ? 0 : atoi(argv[2]);
    int lock_handle = sys_mutex_init(LOCK_KEY+cpu_affinity);
    pid_t pid = sys_getpid();

    sys_taskset(pid, cpu_affinity);
    clock_t Begin = clock();
    // Try to acquire and release mutex lock for 10000 times
    for (int i = 0; i < 10000; i++){
        sys_mutex_acquire(lock_handle);
        sys_mutex_release(lock_handle);
    }
    clock_t End = clock();
    sys_move_cursor(0, print_location);
    printf("fine_grain_lock total ticks with affinity%d: %ld ticks\n\r",cpu_affinity, End - Begin);

    return 0;
}