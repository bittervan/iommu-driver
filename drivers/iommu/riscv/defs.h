#ifndef RISCV_IOMMU_DEFS_H
#define RISCV_IOMMU_DEFS_H

#include <linux/io-pgtable.h>
#include <linux/printk.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/dma-map-ops.h>
#include <linux/dma-direct.h>
#include <linux/dma-mapping.h>

struct riscv_iommu_mmio {
    uint64_t capabilities;
    uint32_t fctl;
    uint32_t flush; // This register can be customized for different IOMMU according to the spec v1.0
    // uint64_t ddtp;
    uint32_t ddtpl;
    uint32_t ddtph;
    uint64_t cqb;
    uint32_t cqh;
    uint32_t cqt;
    uint64_t fqb;
    uint32_t fqh;
    uint32_t fqt;
    uint64_t pqb;
    uint32_t pqh;
    uint32_t pqt;
    uint32_t cqcsr;
    uint32_t fqcbr;
    uint32_t pqcsr;
    uint32_t ipsr;
    uint32_t iocntovf;
    uint32_t iocntinh;
    uint64_t iohpmcycles;
    uint64_t iohpmctr[31];
    uint64_t iohpmevtr[31];
    uint64_t tr_req_iova;
    uint64_t tr_req_ctl;
    uint64_t tr_response;
    uint64_t reserved1[8];
    uint64_t custom[9];
    uint64_t icvec;
    uint64_t msi_cfg_tbl[32];
    uint64_t reserved2[384];
} __attribute__((packed));

struct riscv_iommu_cd {
    uint64_t fsc; // This is also known as iosatip
    uint64_t ta;
    uint64_t iohgatp;
    uint64_t tc;
} __attribute__((packed));

static const uint64_t DDTP_IOMMU_MODE_OFF = 0;
static const uint64_t DDTP_IOMMU_MODE_BARE = 1;
static const uint64_t DDTP_IOMMU_MODE_1LVL = 2;
static const uint64_t DDTP_IOMMU_MODE_2LVL = 3;
static const uint64_t DDTP_IOMMU_MODE_3LVL = 4;

static const uint64_t FSC_IOMMU_MODE_BARE = 0;
static const uint64_t FSC_IOMMU_MODE_SV39 = 8;
static const uint64_t FSC_IOMMU_MODE_SV48 = 9;
static const uint64_t FSC_IOMMU_MODE_SV57 = 10;

static const uint64_t RISCV_IOMMU_OFFSET = 0xab00000000;
static const uint64_t RISCV_IOMMU_PA_START = 0x80000000;
static const uint64_t RISCV_IOMMU_PA_SIZE  = 0x40000000;

typedef uint64_t* pagetable_t;

#endif