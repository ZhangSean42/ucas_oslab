#include <os/ioremap.h>
#include <os/mm.h>
#include <pgtable.h>
#include <type.h>

// maybe you can map it to IO_ADDR_START ?
static uint64_t io_base = IO_ADDR_START;

void *ioremap(unsigned long phys_addr, unsigned long size)
{
    // TODO: [p5-task1] map one specific physical region to virtual address
    PTE *pgdir = (PTE*)current_running[get_current_cpu_id()]->pgdir;
    uint64_t va = io_base;
    io_base += 0x40000000lu;
    va &= VA_MASK;
    uint64_t vpn2 = va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
    PTE *p = (PTE*)&pgdir[vpn2];
    set_pfn(p, (phys_addr & ~(0x40000000lu-1)) >> NORMAL_PAGE_SHIFT);
    uint64_t perm = _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC | _PAGE_ACCESSED | _PAGE_DIRTY;
    set_attribute(p, perm);
    local_flush_tlb_all();
    return (void*)(io_base - 0x40000000lu + (phys_addr & (0x40000000lu-1)));
}

void iounmap(void *io_addr)
{
    // TODO: [p5-task1] a very naive iounmap() is OK
    // maybe no one would call this function?
}
