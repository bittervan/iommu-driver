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

// avl tree is too slow! try hash table
// struct iova_node* iova_tree_root = NULL;
// Define the hash node structure
struct iova_hash_node {
    uint64_t iova;
    uint8_t cnt;
    struct hlist_node hlist;
};

// Define the hash table
DEFINE_HASHTABLE(iova_hash_table, 16);  // 10 buckets

// Initialize the hash table
void init_iova_hash_table(void) {
    // No additional initialization needed; the DEFINE_HASHTABLE macro takes care of it
}

// Insert a node into the hash table
int insert_into_iova_hash_table(uint64_t iova) {
    struct iova_hash_node *new_node;
    struct iova_hash_node *existing_node;

    // Allocate a new node
    new_node = kmalloc(sizeof(struct iova_hash_node), GFP_KERNEL);
    if (!new_node)
        return -ENOMEM;

    // Initialize the new node
    new_node->iova = iova;
    new_node->cnt = 1;  // Initial count is 1

    // Check if a node with the same iova already exists
    hash_for_each_possible(iova_hash_table, existing_node, hlist, iova) {
        if (existing_node->iova == iova) {
            // Iova already exists, increase the count and free the new node
            existing_node->cnt++;
            kfree(new_node);
            return 0;
        }
    }

    // Insert the new node into the hash table
    hash_add(iova_hash_table, &new_node->hlist, iova);

    return 0;
}

// Remove a node from the hash table
void remove_from_iova_hash_table(uint64_t iova) {
    struct iova_hash_node *node;

    // Find the node with the specified iova
    hash_for_each_possible(iova_hash_table, node, hlist, iova) {
        if (node->iova == iova) {
            // Decrease the count
            if (--node->cnt == 0) {
                // Count becomes zero, remove the node from the hash table and free it
                hash_del(&node->hlist);
                kfree(node);
            }
            break;
        }
    }
}

uint8_t get_cnt_for_iova(uint64_t iova) {
    struct iova_hash_node *node;

    // Find the node with the specified iova
    hash_for_each_possible(iova_hash_table, node, hlist, iova) {
        if (node->iova == iova) {
            return node->cnt;
        }
    }

    // Node not found, return 0 or an appropriate default value
    return 0;
}

void riscv_iommu_create_mapping(uint64_t *pgtbl, uint64_t va, uint64_t pa, uint64_t size, uint64_t flags) {
    uint64_t num_pages;
    va = PGROUNDDOWN(va);
    pa = PGROUNDDOWN(pa);
    num_pages = (PGROUNDUP(va + size) - va) / PGSIZE;

    // printk("riscv_iommu_create_mapping: va: %llx, pa: %llx, size: %llx, flags: %llx\n", va, pa, size, flags);


    // iova_tree_root = iova_node_insert(iova_tree_root, va);
    // if (iova_node_get_count(iova_tree_root, va) != 1) {
        // return;
    // }

    // insert the mapping into the iova hash table
    insert_into_iova_hash_table(va);
    if (get_cnt_for_iova(va) != 1) {
        return;
    }

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
    num_pages = 1;
    // return;

    // iova_tree_root = iova_node_delete(iova_tree_root, va);
    // if (iova_node_get_count(iova_tree_root, va) != 0) {
    //     return;
    // }
    remove_from_iova_hash_table(va);
    if (get_cnt_for_iova(va) != 0) {
        return;
    }

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

// struct iova_node {
//     uint8_t cnt;
//     int height;
//     uint64_t iova;
//     struct iova_node *left;
//     struct iova_node *right;
// };

// struct iova_node *iova_tree_root;

// struct iova_node *rotate_node_to_top(struct iova_node* accessed_node) {
//     struct iova_node *parent_node = NULL;
//     struct iova_node *current_node = iova_tree_root;
//     while (1) {
//         if (current_node == accessed_node) {
//             break;
//         } else if (accessed_node->iova < current_node->iova) {
//             struct iova_node *left_node = current_node->left;
//             if (left_node == accessed_node) {
//                 if (parent_node == NULL) {
//                     iova_tree_root = accessed_node;
//                 } else if (parent_node->left == current_node) {
//                     parent_node->left = accessed_node;
//                 } else {
//                     parent_node->right = accessed_node;
//                 }
//                 current_node->left = accessed_node->right;
//                 accessed_node->right = current_node;
//                 break;
//             } else {
//                 parent_node = current_node;
//                 current_node = left_node;
//             }
//         } else {
//             struct iova_node *right_node = current_node->right;
//             if (right_node == accessed_node) {
//                 if (parent_node == NULL) {
//                     iova_tree_root = accessed_node;
//                 } else if (parent_node->left == current_node) {
//                     parent_node->left = accessed_node;
//                 } else {
//                     parent_node->right = accessed_node;
//                 }
//                 current_node->right = accessed_node->left;
//                 accessed_node->left = current_node;
//                 break;
//             } else {
//                 parent_node = current_node;
//                 current_node = right_node;
//             }
//         }
//     }
//     return accessed_node;
// }

// bool remove_iova(uint64_t current_va) {
//     // remove the mapping if the reference count is zero
//     struct iova_node *current_node = iova_tree_root;
//     struct iova_node *parent_node = NULL;
//     bool should_delete = 0;

//     while (1) {
//         if (current_va < current_node->iova) {
//             parent_node = current_node;
//             current_node = current_node->left;
//         } else if (current_va > current_node->iova) {
//             parent_node = current_node;
//             current_node = current_node->right;
//         } else {
//             current_node->cnt--;
//             if (current_node->cnt == 0) {
//                 should_delete = 1;
//                 if (current_node->left == NULL && current_node->right == NULL) {
//                     if (parent_node == NULL) {
//                         iova_tree_root = NULL;
//                     } else if (parent_node->left == current_node) {
//                         parent_node->left = NULL;
//                     } else {
//                         parent_node->right = NULL;
//                     }
//                     kfree(current_node);
//                 } else if (current_node->left == NULL) {
//                     if (parent_node == NULL) {
//                         iova_tree_root = current_node->right;
//                     } else if (parent_node->left == current_node) {
//                         parent_node->left = current_node->right;
//                     } else {
//                         parent_node->right = current_node->right;
//                     }
//                     kfree(current_node);
//                 } else if (current_node->right == NULL) {
//                     if (parent_node == NULL) {
//                         iova_tree_root = current_node->left;
//                     } else if (parent_node->left == current_node) {
//                         parent_node->left = current_node->left;
//                     } else {
//                         parent_node->right = current_node->left;
//                     }
//                     kfree(current_node);
//                 } else {
//                     struct iova_node *successor = current_node->right;
//                     struct iova_node *successor_parent = current_node;
//                     while (successor->left != NULL) {
//                         successor_parent = successor;
//                         successor = successor->left;
//                     }
//                     current_node->iova = successor->iova;
//                     current_node->cnt = successor->cnt;
//                     if (successor_parent == current_node) {
//                         successor_parent->right = successor->right;
//                     } else {
//                         successor_parent->left = successor->right;
//                     }
//                     kfree(successor);
//                 }
//             }
//             break;
//         }
//     }

//     return should_delete;
// }

// struct iova_node *insert_iova(uint64_t current_va) {
//     if (unlikely(iova_tree_root == NULL)) {
//         iova_tree_root = (struct iova_node *)kmalloc(sizeof(struct iova_node), GFP_KERNEL);
//         iova_tree_root->iova = current_va;
//         iova_tree_root->cnt = 1;
//         iova_tree_root->left = NULL;
//         iova_tree_root->right = NULL;
//         return iova_tree_root;
//     } else {
//         struct iova_node *current_node = iova_tree_root;
//         while (1) {
//             if (current_va < current_node->iova) {
//                 if (current_node->left == NULL) {
//                     current_node->left = (struct iova_node *)kmalloc(sizeof(struct iova_node), GFP_KERNEL);
//                     current_node->left->iova = current_va;
//                     current_node->left->cnt = 1;
//                     current_node->left->left = NULL;
//                     current_node->left->right = NULL;
//                     return current_node->left;
//                 } else {
//                     current_node = current_node->left;
//                 }
//             } else if (current_va > current_node->iova) {
//                 if (current_node->right == NULL) {
//                     current_node->right = (struct iova_node *)kmalloc(sizeof(struct iova_node), GFP_KERNEL);
//                     current_node->right->iova = current_va;
//                     current_node->right->cnt = 1;
//                     current_node->right->left = NULL;
//                     current_node->right->right = NULL;
//                     return current_node->right;
//                 } else {
//                     current_node = current_node->right;
//                 }
//             } else {
//                 current_node->cnt++;
//                 return current_node;
//             }
//         }
//     }
//     // if (unlikely(iova_tree_root == NULL)) {
//     //     iova_tree_root = (struct iova_node *)kmalloc(sizeof(struct iova_node), GFP_KERNEL);
//     //     iova_tree_root->iova = current_va;
//     //     iova_tree_root->cnt = 1;
//     //     iova_tree_root->left = NULL;
//     //     iova_tree_root->right = NULL;
//     // } else {
//     //     struct iova_node *current_node = iova_tree_root;
//     //     while (1) {
//     //         if (current_va < current_node->iova) {
//     //             if (current_node->left == NULL) {
//     //                 current_node->left = (struct iova_node *)kmalloc(sizeof(struct iova_node), GFP_KERNEL);
//     //                 current_node->left->iova = current_va;
//     //                 current_node->left->cnt = 1;
//     //                 current_node->left->left = NULL;
//     //                 current_node->left->right = NULL;
//     //                 break;
//     //             } else {
//     //                 current_node = current_node->left;
//     //             }
//     //         } else if (current_va > current_node->iova) {
//     //             if (current_node->right == NULL) {
//     //                 current_node->right = (struct iova_node *)kmalloc(sizeof(struct iova_node), GFP_KERNEL);
//     //                 current_node->right->iova = current_va;
//     //                 current_node->right->cnt = 1;
//     //                 current_node->right->left = NULL;
//     //                 current_node->right->right = NULL;
//     //                 break;
//     //             } else {
//     //                 current_node = current_node->right;
//     //             }
//     //         } else {
//     //             current_node->cnt++;
//     //             break;
//     //         }
//     //     }
//     // }
// }

