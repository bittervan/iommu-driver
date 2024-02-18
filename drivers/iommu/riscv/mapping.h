#ifndef DRIVERS_IOMMU_RISCV_MAPPING_H
#define DRIVERS_IOMMU_RISCV_MAPPING_H

#include "defs.h"
#include "vm.h"

void riscv_iommu_create_mapping(uint64_t *pgtbl, uint64_t va, uint64_t pa, uint64_t size, uint64_t flags);
void riscv_iommu_remove_mapping(uint64_t *pgtbl, uint64_t va, uint64_t size);

#endif