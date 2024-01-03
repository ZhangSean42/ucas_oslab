#include<stdio.h>
#include<unistd.h>
void thread_add(int);
int num_yield;
int a[4];
int print_location[4] = {6, 7, 7, 8};
void main(){
    for (int thread = 0; thread < 4; thread++)
        sys_thread_create(thread_add, thread);
    while (1){
        sys_move_cursor(0, 8);
        printf("> [THREAD] this is from main thread, total yield num = %d\n", num_yield);
    }
}

void thread_add(int thread_rank)
{   
    if (thread_rank == 0 || thread_rank == 2){
        while (1){
            a[thread_rank]++;
            if (a[thread_rank] >= a[(thread_rank+2) % 4] + 1){
                sys_move_cursor(0, print_location[thread_rank]);
                printf("> [THREAD] this is from thread_%d, value = %d\n", thread_rank, a[thread_rank]);
                num_yield++;
                sys_thread_yield();
            }
        }
    }
    while (1)
    ;
}