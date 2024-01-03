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

#define PAGINGMEM_START 0x52002000lu
#define PFREEMEM_START  0x53000000lu
#define PFREEMEM_END    0x60000000lu
#define CORE0_STACK     0xffffffc052001000lu
#define CORE1_STACK     0xffffffc052002000lu
#define MAX_RUN_NUM  64

typedef struct run{
    struct run *next;
} run_t;

typedef struct allocated_run{
    uint64_t va;
    uint64_t pgdir;
    int pin;
} allocated_run_t;

typedef struct shmpage{
    uint64_t kva;
    int sh_nr;
    int key;
} shmpage_t;

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
#define OFFSET_MASK_4K ((1lu << 12) - 1)
#define OFFSET_MASK_2M ((1lu << 21) - 1)
#define ATTRIBUTE_MASK ((1lu << 10) - 1)

#define PPN_BITS 9lu
#define NUM_PTE_ENTRY (1lu << PPN_BITS)

typedef uint64_t PTE;

/* Translation between physical addr and kernel virtual addr */
static inline uint64_t kva2pa(uint64_t kva)
{
    /* TODO: [P4-task1] */
    return kva - 0xffffffc000000000lu;
}

static inline uint64_t pa2kva(uint64_t pa)
{
    /* TODO: [P4-task1] */
    return pa + 0xffffffc000000000lu;
}

/* Get/Set page frame number of the `entry` */
static inline long get_pfn(PTE entry)
{
    /* TODO: [P4-task1] */
    return (entry >> _PAGE_PFN_SHIFT) << NORMAL_PAGE_SHIFT;
}
static inline void set_pfn(PTE *entry, uint64_t pfn)
{
    /* TODO: [P4-task1] */
    *entry = pfn << _PAGE_PFN_SHIFT;
}

/* Get/Set attribute(s) of the `entry` */
static inline long get_attribute(PTE entry)
{
    /* TODO: [P4-task1] */
    return entry & (ATTRIBUTE_MASK);
}
static inline void set_attribute(PTE *entry, uint64_t bits)
{
    /* TODO: [P4-task1] */
    *entry = ((*entry >> _PAGE_PFN_SHIFT) << _PAGE_PFN_SHIFT) | bits;
}

static inline void clear_pgdir(uint64_t pgdir_addr)
{
    /* TODO: [P4-task1] */
    uint64_t *pgdir_ptr = (uint64_t*)pgdir_addr;
    do {
        *pgdir_ptr++ = 0;
    } while((uint64_t)pgdir_ptr & (NORMAL_PAGE_SIZE-1));
}

static inline void clear_pte(uint64_t pte_addr)
{
    /* TODO: [P4-task1] */
    uint64_t *pte_ptr = (uint64_t*)pte_addr;
    do {
        *pte_ptr++ = 0;
    } while((uint64_t)pte_ptr & (NORMAL_PAGE_SIZE-1));
}

#endif  // PGTABLE_H
