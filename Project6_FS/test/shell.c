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
    printf(">     @UCAS_OS: ");

    int whichline = SHELL_BEGIN+1;
    char nowpath[30];
    strcpy(nowpath, "root");

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
            sys_move_cursor(0, SHELL_BEGIN);
            printf("------------------- COMMAND -------------------\n");
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
        // TODO [P6-task1]: mkfs, statfs, cd, mkdir, rmdir, ls
        
        else if(strncmp(cmdbuf, "mkfs", 4) == 0){
            sys_mkfs();
        }
        
        else if(strncmp(cmdbuf, "statfs", 6) == 0){
            sys_statfs();
        }
        
        else if(strncmp(cmdbuf, "cd", 2) == 0){
            char path[60];
            if(cmdbuf[2] == '\0') path[0] = '\0';
            else {
                int ii = 3;
                while(cmdbuf[ii] != '\0'){
                    path[ii-3] = cmdbuf[ii];
                    ii++;
                }
                path[ii-3] = '\0';
            }
            int ok = sys_cd(path, 1);
            if(ok == 0){
                /*if(path[0] != '\0'){
                    if(path[0] == '.' && path[1] == '.'){
                        int m;
                        for(m = 0; m < strlen(path); m++){
                            if(path[m] == '/'){
                                path[m] = '\0';
                                break;
                            }
                        }
                        if(m == strlen(path)) path[0] = '\0';
                    }
                    else if(path[0] == '.'){
                        ;
                    }
                    if(strcmp(path, "root") == 0) path[0] = '\0';
                } 
                printf("now you are in: root/%s\n", path);
                whichline++;*/
            }
            else if(ok == -1){
                printf("ERROR: CAN'T FIND CURRENT PATH\n");
                whichline++;
            }
            else if(ok == -2){
                printf("ERROR: INVALID PATH\n");
                whichline++;
            }
        }
        
        else if(strncmp(cmdbuf, "mkdir", 5) == 0){
            char dirname[30];
            int ii = 5;
            int count = -1;
            while(cmdbuf[ii] != '\0' && cmdbuf[ii] == ' ') {count++; ii++;}
            if(cmdbuf[ii] == '\0') {printf("please enter the name of directory\n"); whichline++;}
            else{
                while(cmdbuf[ii] != '\0'){
                    dirname[ii-6-count] = cmdbuf[ii];
                    ii++;
                }
                dirname[ii-6-count] = '\0';
                int ok = sys_mkdir(dirname);
                if(ok == 0){
                    printf("mkdir succeed\n");
                    whichline++;
                }
                else if(ok == -1){
                    printf("ERROR: CAN'T FIND CURRENT PATH\n");
                    whichline++;
                }
                else if(ok == -2){
                    printf("ERROR: DIR %s ALREADY EXISTS\n", dirname);
                    whichline++;
                }
            }
        }
        
        else if(strncmp(cmdbuf, "rmdir", 5) == 0){
            char dirname[30];
            int ii = 5;
            int count = -1;
            while(cmdbuf[ii] == ' ') {count++; ii++;}
            if(cmdbuf[ii] == '\0') {printf("please enter the name of directory\n"); whichline++;}
            else{
                while(cmdbuf[ii] != '\0'){
                    dirname[ii-6-count] = cmdbuf[ii];
                    ii++;
                }
                dirname[ii-6-count] = '\0';
                int ok = sys_rmdir(dirname);
                if(ok == 0){
                    printf("rmdir succeed\n");
                    whichline++;
                }
                else if(ok == -1){
                    printf("ERROR: CAN'T FIND CURRENT DIR\n");
                    whichline++;
                }
                else if(ok == -2){
                    printf("ERROR: DIR %s DOESN'T EXIST\n", dirname);
                    whichline++;
                }
            }
        }
        
        else if(strncmp(cmdbuf, "ls", 2) == 0){
            int func = -1; //0 for default, 1 for -l, ...
            int ii = 2;
            while(cmdbuf[ii] == ' ') ii++;
            if(cmdbuf[ii] == '\0') func = 0;
            else if(cmdbuf[ii] == '-'){
                if(cmdbuf[ii+1] == 'l') func = 1;
                else{
                    printf("unknown arg: %c\n", cmdbuf[ii+1]);
                    whichline++;
                }
            }
            else{
                printf("please check input\n");
                whichline++;
            }
            if(func != -1){
                int ok = sys_ls(0, func);
                if(ok == -1){
                    printf("ERROR: CAN'T FIND CURRENT DIR\n");
                    whichline++;
                }
            }
        }
        
        // TODO [P6-task2]: touch, cat, ln, ls -l, rm
        else if(strncmp(cmdbuf, "touch", 5) == 0){
            char filename[30];
            int ii = 5;
            int count = -1;
            while(cmdbuf[ii] == ' ') {count++; ii++;}
            if(cmdbuf[ii] == '\0') {printf("please enter the name of file\n"); whichline++;}
            else{
                while(cmdbuf[ii] != '\0'){
                    filename[ii-6-count] = cmdbuf[ii];
                    ii++;
                }
                filename[ii-6-count] = '\0';
                int ok = sys_touch(filename);
                if(ok == 0){
                    printf("successed\n");
                    whichline++;
                }
                else if(ok == -1){
                    printf("ERROR: CAN'T FIND CURRENT DIR\n");
                    whichline++;
                }
                else if(ok == -2){
                    printf("ERROR: FILE %s ALREADY EXISTS\n");
                    whichline++;
                }
            }
        }
        
        else if(strncmp(cmdbuf, "cat", 3) == 0){
            char filename[30];
            int ii = 3;
            int count = -1;
            while(cmdbuf[ii] == ' ') {count++; ii++;}
            if(cmdbuf[ii] == '\0') {printf("please enter the name of file\n"); whichline++;}
            else{
                while(cmdbuf[ii] != '\0'){
                    filename[ii - 4 - count] = cmdbuf[ii];
                    ii++;
                }
                filename[ii - 4 - count] = '\0';
                int ok = sys_cat(filename);
                if(ok == 0);
                else if(ok == -1){
                    printf("ERROR: CAN'T FIND CURRENT DIR\n");
                    whichline++;
                }
                else if(ok == -2){
                    printf("ERROR: FILE %s DOENS'T EXIST IN CURRENT DIR\n", filename);
                    whichline++;
                }
            }
        }
        
        else if(strncmp(cmdbuf, "ln", 2) == 0){
            char src_path[30];
            char dst_path[30];
            int ii = 2;
            int count = -1;
            while(cmdbuf[ii] == ' ') {count++; ii++;}
            int count0 = count;
            if(cmdbuf[ii] == '\0') {printf("please check inputs\n"); whichline++;}
            else{
                while(cmdbuf[ii] != ' ' && cmdbuf[ii] != '\0'){
                    src_path[ii - 3 - count0] = cmdbuf[ii];
                    ii++;
                    count++;
                }
                src_path[ii - 3 - count0] = '\0';
                if(cmdbuf[ii] == '\0') {printf("please check input\n"); whichline++;}
                else{
                    while(cmdbuf[ii] == ' ') {count++; ii++;}
                    while(cmdbuf[ii] != '\0'){
                        dst_path[ii - 3 - count] = cmdbuf[ii];
                        ii++;
                    }            
                }
                dst_path[ii - 3 - count] = '\0';
                int ok = sys_ln(src_path, dst_path);
                if(ok == 0){
                    printf("ln succeed\n");
                    whichline++;
                }
                else if(ok == -1){
                    printf("CAN'T FIND THE FILE'S ANCDIR\n");
                    whichline++;
                }
                else if(ok == -2){
                    printf("NO TARGET FILE IN ANCDIR\n");
                    whichline++;
                }
                else if(ok == -3){
                    printf("CAN'T FIND THE DIR'S ANCDIR\n");
                    whichline++;
                }
                else if(ok == -4){
                    printf("NO SRC DIR IN ANCDIR\n");
                    whichline++;
                }
            }
        }
        
        else if(strncmp(cmdbuf, "rm ", 3) == 0){
            char path[30];
            int ii = 2;
            int count = -1;
            while(cmdbuf[ii] == ' ') {count++; ii++;}
            if(cmdbuf[ii] == '\0') {printf("please check inputs\n"); whichline++;}
            else{
                while(cmdbuf[ii] != ' ' && cmdbuf[ii] != '\0'){
                    path[ii - 3 - count] = cmdbuf[ii];
                    ii++;
                }
                path[ii - 3 - count] = '\0';
                int ok = sys_rm(path);
                if(ok == 0){
                    printf("rm succeeded\n");
                    whichline++;
                }
                else if(ok == -1){
                    printf("CAN'T FIND THE FILE'S ANCDIR\n");
                    whichline++;
                }
                else if(ok == -2){
                    printf("NO TARGET FILE IN ANCDIR\n");
                    whichline++;
                }
            }
        }
        
        else{
            printf("unknown command: %s\n", cmdbuf);
            whichline++;
        } //default
            
        
        whichline++;
        sys_move_cursor(0, whichline);
        printf(">     @UCAS_OS: ");
        
        if(whichline >= SHELL_END){
            sys_clear();
            whichline = SHELL_BEGIN + 1;
            sys_move_cursor(0, whichline);
            printf(">     @UCAS_OS: ");
        }
    }

    return 0;
}
