#include <linux/io-pgtable.h>
#include <linux/printk.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/dma-map-ops.h>
#include <linux/dma-direct.h>
#include <linux/dma-mapping.h>

#include "mapping.h"

extern uint64_t riscv_iommu_pgt[];

static inline void* iommu_direct_alloc(struct device *dev, size_t size, dma_addr_t *dma_handle,
        gfp_t gfp, unsigned long attrs)
{
    void* ret = dma_direct_alloc(dev, size, dma_handle, gfp, attrs);
	riscv_iommu_create_mapping(riscv_iommu_pgt, *dma_handle, *dma_handle, size, PTE_V | PTE_R | PTE_W | PTE_X | PTE_U);
	return ret;
}

static inline void iommu_direct_free(struct device *dev, size_t size, void *vaddr,
		dma_addr_t dma_handle, unsigned long attrs)
{
	riscv_iommu_remove_mapping(riscv_iommu_pgt, dma_handle, size);
	dma_direct_free(dev, size, vaddr, dma_handle, attrs);
}

static inline struct page* iommu_direct_alloc_pages(struct device *dev, size_t size,
		dma_addr_t *dma_handle, gfp_t gfp, unsigned long attrs)
{
	struct page* ret = dma_direct_alloc_pages(dev, size, dma_handle, gfp, attrs);
	riscv_iommu_create_mapping(riscv_iommu_pgt, *dma_handle, *dma_handle, size, PTE_V | PTE_R | PTE_W | PTE_X | PTE_U);
	return ret;
}

static inline void iommu_direct_free_pages(struct device *dev, size_t size,
		struct page *page, dma_addr_t dma_handle, unsigned long attrs)
{
	riscv_iommu_remove_mapping(riscv_iommu_pgt, dma_handle, size);
	dma_direct_free_pages(dev, size, page, dma_handle, attrs);
}

static inline dma_addr_t iommu_direct_map_page(struct device *dev,
		struct page *page, unsigned long offset, size_t size,
		enum dma_data_direction dir, unsigned long attrs)
{
	phys_addr_t phys = page_to_phys(page) + offset;
	dma_addr_t dma_addr = phys_to_dma(dev, phys);

	riscv_iommu_create_mapping(riscv_iommu_pgt, dma_addr, dma_addr, size, PTE_V | PTE_R | PTE_W | PTE_X | PTE_U);

	if (is_swiotlb_force_bounce(dev)) {
		if (is_pci_p2pdma_page(page))
			return DMA_MAPPING_ERROR;
		return swiotlb_map(dev, phys, size, dir, attrs);
	}

	if (unlikely(!dma_capable(dev, dma_addr, size, true))) {
		if (is_pci_p2pdma_page(page))
			return DMA_MAPPING_ERROR;
		if (is_swiotlb_active(dev))
			return swiotlb_map(dev, phys, size, dir, attrs);

		dev_WARN_ONCE(dev, 1,
			     "DMA addr %pad+%zu overflow (mask %llx, bus limit %llx).\n",
			     &dma_addr, size, *dev->dma_mask, dev->bus_dma_limit);
		return DMA_MAPPING_ERROR;
	}

	if (!dev_is_dma_coherent(dev) && !(attrs & DMA_ATTR_SKIP_CPU_SYNC))
		arch_sync_dma_for_device(phys, size, dir);
	return dma_addr;
}

static inline void dma_direct_sync_single_for_device(struct device *dev,
		dma_addr_t addr, size_t size, enum dma_data_direction dir)
{
	phys_addr_t paddr = dma_to_phys(dev, addr);

	if (unlikely(is_swiotlb_buffer(dev, paddr)))
		swiotlb_sync_single_for_device(dev, paddr, size, dir);

	if (!dev_is_dma_coherent(dev))
		arch_sync_dma_for_device(paddr, size, dir);
}

static inline void dma_direct_sync_single_for_cpu(struct device *dev,
		dma_addr_t addr, size_t size, enum dma_data_direction dir)
{
	phys_addr_t paddr = dma_to_phys(dev, addr);

	if (!dev_is_dma_coherent(dev)) {
		arch_sync_dma_for_cpu(paddr, size, dir);
		arch_sync_dma_for_cpu_all();
	}

	if (unlikely(is_swiotlb_buffer(dev, paddr)))
		swiotlb_sync_single_for_cpu(dev, paddr, size, dir);

	if (dir == DMA_FROM_DEVICE)
		arch_dma_mark_clean(paddr, size);
}

static inline void iommu_direct_unmap_page(struct device *dev, dma_addr_t addr,
		size_t size, enum dma_data_direction dir, unsigned long attrs)
{
	phys_addr_t phys = dma_to_phys(dev, addr);

	riscv_iommu_remove_mapping(riscv_iommu_pgt, addr, size);

	if (!(attrs & DMA_ATTR_SKIP_CPU_SYNC))
		dma_direct_sync_single_for_cpu(dev, addr, size, dir);

	if (unlikely(is_swiotlb_buffer(dev, phys)))
		swiotlb_tbl_unmap_single(dev, phys, size, dir,
					 attrs | DMA_ATTR_SKIP_CPU_SYNC);
}


static inline dma_addr_t iommu_direct_map_resource(struct device *dev, 
		dma_addr_t paddr, size_t size, enum dma_data_direction dir,
		unsigned long attrs)
{
	return dma_direct_map_resource(dev, paddr, size, dir, attrs);
}

static inline void iommu_direct_unmap_resource(struct device *dev, dma_addr_t dma_addr,
		size_t size, enum dma_data_direction dir, unsigned long attrs)
{
	// do nothing
}

void iommu_direct_unmap_sg(struct device *dev, struct scatterlist *sgl,
		int nents, enum dma_data_direction dir, unsigned long attrs)
{
	struct scatterlist *sg;
	int i;

	for_each_sg(sgl,  sg, nents, i) {
		if (sg_is_dma_bus_address(sg))
			sg_dma_unmark_bus_address(sg);
		else
			iommu_direct_unmap_page(dev, sg->dma_address,
					      sg_dma_len(sg), dir, attrs);
	}
}

static inline int iommu_direct_map_sg(struct device *dev, struct scatterlist *sgl, int nents,
		enum dma_data_direction dir, unsigned long attrs)
{
	struct pci_p2pdma_map_state p2pdma_state = {};
	enum pci_p2pdma_map_type map;
	struct scatterlist *sg;
	int i, ret;

	for_each_sg(sgl, sg, nents, i) {
		if (is_pci_p2pdma_page(sg_page(sg))) {
			map = pci_p2pdma_map_segment(&p2pdma_state, dev, sg);
			switch (map) {
			case PCI_P2PDMA_MAP_BUS_ADDR:
				continue;
			case PCI_P2PDMA_MAP_THRU_HOST_BRIDGE:
				/*
				 * Any P2P mapping that traverses the PCI
				 * host bridge must be mapped with CPU physical
				 * address and not PCI bus addresses. This is
				 * done with dma_direct_map_page() below.
				 */
				break;
			default:
				ret = -EREMOTEIO;
				goto out_unmap;
			}
		}

		sg->dma_address = iommu_direct_map_page(dev, sg_page(sg),
				sg->offset, sg->length, dir, attrs);
		if (sg->dma_address == DMA_MAPPING_ERROR) {
			ret = -EIO;
			goto out_unmap;
		}
		sg_dma_len(sg) = sg->length;
	}

	return nents;

out_unmap:
	iommu_direct_unmap_sg(dev, sgl, i, dir, attrs | DMA_ATTR_SKIP_CPU_SYNC);
	return ret;
}

// static inline int iommu_direct_map_sg(struct device *dev, struct scatterlist *sg, int nents,
// 		enum dma_data_direction dir, unsigned long attrs)
// {
// 	return dma_direct_map_sg(dev, sg, nents, dir, attrs);
// }

// extern int dma_direct_unmap_sg(struct device *dev, struct scatterlist *sg, int nents,
// 		enum dma_data_direction dir, unsigned long attrs);

// static inline void iommu_direct_unmap_sg(struct device *dev, struct scatterlist *sg, int nents,
// 		enum dma_data_direction dir, unsigned long attrs)
// {
// 	dma_direct_unmap_sg(dev, sg, nents, dir, attrs);
// }

static inline u64 iommu_direct_get_required_mask(struct device *dev)
{
	return dma_direct_get_required_mask(dev);
}

extern size_t dma_direct_max_mapping_size(struct device *dev);

static inline size_t iommu_direct_max_mapping_size(struct device *dev)
{
	return dma_direct_max_mapping_size(dev);
}


static inline size_t iommu_direct_opt_mapping_size(void) {
	return SIZE_MAX;
}