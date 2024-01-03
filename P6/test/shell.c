/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *            Copyright (C) 2018 Institute of Computing Technology, CAS
 *               Author : Han Shukai (email : hanshukai@ict.ac.cn)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *                  The shell acts as a task running in user mode.
 *       The main function is to make system calls through the user's output.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * */

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#define SHELL_WIDTH 80
#define SHELL_BEGIN 15
#define SHELL_END 40
#define MAX_ARGC 5 

int getchar()
{
  int c;
  while ((c = sys_getchar()) == -1)
      ;
  return c;
}

int main(void){
    int print_location_x;
    int print_location_y;
    int usr_command_begin = strlen("> Sean@UCAS_OS:"); 
    char buff[200];
    int bufp; 
    int c;
    
    sys_clear(SHELL_BEGIN, SHELL_END);
    sys_move_cursor(0, SHELL_BEGIN);
    printf("---------- COMMAND -------------------\n"); 
    sys_move_cursor(0, SHELL_END);
    printf("--------------------------------------"); 
    
    print_location_y = SHELL_BEGIN+1; 
    sys_move_cursor(0,print_location_y); 
    
    while (1)
    {
        // TODO [P3-task1]: call syscall to read UART port 
        if(print_location_y == SHELL_END){
            print_location_y--;
            sys_move_cursor(0, print_location_y);
            sys_scroll(SHELL_BEGIN+1, SHELL_END-1);
        }
        
        printf("> Sean@UCAS_OS:");
        sys_get_cwdpath(buff);
        printf("%s$", buff); 
        bufp = 0;
        print_location_x = usr_command_begin + strlen(buff) + 1;
        
        while((c = getchar()) != 13 && c != '\n'){
        // TODO [P3-task1]: parse input
        // note: backspace maybe 8(' \b') or 127(delete)
            if(c == '\b' || c == 127){
                if(bufp> 0){
                    print_location_x--; 
                    bufp--;
                    sys_move_cursor(print_location_x,print_location_y); 
                    printf(" ");
                    sys_move_cursor(print_location_x, print_location_y);
                }
            }
            else{
                buff[bufp++] = (char)c; 
                print_location_x++; 
                printf("%c", (char)c);
            }
        }
        buff[bufp++]='\0';
        
        // TODO [P3-task1]: ps,exec, kill, clear 
        int argc = 0;
        char argv[MAX_ARGC][200]; 
        char *arg[200];
        int argvp = 0; 
        int in_argv = 0;
        
         for(int i = 0; ; i++){
            if(buff[i] == ' ' || buff[i] == '\0'){
                if(in_argv){
                    argv[argc][argvp++] = '\0';
                    argc++;
                    argvp = 0;
                }
                in_argv = 0;
                if(buff[i] == '\0'){
                    break;
                }
            }
            else{
                in_argv = 1;
                argv[argc][argvp++] = buff[i];
            }    
        }
        
        for (int i = 0; i < argc-1; i++){
            arg[i] = argv[i+1];
        }
        
        int empty_command   = (argc == 0);
        int ps_command      = !strcmp(argv[0], "ps"     );
        int clear_command   = !strcmp(argv[0], "clear"  );
        int exec_command    = !strcmp(argv[0], "exec"   );
        int kill_command    = !strcmp(argv[0], "kill"   );
        int taskset_command = !strcmp(argv[0], "taskset");
        int mkfs_command    = !strcmp(argv[0], "mkfs"   );
        int statfs_command  = !strcmp(argv[0], "statfs" );
        int cd_command      = !strcmp(argv[0], "cd"     );
        int mkdir_command   = !strcmp(argv[0], "mkdir"  );
        int rmdir_command   = !strcmp(argv[0], "rmdir"  );
        int ls_command      = !strcmp(argv[0], "ls"     );
        int touch_command   = !strcmp(argv[0], "touch"  );
        int cat_command     = !strcmp(argv[0], "cat"    );
        int rm_command      = !strcmp(argv[0], "rm"     );
        int ln_command      = !strcmp(argv[0], "ln"     );
        
        if(empty_command){
            continue;
        }
        else if(ps_command){
            if(argc > 1){
                if(print_location_y == 24){
                    print_location_y--;
                    sys_move_cursor(0, print_location_y);
                    sys_scroll(SHELL_BEGIN + 1, SHELL_END - 1);       
                }
                printf(" (QAQ)Too many arguments!\n");
                print_location_y++;
                continue;
            }
            char ps_context[(SHELL_END - SHELL_BEGIN - 1) * SHELL_WIDTH + 1];
            sys_ps(ps_context);

            char ps_buffer[SHELL_WIDTH + 1];
            for(int i = 0; ps_context[i]; i++){
                int j = 0;
                while(ps_context[i] != '\n' && ps_context[i]){
                    ps_buffer[j++] = ps_context[i++];
                }
                ps_buffer[j++] = '\n';
                ps_buffer[j++] = '\0';

                if(print_location_y == SHELL_END){
                    print_location_y--;
                    sys_move_cursor(0, print_location_y);
                    sys_scroll(SHELL_BEGIN + 1, SHELL_END - 1);
                }
                printf("%s", ps_buffer);
                print_location_y++;
            }
        }
        else if(clear_command){
            if(argc > 2){
                if(print_location_y == SHELL_END){
                    print_location_y--;
                    sys_move_cursor(0, print_location_y);
                    sys_scroll(SHELL_BEGIN + 1, SHELL_END - 1);       
                }
                printf(" (QAQ)Too many arguments!\n");
                print_location_y++;
                continue;
            }
            sys_clear(SHELL_BEGIN + 1, SHELL_END - 1);
            if (argc == 2 &&argv[1][1] == 'w'){
                sys_clear(0, SHELL_BEGIN - 1);
            }
            print_location_y = SHELL_BEGIN + 1;
            sys_move_cursor(0, print_location_y);
        }
        else if(exec_command){
            if(argc < 2){
                if(print_location_y == SHELL_END){
                    print_location_y--;
                    sys_move_cursor(0, print_location_y);
                    sys_scroll(SHELL_BEGIN + 1, SHELL_END - 1);       
                }
                printf(" (QAQ)Too few arguments!\n");
                print_location_y++;
                continue;
            }
            int pid = sys_exec(argv[1], argc-1, arg);
            printf(" (QAQ)exec process OK, pid = %d...\n", pid);
            print_location_y++;
            
            if (*arg[argc-1] != '&'){
                //sys_waitpid(pid);
            }
        }
        else if(kill_command){
            if(argc > 2){
                if(print_location_y == SHELL_END){
                    print_location_y--;
                    sys_move_cursor(0, print_location_y);
                    sys_scroll(SHELL_BEGIN + 1, SHELL_END - 1);       
                }
                printf(" (QAQ)Too many arguments!\n");
                print_location_y++;
                continue;
            }
            else if(argc < 2){
                if(print_location_y == SHELL_END){
                    print_location_y--;
                    sys_move_cursor(0, print_location_y);
                    sys_scroll(SHELL_BEGIN + 1, SHELL_END - 1);       
                }
                printf(" (QAQ)Too few arguments!\n");
                print_location_y++;
                continue;
            }
            
            int pid = atoi(argv[1]);
            if(pid == 0 || pid == 2){
                if(print_location_y == SHELL_END){
                    print_location_y--;
                    sys_move_cursor(0, print_location_y);
                    sys_scroll(SHELL_BEGIN + 1, SHELL_END - 1);       
                }
                printf(" (QAQ)Cannot kill task (pid=%d): Permission Denied!\n", pid);
                print_location_y++;
            }
            else if(!sys_kill(pid)){
                if(print_location_y == SHELL_END){
                    print_location_y--;
                    sys_move_cursor(0, print_location_y);
                    sys_scroll(SHELL_BEGIN + 1, SHELL_END - 1);       
                }
                printf(" (QAQ)Cannot kill task (pid=%d): Task Does Not Exist!\n", pid);
                print_location_y++;
            }
            else{
                if(print_location_y == SHELL_END){
                    print_location_y--;
                    sys_move_cursor(0, print_location_y);
                    sys_scroll(SHELL_BEGIN + 1, SHELL_END - 1);       
                }
                printf(" (QAQ)Task (pid=%d) killed\n", pid);
                print_location_y++;
            }
        }
        else if(taskset_command){
            int exec = strcmp(argv[1], "-p");
            
            if((exec && argc >= 4) || (!exec && argc >= 5)){
                printf(" (QAQ)Too many arguments!\n");
                print_location_y++;
                continue;
            }
            else if((exec && argc <=2) || (!exec && argc <=3)){
                printf(" (QAQ)Too few arguments!\n");
                print_location_y++;
                continue;
            }

            if(exec){
                unsigned long mask = atoi(argv[1]);
                pid_t pid = sys_exec(argv[2], 1, arg);
                
                sys_taskset(pid, mask);  
                printf(" (QAQ)exec process[%d] and set mask 0x%x\n", pid, mask);
                print_location_y++;
            }
            else{
                unsigned long mask = atoi(argv[2]);
                int pid = atoi(argv[3]);
                sys_taskset(pid, mask);
                printf(" (QAQ)have set mask[%x] of task(pid=%d)\n", mask, pid);
                print_location_y++;
            }
        }
        else if (mkfs_command){
            sys_mkfs();
            sys_move_cursor(print_location_x, print_location_y);
            printf(" (QAQ)Make filesystem succeeded!\n");
            print_location_y++;
        }
        else if (statfs_command){
            printf("\n");
            print_location_y++;
            
            char statfs_context[(SHELL_END - SHELL_BEGIN - 1) * SHELL_WIDTH + 1];
            sys_statfs(statfs_context);

            char statfs_buffer[SHELL_WIDTH + 1];
            for(int i = 0; statfs_context[i]; i++){
                int j = 0;
                while(statfs_context[i] != '\n' && statfs_context[i]){
                    statfs_buffer[j++] = statfs_context[i++];
                }
                statfs_buffer[j++] = '\n';
                statfs_buffer[j++] = '\0';
                printf("%s", statfs_buffer);
                print_location_y++;
            }
        }
        else if (cd_command){
            int done;
            if (argc == 1){
                printf(" (QAQ)Too few arguments");
            }
            else{
                done = sys_enter_fs(arg[0]);
                if (done == -1){
                    printf(" (QAQ)No such file or directory");
                }
            }
            printf("\n");
            print_location_y++;
        }
        else if (mkdir_command){
            int done;
            if (argc == 1){
                done = sys_mkdir(0);
            }
            else{
                done = sys_mkdir(arg[0]);
            }
            if (done == -1){
                printf(" (QAQ)No such file or directory");
            }
            printf("\n");
            print_location_y++;
        }
        else if (rmdir_command || rm_command){
            int done;
            if (argc == 1){
                done = sys_rmdir(0);
            }
            else{
                done = sys_rmdir(arg[0]);
            }
            if (done == -1){
                printf(" (QAQ)No such file or directory");
            }
            printf("\n");
            print_location_y++;
        }
        else if (ls_command){
            char ls_context[(SHELL_END - SHELL_BEGIN - 1) * SHELL_WIDTH + 1];
            int done;
            if (argc == 1){
                done = sys_ls(0, ls_context, 0);
                if (done == -1){
                    printf(" (QAQ)No such file or directory\n");
                    print_location_y++;
                    continue;
                }
                printf("\n");
                print_location_y++;
                int i = 0;
                while (ls_context[i] != '\0'){
                    printf("%c", ls_context[i]);
                    i++;
                }
                printf("\n");
                print_location_y++;
            }
            else{
                int arg0_l = !strcmp(arg[0], "-l");
                int arg1_l = !strcmp(arg[1], "-l");
                if (arg0_l){
                    if (argc == 2)
                        done = sys_ls(0, ls_context, 1);
                    else
                        done = sys_ls(arg[1], ls_context, 1);
                }
                else{
                    done = sys_ls(arg[0], ls_context, arg0_l || arg1_l);
                }
                if (done == -1){
                    printf(" (QAQ)No such file or directory\n");
                    print_location_y++;
                    continue;
                }
                printf("\n");
                print_location_y++;
                int i = 0;
                while (ls_context[i] != '\0'){
                    printf("%c", ls_context[i]);
                    if (ls_context[i] == '\n')
                        print_location_y++;
                    i++;
                }
                if ((arg0_l || arg1_l) == 0){
                    printf("\n");
                    print_location_y++;
                }
            }
        }
        else if (touch_command){
            int done;
            if (argc == 1){
                done = sys_create(0);
            }
            else{
                done = sys_create(arg[0]);
            }
            if (done == -1){
                printf(" (QAQ)No such file or directory");
            }
            printf("\n");
            print_location_y++;
        }
        else if (ln_command){
            int done;
            if (argc != 3){
                printf("command format: ln source target");
            }
            else{
                done = sys_ln(arg[0], arg[1]);
            }
            if (done == -1){
                printf(" (QAQ)No such file or directory");
            }
            printf("\n");
            print_location_y++;
        }
        else if (cat_command){
            int done;
            if (argc == 1){
                done = sys_cat(0);
            }
            else{
                done = sys_cat(arg[0]);
            }
            sys_move_cursor(print_location_x, print_location_y);
            if (done == -1){
                printf(" (QAQ)No such file or directory");
            }
            printf("\n");
            print_location_y++;
        }
        else{
            if(print_location_y == SHELL_END){
                print_location_y--;
                sys_move_cursor(0, print_location_y);
                sys_scroll(SHELL_BEGIN + 1, SHELL_END - 1);       
            }
            printf(" (QAQ)Unknown Command: ");
            printf("%s\n", argv[0]);
            print_location_y++;
        }
    }
}