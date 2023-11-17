#include <os/ioremap.h>
#include <os/mm.h>
#include <pgtable.h>
#include <type.h>

// maybe you can map it to IO_ADDR_START ?
static uintptr_t io_base = IO_ADDR_START;

void *ioremap(unsigned long phys_addr, unsigned long size)
{
    // TODO: [p5-task1] map one specific physical region to virtual address
    uintptr_t va = io_base;
    uintptr_t vabuf = va;
    //what if phys_addr is not aligned?
    if(phys_addr % NORMAL_PAGE_SIZE){
        size += phys_addr % NORMAL_PAGE_SIZE;
        phys_addr -= phys_addr % NORMAL_PAGE_SIZE;    
    }
    int howmanypages = size / NORMAL_PAGE_SIZE;
    if(size % NORMAL_PAGE_SIZE) howmanypages++;
    
    for(int i = 0; i < howmanypages; i++){
        loader_helper_k(vabuf, phys_addr, 0xffffffc051000000lu);
        vabuf += NORMAL_PAGE_SIZE;
        phys_addr += NORMAL_PAGE_SIZE;
    }
    io_base += vabuf - va + NORMAL_PAGE_SIZE;

    return (void*)va;
}

void iounmap(void *io_addr)
{
    // TODO: [p5-task1] a very naive iounmap() is OK
    // maybe no one would call this function?
}
