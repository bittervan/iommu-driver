#include "utils.h"
#include "defs.h"

static const uint64_t IOMMU_PHYS_ADDR = 0x60020000;

__attribute__((aligned(4096))) uint64_t riscv_iommu_pgt[4096 / sizeof(uint64_t)];
__attribute__((aligned(4096))) static struct riscv_iommu_cd riscv_iommu_dct[4096 / sizeof(struct riscv_iommu_cd)]; // context descriptor table

volatile struct riscv_iommu_mmio *mmio_ctrl;

// This implementation ignores the capability register, only use the sv39 format for the IOMMU.
// Initialize the IOMMU with the sv39 format.
// The queues are not used, we use a dedicated register to flush the iotlb to ensure best performance.
// The PCIe device is put to the fisrt context position, since the PCIe IP in vivado removes the device ID, and we only use one device.
static int __init riscv_iommu_init(void) {
    uint64_t ddtp = 0;
    uint64_t phys_dct;
    uint64_t phys_pgt;
    
    printk("riscv_iommu_init\n");

    mmio_ctrl = (struct riscv_iommu_mmio *)ioremap(IOMMU_PHYS_ADDR, sizeof(struct riscv_iommu_mmio));
    ddtp |= DDTP_IOMMU_MODE_1LVL; // 1-level directory table
    phys_dct = virt_to_phys(riscv_iommu_dct); // get the physical address of the mmio_ctrl
    ddtp |= (phys_dct >> 12) << 10; // set the physical address of the mmio_ctrl

    printk("mmio_ctrl->ddtpl: %08x\n", readl(&mmio_ctrl->ddtpl));
    printk("mmio_ctrl->ddtph: %08x\n", readl(&mmio_ctrl->ddtph));
    printk("mmio_ctrl->flush: %08x\n", readl(&mmio_ctrl->flush));
    writel((uint32_t)((uint64_t)ddtp >> 32), &mmio_ctrl->ddtph);
    writel((uint32_t)((uint64_t)ddtp & 0xffffffff), &mmio_ctrl->ddtpl);
    writel(1, &mmio_ctrl->flush);
    printk("mmio_ctrl->ddtpl: %08x\n", readl(&mmio_ctrl->ddtpl));
    printk("mmio_ctrl->ddtph: %08x\n", readl(&mmio_ctrl->ddtph));
    printk("mmio_ctrl->flush: %08x\n", readl(&mmio_ctrl->flush));

    riscv_iommu_dct[0].fsc = 0;
    riscv_iommu_dct[0].fsc |= FSC_IOMMU_MODE_SV39 << 60;
    phys_pgt = virt_to_phys(riscv_iommu_pgt);
    riscv_iommu_dct[0].fsc |= phys_pgt >> 12;
    return 0;
}

void *riscv_iommu_alloc(struct device *dev, size_t size, dma_addr_t *dma_handle, gfp_t gfp, unsigned long attrs) {
    // printk("riscv_iommu_alloc\n");
    return iommu_direct_alloc(dev, size, dma_handle, gfp, attrs);
}

void riscv_iommu_free(struct device *dev, size_t size, void *vaddr, dma_addr_t dma_handle, unsigned long attrs) {
    // printk("riscv_iommu_free\n");
    iommu_direct_free(dev, size, vaddr, dma_handle, attrs);
}

struct page *riscv_iommu_alloc_pages(struct device *dev, size_t size, dma_addr_t *dma_handle, enum dma_data_direction dir, gfp_t gfp) {
    // printk("riscv_iommu_alloc_pages\n");
    return iommu_direct_alloc_pages(dev, size, dma_handle, dir, gfp);
}

void riscv_iommu_free_pages(struct device *dev, size_t size, struct page *page, dma_addr_t dma_handle, enum dma_data_direction dir) {
    // printk("riscv_iommu_free_pages\n");
    iommu_direct_free_pages(dev, size, page, dma_handle, dir);
}

struct sg_table* riscv_iommu_alloc_noncontiguous(struct device *dev, size_t size, enum dma_data_direction dir, gfp_t gfp, unsigned long attrs) {
    panic("riscv_iommu_alloc_noncontiguous not implemented\n");
}

void riscv_iommu_free_noncontiguous(struct device *dev, size_t size, struct sg_table *sgt, enum dma_data_direction dir) {
    panic("riscv_iommu_free_noncontiguous not implemented\n");
}

int riscv_iommu_mmap(struct device *dev, struct vm_area_struct *vma, void *cpu_addr, dma_addr_t dma_addr, size_t size, unsigned long attrs) {
    panic("riscv_iommu_mmap not implemented\n");
}

int riscv_iommu_get_sgtable(struct device *dev, struct sg_table *sgt, void *cpu_addr, dma_addr_t dma_addr, size_t size, unsigned long attrs) {
    panic("riscv_iommu_get_sgtable not implemented\n");
}

dma_addr_t riscv_iommu_map_page(struct device *dev, struct page *page, unsigned long offset, size_t size, enum dma_data_direction dir, unsigned long attrs) {
    // printk("riscv_iommu_map_page\n");
    return iommu_direct_map_page(dev, page, offset, size, dir, attrs);
}

void riscv_iommu_unmap_page(struct device *dev, dma_addr_t handle, size_t size, enum dma_data_direction dir, unsigned long attrs) {
    // printk("riscv_iommu_unmap_page\n");
    iommu_direct_unmap_page(dev, handle, size, dir, attrs);
}
	
int riscv_iommu_map_sg(struct device *dev, struct scatterlist *sg, int nents, enum dma_data_direction dir, unsigned long attrs) {
    // printk("riscv_iommu_map_sg\n");
    return iommu_direct_map_sg(dev, sg, nents, dir, attrs);
}

void riscv_iommu_unmap_sg(struct device *dev, struct scatterlist *sg, int nents, enum dma_data_direction dir, unsigned long attrs) {
    // printk("riscv_iommu_unmap_sg\n");
    iommu_direct_unmap_sg(dev, sg, nents, dir, attrs);
}

dma_addr_t riscv_iommu_map_resource(struct device *dev, phys_addr_t phys_addr, size_t size, enum dma_data_direction dir, unsigned long attrs) {
    // printk("riscv_iommu_map_sg\n");
    return iommu_direct_map_resource(dev, phys_addr, size, dir, attrs);
}

void riscv_iommu_unmap_resource(struct device *dev, dma_addr_t handle, size_t size, enum dma_data_direction dir, unsigned long attrs) {
    // printk("riscv_iommu_unmap_resource\n");
    iommu_direct_unmap_resource(dev, handle, size, dir, attrs);
}

void riscv_iommu_sync_single_for_cpu(struct device *dev, dma_addr_t handle, size_t size, enum dma_data_direction dir) {
    // printk("riscv_iommu_sync_single_for_cpu\n");
    // iommu_direct_sync_single_for_cpu(dev, handle, size, dir);
    panic("riscv_iommu_sync_single_for_cpu not implemented\n");
}

void riscv_iommu_sync_single_for_device(struct device *dev, dma_addr_t handle, size_t size, enum dma_data_direction dir) {
    // printk("riscv_iommu_sync_single_for_device\n");
    // iommu_direct_sync_single_for_device(dev, handle, size, dir);
    panic("riscv_iommu_sync_single_for_device not implemented\n");
}

void riscv_iommu_sync_sg_for_cpu(struct device *dev, struct scatterlist *sg, int nents, enum dma_data_direction dir) {
    // printk("riscv_iommu_sync_sg_for_cpu\n");
    // iommu_direct_sync_sg_for_cpu(dev, sg, nents, dir);
    panic("riscv_iommu_sync_sg_for_cpu not implemented\n");
}

void riscv_iommu_sync_sg_for_device(struct device *dev, struct scatterlist *sg, int nents, enum dma_data_direction dir) {
    // printk("riscv_iommu_sync_sg_for_device\n");
    // iommu_direct_sync_sg_for_device(dev, sg, nents, dir);
    panic("riscv_iommu_sync_sg_for_device not implemented\n");
}

void riscv_iommu_cache_sync(struct device *dev, void *vaddr, size_t size, enum dma_data_direction dir) {
    panic("riscv_iommu_cache_sync not implemented\n");
}

int riscv_iommu_dma_supported(struct device *dev, u64 mask) {
    panic("riscv_iommu_dma_supported not implemented\n");
}

u64 riscv_iommu_get_required_mask(struct device *dev) {
    // panic("riscv_iommu_get_required_mask not implemented\n");
    // printk("riscv_iommu_get_required_mask\n");
    return iommu_direct_get_required_mask(dev);
}

size_t riscv_iommu_max_mapping_size(struct device *dev) {
    // panic("riscv_iommu_max_mapping_size not implemented\n");
    // printk("riscv_iommu_max_mapping_size\n");
    return iommu_direct_max_mapping_size(dev);
}

size_t riscv_iommu_opt_mapping_size(void) {
    // panic("riscv_iommu_opt_mapping_size not implemented\n");
    // printk("riscv_iommu_opt_mapping_size\n");
    return iommu_direct_opt_mapping_size();
}

unsigned long riscv_iommu_get_merge_boundary(struct device *dev) {
    panic("riscv_iommu_get_merge_boundary not implemented\n");
}

struct dma_map_ops riscv_iommu_dma_map_ops = {
    .alloc = riscv_iommu_alloc,
    .free = riscv_iommu_free,
    .alloc_pages = riscv_iommu_alloc_pages,
    .free_pages = riscv_iommu_free_pages,
    .alloc_noncontiguous = riscv_iommu_alloc_noncontiguous,
    .free_noncontiguous = riscv_iommu_free_noncontiguous,
    .mmap = riscv_iommu_mmap,
    .get_sgtable = riscv_iommu_get_sgtable,
    .map_page = riscv_iommu_map_page,
    .unmap_page = riscv_iommu_unmap_page,
    .map_sg = riscv_iommu_map_sg,
    .unmap_sg = riscv_iommu_unmap_sg,
    .map_resource = riscv_iommu_map_resource,
    .unmap_resource = riscv_iommu_unmap_resource,
    .sync_single_for_cpu = riscv_iommu_sync_single_for_cpu,
    .sync_single_for_device = riscv_iommu_sync_single_for_device,
    .sync_sg_for_cpu = riscv_iommu_sync_sg_for_cpu,
    .sync_sg_for_device = riscv_iommu_sync_sg_for_device,
    .cache_sync = NULL,
    .dma_supported = NULL,
    .get_required_mask = riscv_iommu_get_required_mask,
    .max_mapping_size = riscv_iommu_max_mapping_size,
    .opt_mapping_size = riscv_iommu_opt_mapping_size,
    .get_merge_boundary = NULL,
};

arch_initcall(riscv_iommu_init);

// struct io_pgtable *riscv_iommu_alloc_pgtable(struct io_pgtable_cfg *cfg, void *cookie) {
//     printk("*************************\n");
//     printk("riscv_iommu_alloc_pgtable\n");
//     printk("*************************\n");
//     return NULL;
// }
// void riscv_iommu_free_pgtable(struct io_pgtable *iop) {
//     printk("************************\n");
//     printk("riscv_iommu_free_pgtable\n");
//     printk("************************\n");
// }

// struct io_pgtable_init_fns io_pgtable_riscv_init_fns = {
//     .alloc = riscv_iommu_alloc_pgtable,
//     .free = riscv_iommu_free_pgtable,
// };

// struct iommu_device* riscv_iommu_probe_device(struct device *dev) {
//     return 0;
// }

// struct iommu_ops riscv_iommu_ops = {
//     .owner = THIS_MODULE,
//     .pgsize_bitmap = SZ_4K | SZ_2M | SZ_1G,
//     .capable = NULL,
//     .probe_device = riscv_iommu_probe_device,
// };

// static struct pci_driver riscv_iommu_pci_driver = {
// 	.name = KBUILD_MODNAME,
// 	.id_table = riscv_iommu_pci_tbl,
// 	.probe = riscv_iommu_pci_probe,
// 	.remove = riscv_iommu_pci_remove,
// 	.driver = {
// 		   .pm = pm_sleep_ptr(&riscv_iommu_pm_ops),
// 		   .of_match_table = riscv_iommu_of_match,
// 		   },
// };

