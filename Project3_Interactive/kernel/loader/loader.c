#include <os/task.h>
#include <os/string.h>
#include <os/kernel.h>
#include <type.h>

#define SECTOR_SIZE 512
#define NBYTES2SEC(nbytes) (((nbytes) / SECTOR_SIZE) + ((nbytes) % SECTOR_SIZE != 0))

task_info_t taskss[TASK_MAXNUM];
uint64_t load_task_img(const char* taskname)
{
    /**
     * TODO:
     * 1. [p1-task3] load task from image via task id, and return its entrypoint
     * 2. [p1-task4] load task via task name, thus the arg should be 'char *taskname'
     */
     
     uint64_t entrypoint;
     int st_addr = *((int*)0x502001f8);
     short size_of_info = TASK_MAXNUM * sizeof(task_info_t);
     int howmanysec = NBYTES2SEC(((st_addr % SECTOR_SIZE) + size_of_info));
     
     bios_sdread((unsigned)0x52055000, (unsigned)howmanysec, (unsigned)(st_addr / SECTOR_SIZE));//put task_infos here
     
     char* here = (char*)(0x52055000 + (st_addr % SECTOR_SIZE)-1);
     char* dest = (char*)taskss;
     
     memcpy(dest, here, (int)size_of_info);
     
     int tasksindex = 0;
     while(strcmp(taskname, taskss[tasksindex].name) != 0) tasksindex++;
     //bios_putstr("\n");
     //bios_putstr("name loaded is: "); bios_putstr(taskss[tasksindex].name); bios_putchar('\n');
     bios_sdread((unsigned)0x52160000, (unsigned)NBYTES2SEC(taskss[tasksindex].offset_from_this_sector + taskss[tasksindex].size_of_task), (unsigned)taskss[tasksindex].start_from_this_sector);//the task comments membuffer:0x52160000
     if(strcmp(taskname, "shell") == 0)
       entrypoint = 0x52000000;
     else if(strcmp(taskname, "add") == 0)
       entrypoint = 0x520a0000;
     else if(strcmp(taskname, "affinity") == 0)
       entrypoint = 0x52050000;
     else if(strcmp(taskname, "barrier") == 0)
       entrypoint = 0x520d0000;
     else if(strcmp(taskname, "condition") == 0)
       entrypoint = 0x52090000;
     else if(strcmp(taskname, "consumer") == 0)
       entrypoint = 0x52040000;
     else if(strcmp(taskname, "mbox_client") == 0)
       entrypoint = 0x52010000;
     else if(strcmp(taskname, "mbox_server") == 0)
       entrypoint = 0x52080000;
     else if(strcmp(taskname, "multicore") == 0)
       entrypoint = 0x52020000;
     else if(strcmp(taskname, "producer") == 0)
       entrypoint = 0x520c0000;
     else if(strcmp(taskname, "ready_to_exit") == 0)
       entrypoint = 0x52070000;
     else if(strcmp(taskname, "test_affinity") == 0)
       entrypoint = 0x520e0000;
     else if(strcmp(taskname, "test_barrier") == 0)
       entrypoint = 0x52030000;
     else if(strcmp(taskname, "wait_locks") == 0)
       entrypoint = 0x520b0000;
     else if(strcmp(taskname, "waitpid") == 0)
       entrypoint = 0x52030000;
     else{
         bios_putstr("ERROR: INVALID PROCESS NAME -- EXEC FAILED. PLEASE CLEAR THE SHELL\n");
         return 0;
     }
       
     memcpy((uint8_t*)entrypoint, (uint8_t*)(0x52160000 + taskss[tasksindex].offset_from_this_sector), taskss[tasksindex].size_of_task);

     return entrypoint;
}