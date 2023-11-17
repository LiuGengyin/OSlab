#include <os/task.h>
#include <os/string.h>
#include <os/kernel.h>
#include <type.h>
#include <os/mm.h>
#include <pgtable.h>

#define SECTOR_SIZE 512
#define NBYTES2SEC(nbytes) (((nbytes) / SECTOR_SIZE) + ((nbytes) % SECTOR_SIZE != 0))

task_info_t taskss[TASK_MAXNUM];
uint64_t load_task_img(char *taskname, uint64_t* pgdir, uintptr_t user_sp)
{
    /**
     * TODO:
     * 1. [p1-task3] load task from image via task id, and return its entrypoint
     * 2. [p1-task4] load task via task name, thus the arg should be 'char *taskname'
     */
     
     int st_addr = *((int*)0xffffffc0502001f8);
     short size_of_info = TASK_MAXNUM * sizeof(task_info_t);
     int howmanysec = NBYTES2SEC(((st_addr % SECTOR_SIZE) + size_of_info));
     
     uintptr_t buff = 0x51ffff00 - 2*SECTOR_SIZE - size_of_info; //跳转表之前的某一处，把taskinfo暂存在这里
     bios_sdread((unsigned)buff, (unsigned)howmanysec, (unsigned)(st_addr / SECTOR_SIZE));
     
     char* here = (char*)(buff + 0xffffffc000000000lu + (st_addr % SECTOR_SIZE)-1);
     char* dest = (char*)taskss;
     
     memcpy(dest, here, (int)size_of_info);
     
     int tasksindex = 0;
     while(strcmp(taskname, taskss[tasksindex].name) != 0) tasksindex++;
     
     if(NBYTES2SEC(taskss[tasksindex].offset_from_this_sector + taskss[tasksindex].memsz) > 64) printl("sdread panic\n");
     
     uintptr_t task_start_buf = buff - 2*SECTOR_SIZE - taskss[tasksindex].size_of_task; //跳转表之前的某一处，把任务的LOAD字段暂存在这里，最后copy到vaddr处
     bios_sdread((unsigned)task_start_buf, (unsigned)NBYTES2SEC(taskss[tasksindex].offset_from_this_sector + taskss[tasksindex].size_of_task), (unsigned)taskss[tasksindex].start_from_this_sector);
     
     //设置用户态页表
     uint64_t vstart = taskss[tasksindex].vaddr;
     int needpage = (taskss[tasksindex].memsz - (taskss[tasksindex].memsz % 4096)) / 4096 + 1;
     uint64_t kernvstart;
     uint64_t pagess[30];
     for(int i = needpage - 1; i >= 0; i--)
         pagess[i] = kmalloc(); //这些页用于存放用户LOAD字段
     kernvstart = pagess[0];
     
     for(int i = 0; i < needpage; i++){
         //uintptr_t thispage = kmalloc();
         //if(i == needpage - 1) kernvstart = thispage;
         uint64_t va = vstart + 4096 * i; //接下来把va开始的一虚页对应到kva2pa(pagess[i])。
         uint64_t pa = kva2pa(pagess[i]);
         loader_helper(va, pa, pgdir);
     }
     
     //映射用户栈的那一页
     uint64_t vuser_sp = 0xf00010000lu;
     loader_helper(vuser_sp, kva2pa(user_sp), pgdir);
      
     //最后把LOAD字段copy到vaddr处
     memcpy((uint8_t*)kernvstart, (uint8_t*)(task_start_buf + 0xffffffc000000000 + taskss[tasksindex].offset_from_this_sector), taskss[tasksindex].size_of_task);

     return vstart;
}