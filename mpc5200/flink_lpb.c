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
 *  MPC5200 Local Plus Bus communication module                    *
 *                                                                 *
 *******************************************************************/

/** @file flink_lpb.c
 *  @brief MPC5200 Local Plus Bus communication module.
 *
 *  Implements read and write functions over Local Plus Bus.
 *
 *  @author Marco Tinner
 *  @author Urs Graf
 */

#include <linux/init.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/fs.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <asm/io.h>
#include <asm/mpc52xx.h>

#include "../flink.h"

//#define DEBUG
#define MODULE_NAME THIS_MODULE->name

// ############ Module Parameters ############
static unsigned int dev_mem_length = MAX_ADDRESS_SPACE;
module_param(dev_mem_length, uint, 0444);
MODULE_PARM_DESC(dev_mem_length, "device memory length");

MODULE_AUTHOR("Urs Graf");
MODULE_DESCRIPTION("fLink local plus bus module for mpc5200");
MODULE_LICENSE("Dual BSD/GPL");

/// @brief Local Plus Bus device data
struct flink_lpb_data {
	void __iomem* base_ptr;
	unsigned long base_address;
	unsigned long mem_size;
}* lpb_data;

/* Chip Select Controller */
struct mpc52xx_cs_ctl {
	u32 cs0_boot_cr;	/* CS_CTRL + 0x00 */
	u32 cs1_cr;			/* CS_CTRL + 0x04 */
	u32 cs2_cr;			/* CS_CTRL + 0x08 */
	u32 cs3_cr;			/* CS_CTRL + 0x0c */
	u32 cs4_cr;			/* CS_CTRL + 0x10 */
	u32 cs5_cr;			/* CS_CTRL + 0x14 */
	u32 cs_cr;			/* CS_CTRL + 0x18 */
	u32 cs_sr;			/* CS_CTRL + 0x1c */
	u32 cs6_cr;			/* CS_CTRL + 0x20 */
	u32 cs7_cr;			/* CS_CTRL + 0x24 */
	u32 csb_cr;			/* CS_CTRL + 0x28 */
	u32 csdc_cr;		/* CS_CTRL + 0x2c */
};

// ############ Bus communication functions ############
u8 lpb_read8(struct flink_device* fdev, u32 addr) {
	struct flink_lpb_data* lpb_data = (struct flink_lpb_data*)fdev->bus_data;
	if(lpb_data != NULL) {
		return in_8(lpb_data->base_ptr + addr);
	}
	return 0;
}

u16 lpb_read16(struct flink_device* fdev, u32 addr) {
	struct flink_lpb_data* lpb_data = (struct flink_lpb_data*)fdev->bus_data;
	if(lpb_data != NULL) {
		return in_be16(lpb_data->base_ptr + addr);
	}
	return 0;
}

u32 lpb_read32(struct flink_device* fdev, u32 addr){
	struct flink_lpb_data* lpb_data = (struct flink_lpb_data*)fdev->bus_data;
	if(lpb_data != NULL) {
		return in_be32(lpb_data->base_ptr + addr);
	}
	return 0;
}

int lpb_write8(struct flink_device* fdev, u32 addr, u8 val) {
	struct flink_lpb_data* lpb_data = (struct flink_lpb_data*)fdev->bus_data;
	if(lpb_data != NULL) {
		out_8(lpb_data->base_ptr + addr, val);
	}
	return 0;
}

int lpb_write16(struct flink_device* fdev, u32 addr, u16 val) {
	struct flink_lpb_data* lpb_data = (struct flink_lpb_data*)fdev->bus_data;
	if(lpb_data != NULL) {
		out_be16(lpb_data->base_ptr + addr, val);
	}
	return 0;
}

int lpb_write32(struct flink_device* fdev, u32 addr, u32 val) {

	struct flink_lpb_data* lpb_data = (struct flink_lpb_data*)fdev->bus_data;
	if(lpb_data != NULL) {
		out_be32(lpb_data->base_ptr + addr, val);
	}
	return 0;
}

u32 lpb_address_space_size(struct flink_device* fdev) {
	struct flink_lpb_data* lpb_data = (struct flink_lpb_data*)fdev->bus_data;
	return (u32)(lpb_data->mem_size);
}

struct flink_bus_ops lpb_bus_ops = {
	.read8              = lpb_read8,
	.read16             = lpb_read16,
	.read32             = lpb_read32,
	.write8             = lpb_write8,
	.write16            = lpb_write16,
	.write32            = lpb_write32,
	.address_space_size = lpb_address_space_size
};

// search for compatible node in device tree, returns node
static struct device_node* getNode(const char *compatible) {
	struct device_node *ofn;
	ofn = of_find_compatible_node(NULL, NULL, compatible);
	if (!ofn) {
		printk(KERN_ERR "[%s] %s: of_find_compatible_node error\n", MODULE_NAME, __FUNCTION__);
		return NULL;
	}
	return ofn;
}

// ############ Driver probe and release functions ############
static int flink_probe(struct platform_device *ofdev) {
	int len;
	struct device_node *devNode;
	const u32 *propReg, *propRanges;
	u32 cs, csStartAddr, csSize, flag = 0;
	const u32 *addrPtr;
	u64 addr64, size64;
	u32 __iomem* csPtr;
	u32 __iomem* ptr;
	struct flink_device* fdev;
	struct mpc52xx_mmap_ctl __iomem *immr;
	struct mpc52xx_cs_ctl __iomem *csctl;

	if(lpb_data != NULL) return 0;	// second run of probe
	lpb_data = kmalloc(sizeof(struct flink_lpb_data), GFP_KERNEL);
	fdev = flink_device_alloc();

	// get chip select number for FPGA
	devNode = getNode("ntb,flink_driver");
	if(!devNode) return -1;
	propReg = of_get_property(devNode, "reg", &len);
	cs = propReg[0];
	of_node_put(devNode);
	#if defined(DEBUG)
		printk(KERN_DEBUG "[%s] chip select number = %d\n", MODULE_NAME, cs);
	#endif
	flag |= 1 << (cs+16);
	if(cs > 5) flag <<= 4;

	// get chip select range for FPGA
	devNode = getNode("fsl,mpc5200b-lpb");
	if(!devNode) return -1;
	propRanges = of_get_property(devNode, "ranges", &len);
	csStartAddr = propRanges[cs*4+2];
	of_node_put(devNode);
	csSize = propRanges[cs*4+3];
	#if defined(DEBUG)
		printk(KERN_DEBUG "[%s] chip select start address = 0x%x\n", MODULE_NAME, csStartAddr);
		printk(KERN_DEBUG "[%s] chip select size = 0x%x\n", MODULE_NAME, csSize);
	#endif

	// set chip select range for FPGA
	devNode = getNode("fsl,mpc5200b-immr");
	if(!devNode) return -1;
	addrPtr = of_get_address(devNode, 0, &size64, NULL);
	addr64 = of_translate_address(devNode, addrPtr);
	of_node_put(devNode);
	if(request_mem_region((u32)addr64, (u32)size64, MODULE_NAME) == NULL) {
		printk(KERN_ERR "[%s] ERROR: I/O request memory region MMR failed!\n", MODULE_NAME);
		return -1;
	}
	#if defined(DEBUG)
		printk(KERN_DEBUG "[%s] iomap from 0x%x, size 0x%x\n", MODULE_NAME, (u32)addr64, (u32)size64);
	#endif
	immr = ioremap((u32)addr64, (u32)size64);
	immr->ipbi_ws_ctrl |= flag;
	iounmap(immr);
	immr = NULL;
	#if defined(DEBUG)
		printk(KERN_DEBUG "[%s] unmapping IMMR memory region\n", MODULE_NAME);
	#endif
	#if defined(DEBUG)
		printk(KERN_DEBUG "[%s] iomap from 0x%x, size 0x%x\n", MODULE_NAME, (u32)addr64, (u32)size64);
	#endif
	csPtr = ioremap((u32)addr64, (u32)size64);
	ptr = csPtr + 1 + cs*2;
	if(cs > 5) ptr += 9;
	out_be32(ptr, csStartAddr >> 16);
	out_be32(ptr+1, (csStartAddr + csSize - 1) >> 16);
	iounmap(csPtr);
	csPtr = NULL;
	#if defined(DEBUG)
		printk(KERN_DEBUG "[%s] unmapping IMMR memory region\n", MODULE_NAME);
	#endif
	release_mem_region((u32)addr64, (u32)size64);

	// set chip select properties in chip select controller for FPGA
	devNode = getNode("fsl,mpc5200b-csc");
	if(!devNode) return -1;
	addrPtr = of_get_address(devNode, 0, &size64, NULL);
	addr64 = of_translate_address(devNode, addrPtr);
	of_node_put(devNode);
	if(request_mem_region((u32)addr64, (u32)size64, MODULE_NAME) == NULL) {
		printk(KERN_ERR "[%s] ERROR: I/O request memory region CSC failed!\n", MODULE_NAME);
		return -1;
	}
	#if defined(DEBUG)
		printk(KERN_DEBUG "[%s] iomap from 0x%x, size 0x%x\n", MODULE_NAME, (u32)addr64, (u32)size64);
	#endif
	csctl = ioremap((u32)addr64, (u32)size64);
	printk(KERN_DEBUG "[%s] csctl.cs3 = 0x%x\n", MODULE_NAME, csctl->cs3_cr);
	iounmap(csctl);
	csctl = NULL;
	#if defined(DEBUG)
		printk(KERN_DEBUG "[%s] unmapping CSCTL memory region\n", MODULE_NAME);
	#endif
	#if defined(DEBUG)
		printk(KERN_DEBUG "[%s] iomap from 0x%x, size 0x%x\n", MODULE_NAME, (u32)addr64, (u32)size64);
	#endif
	csPtr = ioremap((u32)addr64, (u32)size64);
	ptr = csPtr + cs;
	if(cs > 5) ptr += 2;
	out_be32(ptr, 0x5ff00);
	iounmap(csPtr);
	csPtr = NULL;
	#if defined(DEBUG)
		printk(KERN_DEBUG "[%s] unmapping CSCTL memory region\n", MODULE_NAME);
	#endif
	release_mem_region((u32)addr64, (u32)size64);

	// setup FPGA memory region
	lpb_data->base_address = csStartAddr;
	lpb_data->mem_size = dev_mem_length;
	if(request_mem_region(csStartAddr, dev_mem_length, MODULE_NAME) == NULL) {
		printk(KERN_ERR "[%s] ERROR: I/O request memory region for FPGA (addr=0x%x, size=0x%x) failed!\n", MODULE_NAME, csStartAddr, dev_mem_length);
		return -1;
	}
	#if defined(DEBUG)
		printk(KERN_DEBUG "[%s] iomap from 0x%x, size 0x%x\n", MODULE_NAME, csStartAddr, dev_mem_length);
	#endif
	lpb_data->base_ptr = ioremap(csStartAddr, dev_mem_length);

	// setup flink devices
	flink_device_init(fdev, &lpb_bus_ops, THIS_MODULE);
	fdev->bus_data = lpb_data;
	flink_device_add(fdev);
	return 0;
}

static int driver_remove(struct platform_device *ofdev) {
	struct flink_device* fdev;
	struct flink_device* fdev_next;
	struct flink_lpb_data* lpb_data;

	list_for_each_entry_safe(fdev, fdev_next, flink_get_device_list(), list) {
		if(fdev->appropriated_module == THIS_MODULE) {
			lpb_data = (struct flink_lpb_data*)(fdev->bus_data);
			iounmap(lpb_data->base_ptr);
			#if defined(DEBUG)
				printk(KERN_DEBUG "[%s] unmapping FPGA memory region (addr=0x%x, size=0x%x)\n", MODULE_NAME, (u32)lpb_data->base_address, (u32)lpb_data->mem_size);
			#endif
			release_mem_region(lpb_data->base_address,lpb_data->mem_size);
			kfree(lpb_data);
			flink_device_remove(fdev);
			flink_device_delete(fdev);
		}
	}
	return 0;
}

static void dev_release (struct device *dev) {}

// ############ Data structures for platform driver and device ############

/* Device tree match table for this device */
static struct of_device_id flink_device_ids[] = {
	{ .compatible = "ntb,flink_driver" },
	{}
};

MODULE_DEVICE_TABLE(of, flink_device_ids);

static struct platform_driver flink_lpb_driver = {
	.probe = flink_probe,	// is called when registering device
	.remove = driver_remove,
	.driver = {
		.name = "flink_lpb_device",
		.owner = THIS_MODULE,
		.of_match_table = flink_device_ids,
	},
};

static struct platform_device flink_lpb_device = {
	.name = "flink_lpb_device",
	.id = -1,
	.dev.release	= dev_release,	// is called when unregistering device
};

// ############ Initialization and cleanup ############
static int __init mod_init(void) {
	int err = 0;

	#if defined(DEBUG)
		printk(KERN_DEBUG "[%s] Registering device driver\n", MODULE_NAME);
	#endif

	err = platform_device_register(&flink_lpb_device);
	if (err) {
		printk(KERN_ERR "Cannot register device\n");
		goto exit;
	}

	err = platform_driver_register(&flink_lpb_driver);
	if (err) {
		printk(KERN_ERR "Unable to register [%s]: device driver: %d\n", MODULE_NAME, err);
		goto exit_unregister_device;
	}

	printk(KERN_INFO "[%s] Module sucessfully loaded\n", MODULE_NAME);
	return 0;

exit_unregister_device:
	platform_device_unregister(&flink_lpb_device);
exit:
	return err;
}

static void __exit mod_exit(void) {
	#if defined(DEBUG)
		printk(KERN_DEBUG "[%s] Unloading device driver\n", MODULE_NAME);
	#endif
	platform_driver_unregister(&flink_lpb_driver);
	platform_device_unregister(&flink_lpb_device);
	printk(KERN_INFO "[%s] Module sucessfully unloaded\n", MODULE_NAME);
}

module_init(mod_init);
module_exit(mod_exit);
