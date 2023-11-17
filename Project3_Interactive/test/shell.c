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

#define SHELL_BEGIN 20
#define SHELL_END 31

int main(void)
{
    sys_move_cursor(0, SHELL_BEGIN);
    printf("------------------- COMMAND -------------------\n");
    printf("> root@UCAS_OS: ");

    int whichline = SHELL_BEGIN+1;

    while (1)
    {
        // TODO [P3-task1]: call syscall to read UART port
        
        int ch = 0;
        int current_cursor = 15;
        char cmdbuf[100];
        int bufindex = 0;
        while(1){
            while((ch = sys_getchar()) == -1);
            current_cursor++;
            if(ch == '\r') {printf("\n"); break;}
            if(ch == '\b'){
                current_cursor--;
                if(bufindex > 0){
                    sys_move_cursor(current_cursor, whichline);
                    printf(" ");
                    sys_move_cursor(current_cursor, whichline);
                    current_cursor--;
                    bufindex--;
                }
            }
            else{
                printf("%c", ch);
                cmdbuf[bufindex++] = (char)ch;
            }
        }
        cmdbuf[bufindex] = '\0';
        
        // TODO [P3-task1]: parse input
        // note: backspace maybe 8('\b') or 127(delete)

        // TODO [P3-task1]: ps, exec, kill, clear 
        
        if(strncmp(cmdbuf, "clear", 5) == 0){
            sys_clear();
            whichline = SHELL_BEGIN;
        } //clear
        
        else if(strncmp(cmdbuf, "ps", 2) == 0)
            whichline += sys_ps(whichline);
         //ps
        else if(strncmp(cmdbuf, "exec", 4) == 0){ 
            if(strlen(cmdbuf) <= 5){
                printf("ENTER THE TASK'S NAME -- EXEC FAILED\n");
                whichline++;
            }
            
            else{
                char name[30];
                int argc = 0;
                char argv[10][30];
            
                int ii = 5;
                
                while(1){
                    int j = 0;
                    for(; cmdbuf[ii] != ' ' && cmdbuf[ii] != '&' && cmdbuf[ii] != '\0'; ii++){
                        argv[argc][j++] = cmdbuf[ii];
                    }
                    if(cmdbuf[ii] == '&') break;
                    argv[argc][j] = '\0';
                    argc++;
                    if(cmdbuf[ii] == '\0') break;
                    ii++;
                }
            
                strcpy(name, argv[0]);
                //printf("%s\n%s\n", argv[0], argv[1]);
                //printf("%p\n", argv);
                char* argvv[10];
                for(int i = 0; i < 10; i++)
                    argvv[i] = argv[i];
                pid_t thispid = sys_exec(name, argc, argvv);
                printf("execute %s successfully, pid is %d\n", name, thispid);
                whichline++;
                if(cmdbuf[ii] != '&') sys_waitpid(thispid);
            }
        } //exec
        
        else if(strncmp(cmdbuf, "kill", 4) == 0){
            int ii = 4;
            while(cmdbuf[ii] == ' ') ii++;
            int i = ii;
            while(isdigit(cmdbuf[i])) i++;
            int len = i - ii;
            if((len == 0) && (cmdbuf[ii] != '-')){
                printf("ENTER PID -- KILL FAILED\n");
                whichline++;
            }
            else if(len == 0){
                printf("PLEASE ENTER POSITIVE PID -- KILL FAILED\n");
                whichline++;
            }
            else{
                int temp = 0;
                for(int j = 0; j < len; j++){
                    temp *= 10;
                    temp += (cmdbuf[ii+j] - '0');
                }
                
                int res = sys_kill(temp);
                if(res == 0){
                    printf("PROCESS WITH PID %d DO NOT EXIST -- KILL FAILED\n", temp);
                    whichline++;
                }
                else if(res == -1){
                    printf("YOU CAN'T KILL KERNEL -- KILL FAILED\n");
                    whichline++;
                }
                else if(res == -2){
                    printf("YOU CAN'T KILL YOURSELF -- KILL FAILED\n");
                    whichline++;
                }
                else{
                    printf("killed successfully, pid = %d\n", temp);
                    whichline++;
                }
            }
        } //kill
        
        else{
            printf("unknown command: %s\n", cmdbuf);
            whichline++;
        } //default
            
        
        whichline++;
        sys_move_cursor(0, whichline);
        printf("> root@UCAS_OS: ");
        
        if(whichline >= SHELL_END){
            sys_clear();
            whichline = SHELL_BEGIN + 1;
            sys_move_cursor(0, whichline);
            printf("> root@UCAS_OS: ");
        }
    }

    return 0;
}
