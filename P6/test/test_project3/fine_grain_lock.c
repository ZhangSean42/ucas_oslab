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
    int lock_handle = sys_mutex_init(LOCK_KEY);

    clock_t Begin = clock();
    // Try to acquire and release mutex lock for 10000 times
    for (int i = 0; i < 10000; i++){
        sys_mutex_acquire(lock_handle);
        sys_mutex_release(lock_handle);
    }
    clock_t End = clock();
    sys_move_cursor(0, print_location);
    printf("fine_grain_lock total ticks: %ld ticks\n\r", End - Begin);

    return 0;
}
