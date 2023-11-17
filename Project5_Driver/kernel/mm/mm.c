#include <os/mm.h>

// NOTE: A/C-core
static ptr_t kernMemCurr = FREEMEM_KERNEL;

struct run{
    struct run* next;
};

struct run* freelist;

short startsec;

ptr_t allocPage(int numPage)
{
    // align PAGE_SIZE
    ptr_t ret = ROUND(kernMemCurr, PAGE_SIZE);
    kernMemCurr = ret + numPage * PAGE_SIZE;
    return ret;
}

// NOTE: Only need for S-core to alloc 2MB large page
#ifdef S_CORE
static ptr_t largePageMemCurr = LARGE_PAGE_FREEMEM;
ptr_t allocLargePage(int numPage)
{
    // align LARGE_PAGE_SIZE
    ptr_t ret = ROUND(largePageMemCurr, LARGE_PAGE_SIZE);
    largePageMemCurr = ret + numPage * LARGE_PAGE_SIZE;
    return ret;    
}
#endif

void initpmem(void){
    startsec = *((short*)0xffffffc0502001f6) + 1;
    char* p = 0xffffffc052000000lu;
    for(; p < 0xffffffc052080000lu; p += PAGE_SIZE){
        //freePage(p);
        char* vstart = (char*)p;
        struct run* r;
        r = (struct run*)vstart;
        r->next = freelist;
        freelist = r;
    }
}

void freePage(ptr_t baseAddr)
{
    // TODO [P4-task1] (design you 'freePage' here if you need):
    char* vstart = (char*)baseAddr;
    struct run* r;
    
    for(int i = 0; i < PAGE_SIZE; i++)
        *(vstart + i) = 0;
    
    r = (struct run*)vstart;
    r->next = freelist;
    freelist = r;
}

/*struct run* swap_out(){
    uint64_t va;
    PTE* pgdir = current_running->pgdir;
    //所有页表不能被换出。若页表不在内存中，在内核中试图查询页表时，会导致嵌套中断。所以只能去查询三级页表中所有的页表项，它们不会指示页表。
    for(int i = 0; i < 255; i++){
        if(pgdir[i] | _PAGE_PRESENT != 0){
            PTE* pmd = pa2kva(get_pfn(pgdir[i]) << 12);
            for(int j = 0; j < 512; j++){
                if(pmd[j] | _PAGE_PRESENT != 0){
                    PTE* pgtb = pa2kva(get_pfn(pmd[j]) << 12);
                    for(int k = 0; k < 512; k++){
                        if(pgtb[k] | _PAGE_PRESENT != 0){ //找到要换出的页
                            va = pa2kva(get_pfn(pgtb[k]) << 12);
                            uint64_t pstart = get_pfn(pgtb[k]) << 12;
                            pgtb[k] &= 0xfffffffffffffffe; //抹掉P位
                            pgtb[k] &= 0xffc00000000003ff; //抹掉物理页号
                            pgtb[k] |= (startsec << 10); //写入在磁盘中的起始位置
                            bios_sdwrite((unsigned)pstart, startsec, 8);
                            startsec += 8;
                        }
                    }
                }
            }
        }
    }
    
    return (struct run*)va;
}*/

char* kmalloc()
{
    // TODO [P4-task1] (design you 'kmalloc' here if you need):
    struct run* r;
    r = freelist;
    if(r) {
        freelist = r->next;
        return (char*)r;
    }
    /*else {
        r = swap_out();
        return (char*)r;
    }*/
}


/* this is used for mapping kernel virtual address into user page table */
void share_pgtable(uintptr_t dest_pgdir, uintptr_t src_pgdir)
{
    // TODO [P4-task1] share_pgtable:
    char* dest = (char*)dest_pgdir;
    char* src = (char*)src_pgdir;
    for(int i = 0; i < 4096; i++){
        *(dest++) = *(src++);
    }
}

/* allocate physical page for `va`, mapping it into `pgdir`,
   return the kernel virtual address for the page
   */
uintptr_t alloc_page_helper(uintptr_t va, uintptr_t pgdir)
{
    // TODO [P4-task1] alloc_page_helper:
}

void loader_helper(uint64_t va, uint64_t pa, PTE* pgdir){ //建立从va开始的一页到从pa开始的一页的映射
    uint64_t vpn2 = va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
    uint64_t vpn1 = (vpn2 << PPN_BITS) ^ (va >> (NORMAL_PAGE_SHIFT + PPN_BITS));
    uint64_t vpn0 = ((vpn2 << (PPN_BITS + PPN_BITS)) | (vpn1 << PPN_BITS)) ^ (va >> (NORMAL_PAGE_SHIFT));
         
    if(pgdir[vpn2] == 0){
        PTE* pgdir2 = kmalloc();
        set_pfn(&pgdir[vpn2], kva2pa(pgdir2) >> NORMAL_PAGE_SHIFT);
        set_attribute(&pgdir[vpn2], _PAGE_PRESENT | _PAGE_USER);
        clear_pgdir(pgdir2);
    }
         
    PTE* pmd = (PTE*)get_pa(pgdir[vpn2]);
    pmd = pa2kva(pmd);
    if(pmd[vpn1] == 0){
        PTE* pgtb = kmalloc();
        set_pfn(&pmd[vpn1], kva2pa(pgtb) >> NORMAL_PAGE_SHIFT);
        set_attribute(&pmd[vpn1], _PAGE_PRESENT | _PAGE_USER);
        clear_pgdir(pgtb);
    }
         
    PTE* ptb = (PTE*)get_pa(pmd[vpn1]);
    ptb = pa2kva(ptb);
    set_pfn(&ptb[vpn0], pa >> NORMAL_PAGE_SHIFT);
    set_attribute(&ptb[vpn0], _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC | _PAGE_USER | _PAGE_ACCESSED | _PAGE_DIRTY); //实现换页机制后这里不要设置A和D位
    local_flush_tlb_all();
}

void loader_helper_k(uint64_t va, uint64_t pa, PTE* pgdir){
    uint64_t vpn2 = (va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS)) & 0x1ff;
    uint64_t vpn1 = ((vpn2 << PPN_BITS) ^ (va >> (NORMAL_PAGE_SHIFT + PPN_BITS))) & 0x1ff;
    uint64_t vpn0 = (((vpn2 << (PPN_BITS + PPN_BITS)) | (vpn1 << PPN_BITS)) ^ (va >> (NORMAL_PAGE_SHIFT))) & 0x1ff;
         
    if(pgdir[vpn2] == 0){
        PTE* pgdir2 = kmalloc();
        set_pfn(&pgdir[vpn2], kva2pa(pgdir2) >> NORMAL_PAGE_SHIFT);
        set_attribute(&pgdir[vpn2], _PAGE_PRESENT);
        clear_pgdir(pgdir2);
    }
         
    PTE* pmd = (PTE*)get_pa(pgdir[vpn2]);
    pmd = pa2kva(pmd);
    if(pmd[vpn1] == 0){
        PTE* pgtb = kmalloc();
        set_pfn(&pmd[vpn1], kva2pa(pgtb) >> NORMAL_PAGE_SHIFT);
        set_attribute(&pmd[vpn1], _PAGE_PRESENT);
        clear_pgdir(pgtb);
    }
         
    PTE* ptb = (PTE*)get_pa(pmd[vpn1]);
    ptb = pa2kva(ptb);
    set_pfn(&ptb[vpn0], pa >> NORMAL_PAGE_SHIFT);
    set_attribute(&ptb[vpn0], _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC | _PAGE_ACCESSED | _PAGE_DIRTY); //实现换页机制后这里不要设置A和D位
    local_flush_tlb_all();
}

uintptr_t shm_page_get(int key)
{
    // TODO [P4-task4] shm_page_get:
}

void shm_page_dt(uintptr_t addr)
{
    // TODO [P4-task4] shm_page_dt:
}