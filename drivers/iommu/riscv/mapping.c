#include "defs.h"
#include "vm.h"
#include "avl.h"
#include <linux/hashtable.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/hashtable.h>

uint64_t early_pgtbl[512] __attribute__((__aligned__(0x1000))) = {0};

uint64_t swapper_pgtbl[512] __attribute__((__aligned__(0x1000))) = {0};
#define IOVA_HASH_TABLE_SIZE (1024 * 256)

// It seems using the PFN is the best performance! There will be no collision at all!
// Use hash table to maintain the reference count of each iova
// uint64_t customHash(uint64_t input) {
//     uint64_t hash = input;
//     hash = (hash ^ (hash >> 27)) + (hash % 997);
//     return hash;
// }

// first byte is reference count, the rest are iova;
uint64_t hash_table[IOVA_HASH_TABLE_SIZE] = {0};

uint8_t insert_into_iova_hash_table(uint64_t iova) {
    uint64_t hash = (iova >> 12) % IOVA_HASH_TABLE_SIZE;
    uint64_t ref_cnt;

    while ((hash_table[hash] != 0) && (hash_table[hash] & 0x00FFFFFFFFFFFFFF) != iova) {
        hash = (hash + 1) % IOVA_HASH_TABLE_SIZE;
    }

    ref_cnt = hash_table[hash] >> 56;
    if (ref_cnt == 0) {
        hash_table[hash] = (1UL << 56) | iova;
    } else {
        hash_table[hash] = ((ref_cnt + 1) << 56) | iova;
    }

    return ref_cnt + 1;
}

uint8_t remove_from_iova_hash_table(uint64_t iova) {
    uint64_t hash = (iova >> 12) % IOVA_HASH_TABLE_SIZE;
    uint64_t ref_cnt;

    while ((hash_table[hash] != 0) && (hash_table[hash] & 0x00FFFFFFFFFFFFFF) != iova) {
        hash = (hash + 1) % IOVA_HASH_TABLE_SIZE;
    }

    ref_cnt = hash_table[hash] >> 56;

    if (ref_cnt == 0) {
        return 0;
    } else

    if (ref_cnt == 1) {
        hash_table[hash] = 0;
        return ref_cnt - 1;
    } else {
        hash_table[hash] = ((ref_cnt - 1) << 56) | iova;
        return ref_cnt - 1;
    }
}

// uint8_t get_cnt_for_iova(uint64_t iova) {
//     uint64_t hash = customHash(iova) % IOVA_HASH_TABLE_SIZE;
//     uint64_t ref_cnt;

//     while ((hash_table[hash] != 0) && (hash_table[hash] & 0x00FFFFFFFFFFFFFF) != iova) {
//         hash = (hash + 1) % IOVA_HASH_TABLE_SIZE;
//     }

//     ref_cnt = hash_table[hash] >> 56;
//     return ref_cnt;
// }

void riscv_iommu_create_mapping(uint64_t *pgtbl, uint64_t va, uint64_t pa, uint64_t size, uint64_t flags) {
    uint64_t num_pages;
    va = PGROUNDDOWN(va);
    pa = PGROUNDDOWN(pa);
    num_pages = (PGROUNDUP(va + size) - va) / PGSIZE;

    // printk("riscv_iommu_create_mapping: va: %016llx, pa: %016llx, size: %016llx, flags: %016llx\n", va, pa, size, flags);

    // insert the mapping into the iova hash table
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

        if (insert_into_iova_hash_table(current_va) != 1) {
            return;
        }

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

extern volatile struct riscv_iommu_mmio *mmio_ctrl;

int riscv_iommu_flush_counter = 0;

void riscv_iommu_flush_iotlb(void) {
    riscv_iommu_flush_counter++;
    if (riscv_iommu_flush_counter == 128) {
        riscv_iommu_flush_counter = 0;
        writel(1, &mmio_ctrl->flush);
    }
}

void riscv_iommu_remove_mapping(uint64_t *pgtbl, uint64_t va, uint64_t size) {
    uint64_t num_pages;
    va = PGROUNDDOWN(va);
    num_pages = (PGROUNDUP(va + size) - va) / PGSIZE;
    // num_pages = 1;
    // return;

    // iova_tree_root = iova_node_delete(iova_tree_root, va);
    // if (iova_node_get_count(iova_tree_root, va) != 0) {
    //     return;
    // }
    for (int i = 0; i < num_pages; i++) {
        uint64_t current_va = va + i * PGSIZE;
        uint64_t index1 = VA_VPN2(current_va);
        uint64_t index2 = VA_VPN1(current_va);
        uint64_t index3 = VA_VPN0(current_va);
        uint64_t l1_entry;
        uint64_t *l2_pgtbl;
        uint64_t l2_entry;
        uint64_t *l3_pgtbl;

        if (remove_from_iova_hash_table(current_va) != 0) {
            return;
        }

        if (pgtbl[index1] == 0) {
            continue;
        }
        l1_entry = pgtbl[index1];

        l2_pgtbl = (uint64_t *)phys_to_virt(PTE_PPN(l1_entry) << 12);
        if (l2_pgtbl[index2] == 0) {
            continue;
        }
        l2_entry = l2_pgtbl[index2];

        l3_pgtbl = (uint64_t *)phys_to_virt(PTE_PPN(l2_entry) << 12);
        if (l3_pgtbl[index3] == 0) {
            continue;
        }
        // l3_pgtbl[index3] = 0;
        // only unset the valid bit
        l3_pgtbl[index3] &= ~PTE_V;
    }
    riscv_iommu_flush_iotlb();
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