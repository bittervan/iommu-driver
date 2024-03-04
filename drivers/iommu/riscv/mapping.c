#include "defs.h"
#include "vm.h"

uint64_t early_pgtbl[512] __attribute__((__aligned__(0x1000))) = {0};

uint64_t swapper_pgtbl[512] __attribute__((__aligned__(0x1000))) = {0};

void riscv_iommu_create_mapping(uint64_t *pgtbl, uint64_t va, uint64_t pa, uint64_t size, uint64_t flags) {


    uint64_t num_pages;
    va = PGROUNDDOWN(va);
    pa = PGROUNDDOWN(pa);
    num_pages = (PGROUNDUP(va + size) - va) / PGSIZE;

    // printk("riscv_iommu_create_mapping: va: %llx, pa: %llx, size: %llx, flags: %llx\n", va, pa, size, flags);

    for (int i = 0; i < num_pages; i++) {
        uint64_t current_va = va + i * PGSIZE;
        uint64_t current_pa = pa + i * PGSIZE;
        uint64_t index1 = VA_VPN2(current_va);
        uint64_t index2 = VA_VPN1(current_va);
        uint64_t index3 = VA_VPN0(current_va);
        uint64_t l1_entry;
        uint64_t *l2_pgtbl;
        uint64_t l2_entry;
        uint64_t *l3_pgtbl;

        // printk("before accessing pgtbl, pgtbl: %016llx, index1: %lld\n", (uint64_t)pgtbl, index1);
        if (pgtbl[index1] == 0) {
            uint64_t next_pgtbl = (uint64_t)page_to_virt(alloc_page(GFP_KERNEL | __GFP_ZERO));
            // printk("new allocated next_pgtbl: %016llx\n", next_pgtbl);
            pgtbl[index1] = PA_PPN(virt_to_phys((void*)next_pgtbl)) << 10 | PTE_V;
        }
        l1_entry = pgtbl[index1];

        l2_pgtbl = (uint64_t *)phys_to_virt(PTE_PPN(l1_entry) << 12);
        // printk("before accessing l2_pgtbl, l2_pgtbl: %016llx, index2: %lld\n", (uint64_t)l2_pgtbl, index2);
        if (l2_pgtbl[index2] == 0) {
            uint64_t next_pgtbl = (uint64_t)page_to_virt(alloc_page(GFP_KERNEL | __GFP_ZERO));
            // printk("new allocated next_pgtbl: %016llx\n", next_pgtbl);
            l2_pgtbl[index2] = PA_PPN(virt_to_phys((void*)next_pgtbl)) << 10 | PTE_V;
        }
        l2_entry = l2_pgtbl[index2];

        l3_pgtbl = (uint64_t *)phys_to_virt(PTE_PPN(l2_entry) << 12);
        // printk("before accessing l3_pgtbl, l3_pgtbl: %016llx, index3: %lld\n", (uint64_t)l3_pgtbl, index3);
        if (l3_pgtbl[index3] == 0) {
            l3_pgtbl[index3] = PA_PPN(current_pa) << 10 | flags | PTE_V;
        }
        // printk("after accessing l3_pgtbl\n");
    }
}

void riscv_iommu_remove_mapping(uint64_t *pgtbl, uint64_t va, uint64_t size) {
    uint64_t num_pages;
    va = PGROUNDDOWN(va);
    num_pages = (PGROUNDUP(va + size) - va) / PGSIZE;
    num_pages = 1;
    // return;

    for (int i = 0; i < num_pages; i++) {
        uint64_t current_va = va + i * PGSIZE;
        uint64_t index1 = VA_VPN2(current_va);
        uint64_t index2 = VA_VPN1(current_va);
        uint64_t index3 = VA_VPN0(current_va);
        uint64_t l1_entry;
        uint64_t *l2_pgtbl;
        uint64_t l2_entry;
        uint64_t *l3_pgtbl;

        if (pgtbl[index1] == 0) {
            return;
        }
        l1_entry = pgtbl[index1];

        l2_pgtbl = (uint64_t *)phys_to_virt(PTE_PPN(l1_entry) << 12);
        if (l2_pgtbl[index2] == 0) {
            return;
        }
        l2_entry = l2_pgtbl[index2];

        l3_pgtbl = (uint64_t *)phys_to_virt(PTE_PPN(l2_entry) << 12);
        if (l3_pgtbl[index3] == 0) {
            return;
        }
        // l3_pgtbl[index3] = 0;
        // only unset the valid bit
        l3_pgtbl[index3] &= ~PTE_V;
    }
}

uint64_t virt_is_valid(pagetable_t pgtbl, uint64_t va) {
    uint64_t index1 = VA_VPN2(va);
    uint64_t index2 = VA_VPN1(va);
    uint64_t index3 = VA_VPN0(va);
    uint64_t l1_entry;
    uint64_t *l2_pgtbl;
    uint64_t l2_entry;
    uint64_t *l3_pgtbl;

    if (pgtbl[index1] == 0) {
        return 0;
    }
    l1_entry = pgtbl[index1];

    l2_pgtbl = (uint64_t *)phys_to_virt(PTE_PPN(l1_entry) << 12);
    if (l2_pgtbl[index2] == 0) {
        return 0;
    }
    l2_entry = l2_pgtbl[index2];

    l3_pgtbl = (uint64_t *)phys_to_virt(PTE_PPN(l2_entry) << 12);
    if (l3_pgtbl[index3] == 0) {
        return 0;
    }

    return 1;
}

uint64_t io_to_virt(uint64_t pa) {
    return (uint64_t)phys_to_virt(pa);
}

uint64_t virt_to_io(uint64_t va) {
    return virt_to_phys((void*)va);
}