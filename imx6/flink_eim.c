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
 *  iMX6 EIM (External Interface Module) communication module      *
 *                                                                 *
 *******************************************************************/

/** @file flink_eim.c
 *  @brief iMX6 EIM (External Interface Module) communication module.
 *
 *  Implements read and write functions over EIM bus.
 *
 *  @author Adam Bajric
 */


#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/of_address.h>

#include "../flink.h"
#include "../flink_debug.h"



// ####### macros ##############################################################

#define MOD_VERSION "0.1.0"
#define DEBUG



// ####### function prototypes #################################################

static int flink_eim_probe(struct platform_device *pdev);
static int flink_eim_remove(struct platform_device *pdev);
static void flink_eim_dev_release(struct device *dev);

static u8  flink_eim_read8(struct flink_device* fdev, u32 addr);
static u16 flink_eim_read16(struct flink_device* fdev, u32 addr);
static u32 flink_eim_read32(struct flink_device* fdev, u32 addr);
static int flink_eim_write8(struct flink_device* fdev, u32 addr, u8 val);
static int flink_eim_write16(struct flink_device* fdev, u32 addr, u16 val);
static int flink_eim_write32(struct flink_device* fdev, u32 addr, u32 val);
static u32 flink_eim_address_space_size(struct flink_device* fdev);



// ####### data structures #####################################################

static struct of_device_id flink_eim_of_match[] = {
	{ .compatible = "ntb,flink_eim_driver", },
	{ }
};
MODULE_DEVICE_TABLE(of, flink_eim_of_match);

static struct platform_driver flink_eim_platform_driver =
{
	.probe = flink_eim_probe,
	.remove = flink_eim_remove,
	.driver = {
		.name = "flink_eim_driver",
		.owner = THIS_MODULE,
		.of_match_table = flink_eim_of_match,
	},
};

static struct platform_device flink_eim_device =
{
	.name = "flink_eim_device",
	.id = -1,
	.dev.release = flink_eim_dev_release,
};

struct flink_bus_ops flink_eim_bus_ops =
{
	.read8              = flink_eim_read8,
	.read16             = flink_eim_read16,
	.read32             = flink_eim_read32,
	.write8             = flink_eim_write8,
	.write16            = flink_eim_write16,
	.write32            = flink_eim_write32,
	.address_space_size = flink_eim_address_space_size
};

struct flink_eim_bus_data
{
	void __iomem *base;
	resource_size_t start;
	resource_size_t size;
};



// ####### module initialization and cleanup  ##################################

static int __init mod_init(void)
{
	int err = 0;

	printkd("loading module " MOD_VERSION);

	err = platform_device_register(&flink_eim_device);
	if (err) {
		printke("cannot register device");
		goto exit;
	}

	err = platform_driver_register(&flink_eim_platform_driver);
	if (err) {
		printke("unable to register driver: %d", err);
		goto exit_unregister_device;
	}

	return 0;

exit_unregister_device:
	platform_device_unregister(&flink_eim_device);
exit:
	return err;
}
module_init(mod_init);


static void __exit mod_exit(void)
{
	printkd("unloading module");
	platform_driver_unregister(&flink_eim_platform_driver);
	platform_device_unregister(&flink_eim_device);
}
module_exit(mod_exit);



// ####### platform driver probe and remove ####################################

static int flink_eim_probe(struct platform_device *pdev)
{
	int err = 0;
	struct resource res;
	struct flink_device *fdev;
	struct flink_eim_bus_data *bus_data;
	const struct of_device_id* match = of_match_device(flink_eim_of_match, &pdev->dev);
	
	// sanity check
	if (!match) {
		printka("wrong device");
		err = -ENODEV;
		goto match_failure;
	}

	bus_data = kmalloc(sizeof(struct flink_eim_bus_data), GFP_KERNEL);
	if (!bus_data) {
		err = -ENOMEM;
		goto bus_data_alloc_failure;
	}

	fdev = flink_device_alloc();
	if (!fdev) {
		err = -ENOMEM;
		goto fdev_alloc_failure;
	}
	
	// get resource from device tree
	err = of_address_to_resource(pdev->dev.of_node, 0, &res);
	if (err) {
		printka("cannot get resource from device tree");
		goto ressource_failure;
	}

	bus_data->start = res.start;
	bus_data->size = resource_size(&res);

	// request memory region
	if (!request_mem_region(bus_data->start, bus_data->size, "flink_eim_driver")) {
		printka("failed to request memory region");
		err = -ENOMEM;
		goto mem_request_failure;
	}
	
	// remap memory region
	bus_data->base = of_iomap(pdev->dev.of_node, 0);
	if (!bus_data->base) {
		printka("failed to remap memory");
		err = -ENOMEM;
		goto mem_iomap_failure;
	}

	// setup flink device
	flink_device_init(fdev, &flink_eim_bus_ops, THIS_MODULE);
	fdev->bus_data = bus_data;
	flink_device_add(fdev);

	return 0;
	
mem_iomap_failure:
	release_mem_region(bus_data->start, bus_data->size);
mem_request_failure:
	// nothing to cleanup
ressource_failure:
	flink_device_delete(fdev);
fdev_alloc_failure:
	kfree(bus_data);
bus_data_alloc_failure:
	// nothing to cleanup
match_failure:
	// nothing to cleanup
	return err;
}

static int flink_eim_remove(struct platform_device *pdev)
{
	struct flink_device *fdev;
	struct flink_device *fdev_next;
	struct flink_eim_bus_data *bus_data;

	list_for_each_entry_safe(fdev, fdev_next, flink_get_device_list(), list) {
		if(fdev->appropriated_module == THIS_MODULE) {
			bus_data = (struct flink_eim_bus_data *)(fdev->bus_data);
			flink_device_remove(fdev);
			flink_device_delete(fdev);
			iounmap(bus_data->base);
			release_mem_region(bus_data->start, bus_data->size);
			kfree(bus_data);
		}
	}

	return 0;
}


static void flink_eim_dev_release(struct device *dev) { }



// ####### flink bus operations ################################################

static u8 flink_eim_read8(struct flink_device* fdev, u32 addr)
{
	struct flink_eim_bus_data* d = (struct flink_eim_bus_data*)fdev->bus_data;
	if (d != NULL) {
		return (u8)ioread32(d->base + addr);
	}
	return 0;
}

static u16 flink_eim_read16(struct flink_device* fdev, u32 addr)
{
	struct flink_eim_bus_data* d = (struct flink_eim_bus_data*)fdev->bus_data;
	if (d != NULL) {
		return (u16)ioread32(d->base + addr);
	}
	return 0;
}

static u32 flink_eim_read32(struct flink_device* fdev, u32 addr)
{
	struct flink_eim_bus_data* d = (struct flink_eim_bus_data*)fdev->bus_data;
	if (d != NULL) {
		return ioread32(d->base + addr);
	}
	return 0;
}

static int flink_eim_write8(struct flink_device* fdev, u32 addr, u8 val)
{
	struct flink_eim_bus_data* d = (struct flink_eim_bus_data*)fdev->bus_data;
	if (d != NULL) {
		u32 v = ((flink_eim_read32(fdev, addr) & 0xff000000) | val);
		iowrite32(v, d->base + addr);
	}
	return 0;
}

static int flink_eim_write16(struct flink_device* fdev, u32 addr, u16 val)
{
	struct flink_eim_bus_data* d = (struct flink_eim_bus_data*)fdev->bus_data;
	if (d != NULL) {
		u32 v = ((flink_eim_read32(fdev, addr) & 0xffff0000) | val);
		iowrite32(v, d->base + addr);
	}
	return 0;
}

static int flink_eim_write32(struct flink_device* fdev, u32 addr, u32 val)
{
	struct flink_eim_bus_data* d = (struct flink_eim_bus_data*)fdev->bus_data;
	if (d != NULL) {
		iowrite32(val, d->base + addr);
	}
	return 0;
}

static u32 flink_eim_address_space_size(struct flink_device* fdev)
{
	struct flink_eim_bus_data* d = (struct flink_eim_bus_data*)fdev->bus_data;
	return (u32)(d->size);
}



// ####### module infos ########################################################

MODULE_DESCRIPTION("flink EIM module for iMX6");
MODULE_VERSION(MOD_VERSION);
MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Adam Bajric <adam.bajric@ntb.ch>");

