#ifndef _VM_H
#define _VM_H

#include "defs.h"

#define VA_VPN2(va) ((va >> 30) & 0x1ff)
#define VA_VPN1(va) ((va >> 21) & 0x1ff)
#define VA_VPN0(va) ((va >> 12) & 0x1ff)
#define VA_VPN(va)  ((va >> 12) & 0x7ffffff)
#define VA_OFFSET(va) (va & 0xfff)

#define PA_PPN2(pa) ((pa >> 30) & 0x3ffffff)
#define PA_PPN1(pa) ((pa >> 21) & 0x1ff)
#define PA_PPN0(pa) ((pa >> 12) & 0x1ff)
#define PA_PPN(pa)  ((pa >> 12) & 0xfffffffffff)
#define PA_OFFSET(pa) (pa & 0xfff)

#define PTE_V 0x1
#define PTE_R 0x2
#define PTE_W 0x4
#define PTE_X 0x8
#define PTE_U 0x10
#define PTE_G 0x20
#define PTE_A 0x40
#define PTE_D 0x80

#define PTE_PPN2(pte) ((pte >> 28) & 0x3ffffff)
#define PTE_PPN1(pte) ((pte >> 19) & 0x1ff)
#define PTE_PPN0(pte) ((pte >> 10) & 0x1ff)
#define PTE_PPN(pte)  ((pte >> 10) & 0xfffffffffff)

// #define SATP_PPN(satp) (satp & 0xfffffffffff)
#define PGSIZE 0x1000 // 4KB
#define PGROUNDUP(addr) ((addr + PGSIZE - 1) & (~(PGSIZE - 1)))
#define PGROUNDDOWN(addr) (addr & (~(PGSIZE - 1)))

extern char _stext[];
extern char _etext[];
extern char _srodata[];
extern char _erodata[];
extern char _sdata[];

extern char _sramdisk[];
extern char _eramdisk[];

extern uint64_t swapper_pgtbl[];

void create_mapping(pagetable_t pgtbl, uint64_t va, uint64_t pa, uint64_t size, uint64_t flags);

uint64_t user_to_phys(pagetable_t pgtbl, uint64_t user_va);
uint64_t user_to_virt(pagetable_t pgtbl, uint64_t user_va);

uint64_t virt_is_valid(pagetable_t pgtbl, uint64_t va);

uint64_t io_to_virt(uint64_t pa);
uint64_t virt_to_io(uint64_t va);

void setup_kernel_vm(struct task_struct* task);

#endif