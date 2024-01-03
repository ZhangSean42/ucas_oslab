#include <os/mm.h>
#include <os/string.h>
#include <os/kernel.h>
#include <screen.h>

// NOTE: A/C-core
allocated_run_t arun[MAX_RUN_NUM];
shmpage_t shmpages[MAX_RUN_NUM];
run_t free_list = {NULL};
run_t paging_list = {NULL};
int sd_idx;
int arun_ptr;
extern int end_sec;
extern int swap_sec_start;

void register_run(uint64_t va, uint64_t kva, uint64_t pgdir, int pin)
{
    int idx = (kva & (0x3flu << NORMAL_PAGE_SHIFT)) >> NORMAL_PAGE_SHIFT;
    arun[idx].va = va;
    arun[idx].pgdir = pgdir;
    arun[idx].pin = pin;
    
    int i;
    for (i = 0; i < 64; i++){
        if (pcb[i].pgdir == pgdir){
            break;
        }
    }
    printl("register run for %s, idx=%d, kva=%lx, va=%lx, pgdir=%lx\n", pcb[i].name, idx, kva, va, pgdir);
}

void *kalloc(run_t *list_head)
{
    run_t *p = list_head->next;
    if (p == NULL)
        return (void*)0;
    list_head->next = p->next;
    memset((void*)p, 0, PAGE_SIZE);
    return (void*)p;
}

void freePage(uint64_t baseAddr, run_t *list_head)
{
    // TODO [P4-task1] (design you 'freePage' here if you need):
    memset((void*)baseAddr, 0, PAGE_SIZE);
    run_t *p = (run_t*)baseAddr;
    p->next = list_head->next;
    list_head->next = p;
}

/* this is used for mapping kernel virtual address into user page table */
void share_pgtable(PTE *dest_pgdir, PTE *src_pgdir)
{
    // TODO [P4-task1] share_pgtable:
    for (uint64_t kva = 0xffffffc050000000; kva < 0xffffffc060000000; kva += PAGE_SIZE){
        uint64_t va = kva & VA_MASK;
        uint64_t vpn2 = va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
        dest_pgdir[vpn2] = src_pgdir[vpn2];
    }
}

PTE *walk(uint64_t va, PTE *pgdir)
{
    uint64_t vpn2, vpn1, vpn0;
    PTE *pmd, *pgt;
    uint64_t new_page;
    
    va &= VA_MASK;
    vpn2 = va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
    vpn1 = (vpn2 << PPN_BITS) ^ (va >> (NORMAL_PAGE_SHIFT + PPN_BITS));
    vpn0 = (va >> NORMAL_PAGE_SHIFT) ^ (vpn1 << PPN_BITS) ^ (vpn2 << (PPN_BITS + PPN_BITS));
    
    if ((pgdir[vpn2] & _PAGE_PRESENT) == 0){
        // alloc a new second-level page directory
        new_page = kalloc(&paging_list);
        set_pfn(&pgdir[vpn2], kva2pa(new_page) >> NORMAL_PAGE_SHIFT);
        set_attribute(&pgdir[vpn2], _PAGE_PRESENT);
    }
    
    pmd = (PTE*)pa2kva(get_pfn(pgdir[vpn2]));
    
    if ((pmd[vpn1] & _PAGE_PRESENT) == 0){
        new_page = kalloc(&paging_list);
        set_pfn(&pmd[vpn1], kva2pa(new_page) >> NORMAL_PAGE_SHIFT);
        set_attribute(&pmd[vpn1], _PAGE_PRESENT);
    }
    
    pgt = (PTE*)pa2kva(get_pfn(pmd[vpn1]));
    return &pgt[vpn0];
}

uint64_t walk_addr(uint64_t va, PTE *pgdir)
{
    uint64_t vpn2, vpn1, vpn0;
    PTE *pmd, *pgt;
    uint64_t new_page;
    
    va &= VA_MASK;
    vpn2 = va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
    vpn1 = (vpn2 << PPN_BITS) ^ (va >> (NORMAL_PAGE_SHIFT + PPN_BITS));
    vpn0 = (va >> NORMAL_PAGE_SHIFT) ^ (vpn1 << PPN_BITS) ^ (vpn2 << (PPN_BITS + PPN_BITS));
    
    if ((pgdir[vpn2] & _PAGE_PRESENT) == 0){
        new_page = kalloc(&paging_list);
        set_pfn(&pgdir[vpn2], kva2pa(new_page) >> NORMAL_PAGE_SHIFT);
        set_attribute(&pgdir[vpn2], _PAGE_PRESENT);
    }
    
    pmd = (PTE*)pa2kva(get_pfn(pgdir[vpn2]));
    
    if ((pmd[vpn1] & _PAGE_PRESENT) == 1 && ((pmd[vpn1] & _PAGE_READ) | (pmd[vpn1] & _PAGE_WRITE) | (pmd[vpn1] & _PAGE_EXEC)) != 0){
        uint64_t tmp = pmd[vpn1] >> _PAGE_PFN_SHIFT;
        tmp = get_pfn(pmd[vpn1]) << (LARGE_PAGE_SHIFT - NORMAL_PAGE_SHIFT);
        tmp = tmp | va & OFFSET_MASK_2M;
        return pa2kva(get_pfn(pmd[vpn1]) | va & OFFSET_MASK_2M);
    }
    
    if ((pmd[vpn1] & _PAGE_PRESENT) == 0){
        new_page = kalloc(&paging_list);
        set_pfn(&pmd[vpn1], kva2pa(new_page) >> NORMAL_PAGE_SHIFT);
        set_attribute(&pmd[vpn1], _PAGE_PRESENT);
    }
    
    pgt = (PTE*)pa2kva(get_pfn(pmd[vpn1]));
    return pa2kva(get_pfn(pgt[vpn0]) | va & OFFSET_MASK_4K);
}

uint64_t swap_out()
{
    uint64_t dirty, access;
    int pin;
    uint64_t perm;
    int out_idx;

    do{
        PTE *p = walk(arun[arun_ptr].va, (PTE*)arun[arun_ptr].pgdir);
        dirty  = *p & _PAGE_DIRTY;
        access = *p & _PAGE_ACCESSED;
        pin = arun[arun_ptr].pin;
        if (!pin){
            if (dirty){
                perm = get_attribute(*p);
                set_attribute(p, perm & ~_PAGE_DIRTY);
            } 
            else if(access){
                perm = get_attribute(*p);
                set_attribute(p, perm & ~_PAGE_ACCESSED);
            }
        }
        out_idx = arun_ptr;
        arun_ptr = (arun_ptr + 1) % MAX_RUN_NUM;
    }while (access != 0 || pin != 0);
    PTE *p = walk(arun[out_idx].va, (PTE*)arun[out_idx].pgdir);
    
    int i;
    for (i = 0; i < 16; i++){
        if (pcb[i].pgdir == arun[out_idx].pgdir){
            break;
        }
    }
    printl("swap out page %lx from %s with idx %d, sd_idx = %d\n", arun[out_idx].va, pcb[i].name, out_idx, sd_idx);
    
    uint64_t kva = pa2kva(get_pfn(*p));
    set_pfn(p, sd_idx);
    set_attribute(p, _PAGE_SOFT); 
    bios_sd_write(kva2pa(kva), 8, swap_sec_start+sd_idx);
    sd_idx += 8;
    local_flush_tlb_all();
    return kva;
}

void map_normal_page(uint64_t va, uint64_t kva, PTE *pgdir, uint64_t perm)
{
    PTE *p = walk(va, pgdir);
    set_pfn(p, kva2pa(kva) >> NORMAL_PAGE_SHIFT);
    set_attribute(p, perm);
}

void unmap_normal_page(uint64_t va, PTE *pgdir, int free)
{
    PTE *p = walk(va, pgdir);
    if (free == 1 && *p & _PAGE_PRESENT == 1){
        uint64_t kva = pa2kva(get_pfn(*p));
        
        int i;
        for (i = 0; i < 16; i++){
            if (pcb[i].pgdir == pgdir){
                break;
            }
        }
        printl("unmap va=%lx for %s\n", va, pcb[i].name);
        freePage(kva, &free_list);
       // for (run_t *q = free_list.next; q != NULL; q = q->next){
         //   printl("current free_list %lx\n", q);
        //}
    } 
    
    *p &= ~_PAGE_PRESENT;
    local_flush_tlb_all();
}

uint64_t uvmalloc(uint64_t current_va, uint64_t size, uint64_t pgdir, uint64_t perm, int pin)
{
    // TODO [P4-task1] (design you 'uvmalloc' here if you need):
    uint64_t uvmalloc_end = ROUNDDOWN(current_va + size - 1, NORMAL_PAGE_SIZE);
    uint64_t current_va_aligned = ROUND(current_va, NORMAL_PAGE_SIZE);
    uint64_t kva;
    
    for (uint64_t va = current_va_aligned; va <= uvmalloc_end; va += PAGE_SIZE){
        kva = (uint64_t)kalloc(&free_list);
        if (!kva){
            kva = swap_out();
        }
        
        int i;
        for (i = 0; i < 16; i++){
            if (pcb[i].pgdir == pgdir){
                break;
            }
        }
        printl("map va=%lx for %s\n", va, pcb[i].name);
        //for (run_t *q = free_list.next; q != NULL; q = q->next){
          //  printl("current free_list %lx\n", q);
        //}
        
        register_run(va, kva, pgdir, pin);
        map_normal_page(va, kva, (PTE*)pgdir, perm);
    }
            
    return kva;
}

void uvmdealloc(uint64_t current_va, uint64_t size, uint64_t pgdir)
{
    uint64_t uvmalloc_end = ROUNDDOWN(current_va + size - 1, NORMAL_PAGE_SIZE);
    uint64_t current_va_aligned = ROUND(current_va, NORMAL_PAGE_SIZE);

    for (uint64_t va = current_va_aligned; va <= uvmalloc_end; va += PAGE_SIZE){
        unmap_normal_page(va, (PTE*)pgdir, 1);
    }
}

void uvmdealloc_all(PTE *pgdir)
{
    for (uint64_t i = 0; i < (NUM_PTE_ENTRY >> 1); i++){
        if (pgdir[i] & _PAGE_PRESENT){
            PTE* pmd = (PTE*)pa2kva(get_pfn(pgdir[i]));
            for (uint64_t j = 0; j < NUM_PTE_ENTRY; j++){
                if (pmd[j] & _PAGE_PRESENT){
                PTE* pgt = (PTE*)pa2kva(get_pfn(pmd[j]));
                    for (uint64_t k = 0; k < NUM_PTE_ENTRY; k++){
                        unmap_normal_page((i << (NORMAL_PAGE_SHIFT+PPN_BITS+PPN_BITS)) + (j << (NORMAL_PAGE_SHIFT+PPN_BITS)) + (k << NORMAL_PAGE_SHIFT), pgdir, 1);
                    }
                }
                else{
                    continue;
                }
            }
        }
        else{
            continue;
        }
    }
}

void *init_uvm(int pcb_idx, uint64_t mem_sz)
{
    uint64_t pgdir = (uint64_t)kalloc(&paging_list);
    pcb[pcb_idx].pgdir = pgdir;
    uint64_t perm = _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC | _PAGE_USER | _PAGE_ACCESSED | _PAGE_DIRTY;
    uvmalloc(USR_START, mem_sz, pgdir, perm, 1);
    share_pgtable((PTE*)pgdir, (PTE*)pa2kva(PGDIR_PA));
}

void free_walk(PTE *pgdir, int level)
{
    int freeable_entry = level ? NUM_PTE_ENTRY : (NUM_PTE_ENTRY >> 1);
    for (int i = 0; i < freeable_entry; i++){
        if ((pgdir[i] & _PAGE_PRESENT) && level != 2){
            free_walk((PTE*)pa2kva(get_pfn(pgdir[i])), level+1);
        }
        /*else if ((pgdir[i] & _PAGE_PRESENT) && level == 2){
            screen_move_cursor(0,0);
            printk("panic, free pagetable before free physical page\n");
            while (1)
                ;
        }*/
    }
    freePage((uint64_t)pgdir, &paging_list);
}

uint64_t shm_page_get(int key)
{
    // TODO [P4-task4] shm_page_get:
    int i;
    uint64_t va;
    for (i = 0; i < MAX_RUN_NUM; i++){
        if (shmpages[i].key == key){
            shmpages[i].sh_nr++;
            goto choose_va;
        }
    }
    for (i = 0; i < MAX_RUN_NUM; i++){
        if (shmpages[i].key == 0){
            uint64_t share_page = (uint64_t)kalloc(&paging_list);
            shmpages[i].kva = share_page;
            shmpages[i].sh_nr++;
            shmpages[i].key = key;
            goto choose_va;
        }
    }
    screen_move_cursor(0, 0);
    printk("panic, no more free shmpages\n");
    while (1){
        ;
    }
    
choose_va:
    for (va = 0x1000lu; va < 0x100000lu; va += NORMAL_PAGE_SIZE){
        PTE *p = walk(va, (PTE*)current_running[get_current_cpu_id()]->pgdir);
        if (*p & _PAGE_PRESENT){
            continue;
        }
        else{
          uint64_t perm = _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC | _PAGE_USER | _PAGE_ACCESSED | _PAGE_DIRTY;
          map_normal_page(va, shmpages[i].kva, current_running[get_current_cpu_id()]->pgdir, perm);
          break;
        }
    }
    return va;
}

void shm_page_dt(uint64_t addr)
{
    // TODO [P4-task4] shm_page_dt:
    uint64_t kva = walk_addr(addr, (PTE*)current_running[get_current_cpu_id()]->pgdir);
    for (int i = 0; i < MAX_RUN_NUM; i++){
        if (shmpages[i].kva = kva){
            shmpages[i].sh_nr--;
            unmap_normal_page(addr, (PTE*)current_running[get_current_cpu_id()]->pgdir, 0);
            if (shmpages[i].sh_nr == 0){
                shmpages[i].key = 0;
                freePage(kva, &paging_list);
            }
            break;
        }
    }
}

uint64_t fetchaddr(uint64_t va, PTE *pgdir)
{
    return walk_addr(va, pgdir);
}

void inst_page_fault(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    uint64_t pgdir = current_running[get_current_cpu_id()]->pgdir;
    uint64_t perm = _PAGE_PRESENT | _PAGE_READ | _PAGE_EXEC | _PAGE_USER | _PAGE_ACCESSED;
    printl("\n");
    printl("inst page fault\n");
    if (stval >= 0xffffffc050000000lu && stval <= 0xffffffc060000000lu){
        screen_move_cursor(0, 0);
        printk("Dennied access!!!\n");
        do_exit();
    }
    PTE *p = walk(stval, pgdir);
    if ((*p & _PAGE_PRESENT) == 0){
        if ((*p & _PAGE_SOFT) != 0){
            int sd_idx = *p >> _PAGE_PFN_SHIFT;
            
            printl("swap in page from sd_idx %d for %s\n", sd_idx, current_running[get_current_cpu_id()]->name);
            
            uint64_t kva = uvmalloc(ROUNDDOWN(stval, NORMAL_PAGE_SIZE), PAGE_SIZE, pgdir, perm, 0);
            printl("\n");
            bios_sd_read(kva2pa(kva), 8, swap_sec_start+sd_idx);
        }
        else {
            uvmalloc(ROUNDDOWN(stval, NORMAL_PAGE_SIZE), PAGE_SIZE, pgdir, perm, 0);
        }
    }
    else if ((*p & _PAGE_ACCESSED) == 0 || (*p & _PAGE_EXEC) == 0){
        map_normal_page(ROUNDDOWN(stval, NORMAL_PAGE_SIZE), pa2kva(get_pfn(*p)), (PTE*)pgdir, perm);
    }
    local_flush_tlb_all();
}

void load_page_fault(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    uint64_t pgdir = current_running[get_current_cpu_id()]->pgdir;
    uint64_t perm = _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | _PAGE_USER | _PAGE_ACCESSED;
    printl("\n");
    printl("load page fault\n");
    if (stval >= 0xffffffc050000000lu && stval <= 0xffffffc060000000lu){
        screen_move_cursor(0, 0);
        printk("Dennied access!!!\n");
        do_exit();
    }
    PTE *p = walk(stval, pgdir);
    if ((*p & _PAGE_PRESENT) == 0){
        if ((*p & _PAGE_SOFT) != 0){
            int sd_idx = *p >> _PAGE_PFN_SHIFT;
            
            printl("swap in page from sd_idx %d for %s\n", sd_idx, current_running[get_current_cpu_id()]->name);
            
            uint64_t kva = uvmalloc(ROUNDDOWN(stval, NORMAL_PAGE_SIZE), PAGE_SIZE, pgdir, perm, 0);
            printl("\n");
            bios_sd_read(kva2pa(kva), 8, swap_sec_start+sd_idx);
        }
        else {
            uvmalloc(ROUNDDOWN(stval, NORMAL_PAGE_SIZE), PAGE_SIZE, pgdir, perm, 0);
        }
    }
    else if ((*p & _PAGE_ACCESSED) == 0){
        map_normal_page(ROUNDDOWN(stval, NORMAL_PAGE_SIZE), pa2kva(get_pfn(*p)), (PTE*)pgdir, perm);
    }
    local_flush_tlb_all();
}

void store_page_fault(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    uint64_t pgdir = current_running[get_current_cpu_id()]->pgdir;
    uint64_t perm = _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | _PAGE_USER | _PAGE_ACCESSED | _PAGE_DIRTY;

    printl("\n");
    printl("store page fault\n");
    if (stval >= 0xffffffc050000000lu && stval <= 0xffffffc060000000lu){
        screen_move_cursor(0, 0);
        printk("Dennied access!!!\n");
        do_exit();
    }
    PTE *p = walk(stval, (PTE*)pgdir);
    if ((*p & _PAGE_PRESENT) == 0){
        if ((*p & _PAGE_SOFT) != 0){
            int sd_idx = *p >> _PAGE_PFN_SHIFT;
            
            printl("swap in page from sd_idx %d for %s\n", sd_idx, current_running[get_current_cpu_id()]->name);
            
            uint64_t kva = uvmalloc(ROUNDDOWN(stval, NORMAL_PAGE_SIZE), PAGE_SIZE, pgdir, perm, 0);
            bios_sd_read(kva2pa(kva), 8, swap_sec_start+sd_idx);
        }
        else{
            uvmalloc(ROUNDDOWN(stval, NORMAL_PAGE_SIZE), PAGE_SIZE, pgdir, perm, 0);
        }
    }
    else if ((*p & _PAGE_WRITE) == 0){
        uint64_t pre_kva = pa2kva(get_pfn(*p));
        uint64_t kva = uvmalloc(ROUNDDOWN(stval, NORMAL_PAGE_SIZE), PAGE_SIZE, pgdir, perm, 0);
        memcpy((uint8_t*)kva, (uint8_t*)pre_kva, PAGE_SIZE);
        screen_move_cursor(0, 4);
        printk("doing copy-on-write!!!\n");
    }
    else if ((*p & _PAGE_ACCESSED) == 0 || (*p & _PAGE_DIRTY) == 0){
        map_normal_page(ROUNDDOWN(stval, NORMAL_PAGE_SIZE), pa2kva(get_pfn(*p)), (PTE*)pgdir, perm);
    }
    local_flush_tlb_all();
}