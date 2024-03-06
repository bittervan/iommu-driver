#ifndef RISCV_IOMMU_AVL_H
#define RISCV_IOMMU_AVL_H
#include <linux/slab.h>


struct iova_node* iova_node_insert(struct iova_node* root, uint64_t iova);

struct iova_node* iova_node_delete(struct iova_node* root, uint64_t iova);

int iova_node_get_count(struct iova_node* root, uint64_t iova);

#endif