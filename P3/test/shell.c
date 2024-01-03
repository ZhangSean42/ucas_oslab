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
#define SHELL_BEGIN 10
#define SHELL_END 35
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
    int usr_command_begin = strlen("> root@UCAS_OS: "); 
    char buff[200]; 
    int bufp; 
    int c;
    
    sys_clear(0, SHELL_END);
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
        
        printf("> Sean@UCAS_OS: "); 
        bufp = 0;
        print_location_x = usr_command_begin;
        
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
        
        int empty_command = (argc == 0);
        int ps_command = !strcmp(argv[0], "ps");
        int clear_command = !strcmp(argv[0], "clear");
        int exec_command = !strcmp(argv[0], "exec");
        int kill_command = !strcmp(argv[0], "kill");
        int taskset_command = !strcmp(argv[0], "taskset");
        
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
            if(argc > 1){
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
            print_location_y = SHELL_BEGIN + 1;
            sys_move_cursor(0, print_location_y);
        }
        else if(exec_command){
            if(argc > 3){
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
            int pid = sys_exec(argv[1], 1, arg);
            printf(" (QAQ)exec process OK, pid = %d...\n", pid);
            print_location_y++;
            
            if (argc == 2 || (*arg[1]) != '&'){
                sys_waitpid(pid);
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
    /*sys_move_cursor(0, SHELL_BEGIN);
    printf("---------- COMMAND -------------------\n"); 
    while(1)
    ;*/
}