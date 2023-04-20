/*******************************************************************
 *   _________     _____      _____    ____  _____    ___  ____    *
 *  |_   ___  |  |_   _|     |_   _|  |_   \|_   _|  |_  ||_  _|   *
 *    | |_  \_|    | |         | |      |   \ | |      | |_/ /     *
 *    |  _|        | |   _     | |      | |\ \| |      |  __'.     *
 *   _| |_        _| |__/ |   _| |_    _| |_\   |_    _| |  \ \_   *
 *  |_____|      |________|  |_____|  |_____|\____|  |____||____|  *
 *                                                                 *
 *******************************************************************
 *                                                                 *
 *  PCI bus communication module                                   *
 *                                                                 *
 *******************************************************************/
/** @file flink_pci.c
 *  @brief PCI bus communication module. 
 * 
 *  Implements read and write functions over pci bus. 
 *
 *  @author Martin Züger
 *  @author Urs Graf
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/pci.h>

#include "flink.h"
#include "flink_pci.h"

//#define DBG 1
#define MODULE_NAME THIS_MODULE->name

#define BAR_0 0

MODULE_AUTHOR("Martin Zueger <martin@zueger.eu>");
MODULE_DESCRIPTION("fLink PCI/PCIe module");
MODULE_LICENSE("Dual BSD/GPL");

// ############ Module parameters ############
static unsigned short vid = 0x1172;
static unsigned short pid = 0x0004;

module_param(vid, ushort, 0444);
MODULE_PARM_DESC(vid, "PCI vendor ID, eg. '0x1172' for Altera");
module_param(pid, ushort, 0444);
MODULE_PARM_DESC(pid, "PCI product ID, eg. '0x000'");

// ############ Bus communication functions ############
u8 pci_read8(struct flink_device* fdev, u32 addr) {
	struct flink_pci_data* pci_data = (struct flink_pci_data*)fdev->bus_data;
	if(pci_data != NULL) {
		return ioread8(pci_data->base_addr + addr);
	}
	return 0;
}

u16 pci_read16(struct flink_device* fdev, u32 addr) {
	struct flink_pci_data* pci_data = (struct flink_pci_data*)fdev->bus_data;
	if(pci_data != NULL) {
		return ioread16(pci_data->base_addr + addr);
	}
	return 0;
}

u32 pci_read32(struct flink_device* fdev, u32 addr) {
	struct flink_pci_data* pci_data = (struct flink_pci_data*)fdev->bus_data;
	u32* read_address = (u32*)(pci_data->base_addr + addr);
	if(pci_data != NULL) {
		#if defined(DBG)
			printk(KERN_DEBUG "[%s] Reading 32 bit from PCI device (flink device id %u) at address 0x%p...", MODULE_NAME, fdev->id, read_address);
		#endif
		return ioread32(read_address);
	}
	else {
		#if defined(DBG)
			printk(KERN_ERR "[%s] Reading 32 bit from PCI device (flink device id %u) at address 0x%p failed!", MODULE_NAME, fdev->id, read_address);
		#endif
	}
	return 0;
}

int pci_write8(struct flink_device* fdev, u32 addr, u8 val) {
	struct flink_pci_data* pci_data = (struct flink_pci_data*)fdev->bus_data;
	if(pci_data != NULL) {
		iowrite8(val, pci_data->base_addr + addr);
		return 0;
	}
	return -1;
}

int pci_write16(struct flink_device* fdev, u32 addr, u16 val) {
	struct flink_pci_data* pci_data = (struct flink_pci_data*)fdev->bus_data;
	if(pci_data != NULL) {
		iowrite16(val, pci_data->base_addr + addr);
		return 0;
	}
	return -1;
}

int pci_write32(struct flink_device* fdev, u32 addr, u32 val) {
	struct flink_pci_data* pci_data = (struct flink_pci_data*)fdev->bus_data;
	if(pci_data != NULL) {
		iowrite32(val, pci_data->base_addr + addr);
		return 0;
	}
	return -1;
}

u32 pci_address_space_size(struct flink_device* fdev) {
	struct flink_pci_data* pci_data = (struct flink_pci_data*)fdev->bus_data;
	if(pci_data != NULL) {
		return (u32)(pci_data->mem_size - PCI_CONFIG_SIZE);
	}
	return 0;
}

struct flink_bus_ops pci_bus_ops = {
	.read8              = pci_read8,
	.read16             = pci_read16,
	.read32             = pci_read32,
	.write8             = pci_write8,
	.write16            = pci_write16,
	.write32            = pci_write32,
	.address_space_size = pci_address_space_size
};

// ############ Device handling ############
static struct flink_device* create_flink_pci_device(struct flink_bus_ops* bus_ops, struct pci_dev* pci_device, void __iomem* base_ptr, unsigned long length) {
	struct flink_pci_data* pci_data = kmalloc(sizeof(struct flink_pci_data), GFP_KERNEL);
	struct flink_device* fdev = flink_device_alloc();
	
	if(pci_data != NULL && fdev != NULL) {
		pci_data->pci_device = pci_device;
		pci_data->base_addr = base_ptr + BASE_OFFSET;
		pci_data->mem_size = length;
		
		flink_device_init(fdev, bus_ops, THIS_MODULE);
		fdev->bus_data = pci_data;
		return fdev;
	}
	return NULL;
}

// ############ Initialization and cleanup ############
static int __init flink_pci_init(void) {
	int error = 0;
	struct flink_device* flink_pci_dev;
	struct pci_dev* pci_device;
	void __iomem* mmio_base_ptr;
	unsigned long mmio_length;
	
	#if defined(DBG)
		printk(KERN_DEBUG "[%s] Initializing module with parameters 'vid=%x, pid=%x'", MODULE_NAME, vid, pid);
	#endif

	// Get PCI device struct
	pci_device = pci_get_device(vid, pid, NULL);
	if(pci_device == NULL) {
		printk(KERN_ALERT "[%s] ERROR: PCIe device not found!", MODULE_NAME);
		goto err_pci_device_not_found;
	}
	
	// Initialize and enable the PCI device
	error = pci_enable_device(pci_device);
	if(error) {
		printk(KERN_ALERT "[%s] ERROR: Unable to enable PCI device!", MODULE_NAME);
		goto err_pci_enable_device;
	}
	
	// Reserve PCI memory resources
	error = pci_request_regions(pci_device, KBUILD_MODNAME);
	if(error) {
		printk(KERN_ALERT "[%s] ERROR: Memory region request failed!", MODULE_NAME);
		goto err_pci_region_request;
	}
	
	// I/O Memory mapping
	mmio_length = pci_resource_len(pci_device, BAR_0);
	mmio_base_ptr = pci_iomap(pci_device, BAR_0, mmio_length);
	#if defined(DBG)
		printk(KERN_DEBUG "[%s] PCI resource I/O memory mapping:", MODULE_NAME);
		printk(KERN_DEBUG "  -> Base address:  0x%p", mmio_base_ptr);
		printk(KERN_DEBUG "  -> Memory length: 0x%lx (%lu bytes)", mmio_length, mmio_length);
	#endif
	if(mmio_base_ptr == NULL){
		printk(KERN_ALERT "[%s] ERROR: I/O Memory mapping failed!", MODULE_NAME);
		goto err_pci_iomap;
	}
	
	flink_pci_dev = create_flink_pci_device(&pci_bus_ops, pci_device, mmio_base_ptr, mmio_length);
	flink_device_add(flink_pci_dev);
	
	// All done
	printk(KERN_INFO "[%s] Module sucessfully loaded", MODULE_NAME);
	
	return 0;

// ---- ERROR HANDLING ----
	err_pci_iomap:
		pci_release_regions(pci_device);
	
	err_pci_region_request:
		pci_disable_device(pci_device);
	
	err_pci_enable_device:
		pci_device = NULL;
	
	err_pci_device_not_found:
		// nothing to do
	
	return error;
}

static void __exit flink_pci_exit(void) {
	struct flink_device* fdev;
	struct flink_device* fdev_next;
	struct flink_pci_data* pci_data;
	
	#if defined(DBG)
		printk(KERN_DEBUG "[%s] Looking for devices which are appropriated to this module...", MODULE_NAME);
	#endif
	
	list_for_each_entry_safe(fdev, fdev_next, flink_get_device_list(), list) {
		if(fdev->appropriated_module == THIS_MODULE) {
			#if defined(DBG)
				printk(KERN_DEBUG "[%s] Device with id '%u' is appropriated to this module and will be removed!", MODULE_NAME, fdev->id);
			#endif
			pci_data = (struct flink_pci_data*)(fdev->bus_data);
			pci_release_regions(pci_data->pci_device);
			pci_disable_device(pci_data->pci_device);
			kfree(pci_data);
			flink_device_remove(fdev);
			flink_device_delete(fdev);
		}
		else {
			#if defined(DBG)
				printk(KERN_DEBUG "[%s] Device with id '%u' is not appropriated to this module!", MODULE_NAME, fdev->id);
			#endif
		}
	}
	printk(KERN_INFO "[%s] Module sucessfully unloaded", MODULE_NAME);
}

module_init(flink_pci_init);
module_exit(flink_pci_exit);
