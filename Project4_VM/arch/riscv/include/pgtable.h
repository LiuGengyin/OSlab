#ifndef PGTABLE_H
#define PGTABLE_H

#include <type.h>

#define SATP_MODE_SV39 8
#define SATP_MODE_SV48 9

#define SATP_ASID_SHIFT 44lu
#define SATP_MODE_SHIFT 60lu

#define NORMAL_PAGE_SHIFT 12lu
#define NORMAL_PAGE_SIZE (1lu << NORMAL_PAGE_SHIFT)
#define LARGE_PAGE_SHIFT 21lu
#define LARGE_PAGE_SIZE (1lu << LARGE_PAGE_SHIFT)

/*
 * Flush entire local TLB.  'sfence.vma' implicitly fences with the instruction
 * cache as well, so a 'fence.i' is not necessary.
 */
static inline void local_flush_tlb_all(void)
{
    __asm__ __volatile__ ("sfence.vma" : : : "memory");
}

/* Flush one page from local TLB */
static inline void local_flush_tlb_page(unsigned long addr)
{
    __asm__ __volatile__ ("sfence.vma %0" : : "r" (addr) : "memory");
}

static inline void local_flush_icache_all(void)
{
    asm volatile ("fence.i" ::: "memory");
}

static inline void set_satp(
    unsigned mode, unsigned asid, unsigned long ppn)
{
    unsigned long __v =
        (unsigned long)(((unsigned long)mode << SATP_MODE_SHIFT) | ((unsigned long)asid << SATP_ASID_SHIFT) | ppn);
    __asm__ __volatile__("sfence.vma\ncsrw satp, %0" : : "rK"(__v) : "memory");
}

#define PGDIR_PA 0x51000000lu  // use 51000000 page as PGDIR

/*
 * PTE format:
 * | XLEN-1  10 | 9             8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0
 *       PFN      reserved for SW   D   A   G   U   X   W   R   V
 */

#define _PAGE_ACCESSED_OFFSET 6

#define _PAGE_PRESENT (1 << 0)
#define _PAGE_READ (1 << 1)     /* Readable */
#define _PAGE_WRITE (1 << 2)    /* Writable */
#define _PAGE_EXEC (1 << 3)     /* Executable */
#define _PAGE_USER (1 << 4)     /* User */
#define _PAGE_GLOBAL (1 << 5)   /* Global */
#define _PAGE_ACCESSED (1 << 6) /* Set by hardware on any access \
                                 */
#define _PAGE_DIRTY (1 << 7)    /* Set by hardware on any write */
#define _PAGE_SOFT (1 << 8)     /* Reserved for software */

#define _PAGE_PFN_SHIFT 10lu

#define VA_MASK ((1lu << 39) - 1)

#define PPN_BITS 9lu
#define NUM_PTE_ENTRY (1 << PPN_BITS)

typedef uint64_t PTE;

/* Translation between physical addr and kernel virtual addr */
static inline uintptr_t kva2pa(uintptr_t kva)
{
    /* TODO: [P4-task1] */
    return (kva - 0xffffffc000000000lu);
}

static inline uintptr_t pa2kva(uintptr_t pa)
{
    /* TODO: [P4-task1] */
    return (pa + 0xffffffc000000000lu);
}

/* get physical page addr from PTE 'entry' */
static inline uint64_t get_pa(PTE entry)
{
    /* TODO: [P4-task1] */
    return ((entry & 0xfffffffffffffc00lu) << 2);
}

/* Get/Set page frame number of the `entry` */
static inline long get_pfn(PTE entry)
{
    /* TODO: [P4-task1] */
    return (entry >> 10);
}
static inline void set_pfn(PTE *entry, uint64_t pfn)
{
    /* TODO: [P4-task1] */
    *entry &= 0xffc00000000003fflu;
    //pfn &= 0xffffffffffflu;
    *entry |= (pfn << 10);
}

/* Get/Set attribute(s) of the `entry` */
static inline long get_attribute(PTE entry, uint64_t mask)
{
    /* TODO: [P4-task1] */
    return (entry & mask); //传入mask以获取对应位的状态，返回值其余位为0.
}
static inline void set_attribute(PTE *entry, uint64_t bits)
{
    /* TODO: [P4-task1] */
    *entry &= 0xfffffffffffffc00lu;
    *entry |= bits;
}

static inline void clear_pgdir(uintptr_t pgdir_addr)
{
    /* TODO: [P4-task1] */
    for(int i = 0; i < 4096; i++){
        *((char*)pgdir_addr + i) = 0;
    }
}

/* 
 * query the page table stored in pgdir_va to obtain the physical 
 * address corresponding to the virtual address va.
 * 
 * return the kernel virtual address of the physical address 
 */
static inline uintptr_t get_kva_of(uintptr_t va, uintptr_t pgdir_va)
{
    // TODO: [P4-task1] (todo if you need)
    //输入的页表应为根目录，查询工作全部由本函数完成。
    uintptr_t kva;
    uint64_t vpn2 = va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
    uint64_t vpn1 = (vpn2 << PPN_BITS) ^ (va >> (NORMAL_PAGE_SHIFT + PPN_BITS));
    uint64_t vpn0 = ((vpn2 << (PPN_BITS + PPN_BITS)) | (vpn1 << PPN_BITS)) ^ (va >> (NORMAL_PAGE_SHIFT));
    //若为1G大页
    PTE* p1 = (PTE*)pgdir_va;
    if(p1[vpn2] | (_PAGE_READ | _PAGE_WRITE | _PAGE_EXEC)){
        kva = ((p1[vpn2] & 0x3ffffff0000000lu) << 2) | (va & 0x3ffffffflu); //页表项的[53:28]连上虚址的[29:0]。
        kva += 0xffffffc000000000lu;
        return kva;
    }
    //若为2M大页
    PTE* p2 = (PTE*)(p1[vpn2] & 0x3ffffffffffc00lu);
    if(p2[vpn1] | (_PAGE_READ | _PAGE_WRITE | _PAGE_EXEC)){
        kva = ((p2[vpn1] & 0x3ffffffff80000lu) << 2) | (va & 0x1ffffflu); //页表项的[53:19]连上虚址的[20:0]。
        kva += 0xffffffc000000000lu;
        return kva;
    }
    //正常页
    PTE* p3 = (PTE*)(p2[vpn1] & 0x3ffffffffffc00lu);
    kva = ((p3[vpn0] & 0x3ffffffffffc00lu) << 2) | (va & 0xffflu); //页表项的[53:10]连上虚址的[11:0]。
    kva += 0xffffffc000000000lu;
    return kva;
}


#endif  // PGTABLE_H
