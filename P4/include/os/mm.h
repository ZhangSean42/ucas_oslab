/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *            Copyright (C) 2018 Institute of Computing Technology, CAS
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *                                   Memory Management
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
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
#ifndef MM_H
#define MM_H

#include <type.h>
#include <pgtable.h>
#include <os/sched.h>

#define MAP_KERNEL 1
#define MAP_USER 2
#define MEM_SIZE 32
#define PAGE_SIZE 4096 // 4K
#define INIT_KERNEL_STACK 0xffffffc052000000lu
#define USR_START 0x10000lu

/* Rounding; only works for n = power of two */
#define ROUND(a, n)     (((((uint64_t)(a))+(n)-1)) & ~((n)-1))
#define ROUNDDOWN(a, n) (((uint64_t)(a)) & ~((n)-1))

// TODO [P4-task1] */
extern void *kalloc(run_t*);
extern void freePage(uint64_t, run_t*);
extern void *init_uvm(int, uint64_t);
extern void map_normal_page(uint64_t, uint64_t, PTE*, uint64_t);
extern void unmap_normal_page(uint64_t, PTE*, int);
extern uint64_t uvmalloc(uint64_t, uint64_t, uint64_t, uint64_t, int);
extern void uvmdealloc(uint64_t, uint64_t, uint64_t);
extern void uvmdealloc_all(PTE*);
extern void free_walk(PTE*, int);
extern void share_pgtable(PTE*, PTE*);
extern void inst_page_fault(regs_context_t*, uint64_t, uint64_t);
extern void load_page_fault(regs_context_t*, uint64_t, uint64_t);
extern void store_page_fault(regs_context_t*, uint64_t, uint64_t);
extern uint64_t fetchaddr(uint64_t, PTE*);
extern void register_run(uint64_t, uint64_t, uint64_t, int);

// NOTE: A/C-core
#define USER_STACK_ADDR 0xf00010000lu

// TODO [P4-task4]: shm_page_get/dt */
uint64_t shm_page_get(int key);
void shm_page_dt(uint64_t addr);



#endif /* MM_H */
