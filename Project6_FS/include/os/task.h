#ifndef __INCLUDE_TASK_H__
#define __INCLUDE_TASK_H__

#include <type.h>

#define TASK_MEM_BASE    0x52000000
#define TASK_MAXNUM      16
#define TASK_SIZE        0x10000

/* TODO: [p1-task4] implement your own task_info_t! */
typedef struct {
    char name[20];
    int offset_from_this_sector;
    int size_of_task;
    int start_from_this_sector;
    long vaddr;
    int memsz;
} task_info_t;

extern task_info_t tasks[TASK_MAXNUM];

#endif