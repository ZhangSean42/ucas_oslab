#include <stdio.h>
#include <unistd.h>

pid_t add_num(int num)
{
    pid_t pid;
    if ((pid = sys_fork())){
        sys_move_cursor(0, 0);
        printf("This is from father[pid=%d]: ", sys_getpid());
        printf("num = %d", num);
        sys_waitpid(pid);
    }
    else{
        sys_move_cursor(0, 1);
        num++;
        printf("This is from son[pid=%d]: ", sys_getpid());
        printf("num = %d", num);
    }
    return pid;
}

int main()
{
    char *byebye = "Byebye, world\n";
    pid_t pid;
    int num = 0;
    
    pid = add_num(num);
    if (pid){
        sys_move_cursor(0, 2);
        printf("This is from father[pid=%d]: ", sys_getpid());
        printf("%s", byebye);
    }
    else{
        sys_move_cursor(0, 3);
        printf("This is from son[pid=%d]: ", sys_getpid());
        printf("%s", byebye);
    }
    return 0;
}