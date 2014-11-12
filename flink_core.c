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
 *  Core Module                                                    *
 *                                                                 *
 *******************************************************************/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <asm/uaccess.h>

#include "flink.h"

#define DBG 0
#define MODULE_NAME THIS_MODULE->name
#define SYSFS_CLASS_NAME "flink"
#define MAX_DEV_NAME_LENGTH 15

MODULE_AUTHOR("Martin Zueger <martin@zueger.eu>");
MODULE_DESCRIPTION("fLink core module");
MODULE_SUPPORTED_DEVICE("none");
MODULE_LICENSE("Dual BSD/GPL");

static LIST_HEAD(device_list);
static LIST_HEAD(loaded_if_modules);
static struct class* sysfs_class;

// ############ File operations ############

int flink_open(struct inode* i, struct file* f) {
	struct flink_device* fdev = flink_get_device_by_cdev(i->i_cdev);
	struct flink_private_data* p_data = kmalloc(sizeof(struct flink_private_data), GFP_KERNEL);
	memset(p_data, 0, sizeof(*p_data));
	p_data->fdev = fdev;
	f->private_data = p_data;
	#if defined(DBG)
		printk(KERN_DEBUG "[%s] Device node opened.", MODULE_NAME);
	#endif
	return 0;
}

int flink_relase(struct inode* i, struct file* f) {
	kfree(f->private_data);
	#if defined(DBG)
		printk(KERN_DEBUG "[%s] Device node closed.", MODULE_NAME);
	#endif
	return 0;
}

ssize_t flink_read(struct file* f, char __user* data, size_t size, loff_t* offset) {
	struct flink_private_data* pdata = (struct flink_private_data*)(f->private_data);
	#if defined(DBG)
		printk(KERN_DEBUG "[%s] Reading from device...", MODULE_NAME);
	#endif
	if(pdata != NULL && pdata->current_subdevice != NULL) {
		struct flink_subdevice* subdev = pdata->current_subdevice;
		struct flink_device* fdev = subdev->parent;
		u32 rdata = 0;
		u32 roffset = 0;
		unsigned long rsize = 0;
		#if defined(DBG)
			printk(KERN_DEBUG "  -> Device: %u/%u", fdev->id, subdev->id);
			printk(KERN_DEBUG "  -> Size:   0x%x (%u bytes)", size, size);
			printk(KERN_DEBUG "  -> Offset: 0x%x", (u32)*offset);
		#endif
		switch(size) {
			case 1:
				// TODO
				return 0;
			case 2:
				// TODO
				return 0;
			case 4:
				rdata = 0;
				roffset = (u32)*offset;
				if(roffset > subdev->mem_size) {
					return 0;
				}
				rdata = fdev->bus_ops->read32(fdev, subdev->base_addr + roffset);
				rsize = copy_to_user(data, &rdata, sizeof(rdata));
				if(rsize > 0) {
					#if defined(DBG)
						printk(KERN_DEBUG "  -> Copying to user space failed: %lu bytes not copied!", rsize);
					#endif
					return 0;
				}
				#if defined(DBG)
					printk(KERN_DEBUG "  -> Value:  0x%x", rdata);
				#endif
				return sizeof(rdata);
			default:
				if(size % 4 == 0) {
					// TODO
				}
				return 0;
		}
	}
	return 0;
}

ssize_t flink_write(struct file* f, const char __user* data, size_t size, loff_t* offset) {
	struct flink_private_data* pdata = (struct flink_private_data*)(f->private_data);
	#if defined(DBG)
		printk(KERN_DEBUG "[%s] Writing to device...", MODULE_NAME);
	#endif
	if(pdata != NULL && pdata->current_subdevice != NULL) {
		struct flink_subdevice* subdev = pdata->current_subdevice;
		struct flink_device* fdev = subdev->parent;
		u32 wdata = 0;
		u32 woffset = 0;
		unsigned long wsize = 0;
		#if defined(DBG)
			printk(KERN_DEBUG "  -> Device: %u/%u", fdev->id, subdev->id);
			printk(KERN_DEBUG "  -> Size:   0x%x (%u bytes)", size, size);
			printk(KERN_DEBUG "  -> Offset: 0x%x", (u32)*offset);
		#endif
		switch(size) {
			case 1:
				// TODO
				return 0;
			case 2:
				// TODO
				return 0;
			case 4:
				woffset = (u32)*offset;
				if(woffset > subdev->mem_size) {
					return 0;
				}
				wsize = copy_from_user(&wdata, data, sizeof(wdata));
				if(wsize > 0) {
					#if defined(DBG)
						printk(KERN_DEBUG "  -> Copying from user space failed: %lu bytes not copied!", wsize);
					#endif
					return 0;
				}
				fdev->bus_ops->write32(fdev, subdev->base_addr + woffset, wdata);
				#if defined(DBG)
					printk(KERN_DEBUG "  -> Value:  0x%x", wdata);
				#endif
				return sizeof(wdata);
			default:
				if(size % 4 == 0) {
					// TODO
				}
				return 0;
		}
	}
	return 0;
}

long flink_ioctl(struct file* f, unsigned int cmd, unsigned long arg) {
	int error = 0;
	struct flink_private_data* pdata = (struct flink_private_data*)(f->private_data);
	struct flink_subdevice* src;
	u8 id;
	struct ioctl_bit_container_t rwbit_container;
	u32 temp;
	
	#if defined(DBG)
		printk(KERN_DEBUG "[%s] I/O control call...", MODULE_NAME);
	#endif
	switch(cmd) {
		case SELECT_SUBDEVICE:
			#if defined(DBG)
				printk(KERN_DEBUG "  -> SELECT_SUBDEVICE (0x%x)", SELECT_SUBDEVICE);
			#endif
			error = copy_from_user(&id, (void __user *)arg, sizeof(id));
			if(error != 0) {
				#if defined(DBG)
					printk(KERN_DEBUG "  -> Error while copying from userspace: %i", error);
				#endif
				return -EINVAL;
			}
			return flink_select_subdevice(f, id, 0);
		case SELECT_SUBDEVICE_EXCL:
			#if defined(DBG)
				printk(KERN_DEBUG "  -> SELECT_SUBDEVICE_EXCL (0x%x)", SELECT_SUBDEVICE_EXCL);
			#endif
			error = copy_from_user(&id, (void __user *)arg, sizeof(id));
			if(error != 0) {
				#if defined(DBG)
					printk(KERN_DEBUG "  -> Error while copying from userspace: %i", error);
				#endif
				return -EINVAL;
			}
			return flink_select_subdevice(f, id, 1);
		case READ_NOF_SUBDEVICES:
			#if defined(DBG)
				printk(KERN_DEBUG "  -> READ_NOF_SUBDEVICES (0x%x) -> %u", READ_NOF_SUBDEVICES, pdata->fdev->nof_subdevices);
			#endif
			error = copy_to_user((void __user *)arg, &(pdata->fdev->nof_subdevices), sizeof(pdata->fdev->nof_subdevices));
			if(error != 0) {
				#if defined(DBG)
					printk(KERN_DEBUG "  -> Error while copying to userspace: %i", error);
				#endif
				return -EINVAL;
			}
			break;
		case READ_SUBDEVICE_INFO:
			#if defined(DBG)
				printk(KERN_DEBUG "  -> READ_SUBDEVICE_INFO (0x%x)", READ_SUBDEVICE_INFO);
			#endif
			error = copy_from_user(&id, (void __user *)arg, sizeof(id));
			if(error != 0) {
				#if defined(DBG)
					printk(KERN_DEBUG "  -> Error while copying from userspace: %i", error);
				#endif
				return -EINVAL;
			}
			if(id >= pdata->fdev->nof_subdevices) {
				#if defined(DBG)
					printk(KERN_DEBUG "  -> Illegal subdevice id");
				#endif
				return -EINVAL;
			}
			src = flink_get_subdevice_by_id(pdata->fdev, id);
			if(src == NULL) {
				#if defined(DBG)
					printk(KERN_DEBUG "  -> Getting kernel subdevice structure failed.");
				#endif
				return -EINVAL;
			}
			error = copy_to_user((void __user *)arg, &(src->id), FLINKLIB_SUBDEVICE_SIZE);
			if(error != 0) {
				#if defined(DBG)
					printk(KERN_DEBUG "  -> Error while copying to userspace: %i", error);
				#endif
				return -EINVAL;
			}
			break;
		case READ_SINGLE_BIT:
			#if defined(DBG)
				printk(KERN_DEBUG "  -> READ_SINGLE_BIT (0x%x)", READ_SINGLE_BIT);
			#endif
			error = copy_from_user(&rwbit_container, (void __user *)arg, sizeof(rwbit_container));
			if(error != 0) {
				#if defined(DBG)
					printk(KERN_DEBUG "  -> Error while copying from userspace: %i", error);
				#endif
				return -EINVAL;
			}
			temp = pdata->fdev->bus_ops->read32(pdata->fdev, pdata->current_subdevice->base_addr + rwbit_container.offset);
			#if defined(DBG)
				printk(KERN_DEBUG "  -> Read from device: 0x%x", temp);
			#endif
			rwbit_container.value = ((temp & (1 << rwbit_container.bit)) != 0);
			#if defined(DBG)
				printk(KERN_DEBUG "  -> Bit value: 0x%x", rwbit_container.value);
			#endif
			error = copy_to_user((void __user *)arg, &rwbit_container, sizeof(rwbit_container));
			if(error != 0) {
				#if defined(DBG)
					printk(KERN_DEBUG "  -> Error while copying to userspace: %i", error);
				#endif
				return -EINVAL;
			}
			break;
		case WRITE_SINGLE_BIT:
			#if defined(DBG)
				printk(KERN_DEBUG "  -> WRITE_SINGLE_BIT (0x%x)", WRITE_SINGLE_BIT);
			#endif
			error = copy_from_user(&rwbit_container, (void __user *)arg, sizeof(rwbit_container));
			if(error != 0) {
				#if defined(DBG)
					printk(KERN_DEBUG "  -> Error while copying from userspace: %i", error);
				#endif
				return -EINVAL;
			}
			else {
				#if defined(DBG)
					printk(KERN_DEBUG "  -> Copied from user space: offset = 0x%x, bit = %u, value = %u", rwbit_container.offset, rwbit_container.bit, rwbit_container.value);
				#endif
			}
			temp = pdata->fdev->bus_ops->read32(pdata->fdev, pdata->current_subdevice->base_addr + rwbit_container.offset);
			#if defined(DBG)
				printk(KERN_DEBUG "  -> Read from device: 0x%x", temp);
			#endif
			if(rwbit_container.value != 0) { // set bit
				temp |= (1 << rwbit_container.bit);
				#if defined(DBG)
					printk(KERN_DEBUG "  -> Setting bit by writing 0x%x to device", temp);
				#endif
			}
			else { // clear bit
				temp &= ~(1 << rwbit_container.bit);
				#if defined(DBG)
					printk(KERN_DEBUG "  -> Clearing bit by writing 0x%x to device", temp);
				#endif
			}
			pdata->fdev->bus_ops->write32(pdata->fdev, pdata->current_subdevice->base_addr + rwbit_container.offset, temp);
			break;
		default:
			#if defined(DBG)
				printk(KERN_DEBUG "  -> Error: illegal ioctl command: 0x%x!", cmd);
			#endif
			return -EINVAL;
    }
	return 0;
}

loff_t flink_llseek(struct file* f, loff_t off, int whence) {
	struct flink_private_data* pdata = (struct flink_private_data*)(f->private_data);
	#if defined(DBG)
		printk(KERN_DEBUG "[%s] llseek call...", MODULE_NAME);
	#endif
	if(pdata != NULL && pdata->current_subdevice != NULL) {
		loff_t newpos;
		switch(whence) {
			case 0: /* SEEK_SET */
				newpos = off;
				break;
			case 1: /* SEEK_CUR */
				newpos = f->f_pos + off;
				break;
			case 2: /* SEEK_END */
				newpos = pdata->current_subdevice->mem_size + off;
				break;
			default: /* can't happen */
				return -EINVAL;
		}
		if(newpos < 0) return -EINVAL;
		f->f_pos = newpos;
		#if defined(DBG)
			printk(KERN_DEBUG "  -> new position: 0x%x", (u32)newpos);
		#endif
		return newpos;
	}
	return -EINVAL;
}

struct file_operations flink_fops = {
	.owner          = THIS_MODULE,
	.open           = flink_open,
	.release        = flink_relase,
	.read           = flink_read,
	.write          = flink_write,
	.unlocked_ioctl = flink_ioctl,
	.llseek         = flink_llseek
};

// ############ Initialization ############
static int __init flink_init(void) {
	int error = 0;
	
	// Create sysfs class
	sysfs_class = class_create(THIS_MODULE, SYSFS_CLASS_NAME);
	
	// ---- All done ----
	printk(KERN_INFO "[%s] Module sucessfully loaded", MODULE_NAME);
	
	return 0;

	// ---- ERROR HANDLING ----
	
	return error;
}
module_init(flink_init);

// ############ Cleanup ############
static void __exit flink_exit(void) {
	// Destroy sysfs class
	class_destroy(sysfs_class);
	
	// ---- All done ----
	printk(KERN_INFO "[%s] Module sucessfully unloaded", MODULE_NAME);
}
module_exit(flink_exit);

// ############ Device and module handling functions ############

/**
 * create_device_node() - creates a device node for a flink device
 * @fdev: the flink device to create a device node for
 */
static int create_device_node(struct flink_device* fdev) {
	static unsigned int dev_counter = 0;
	int error = 0;
	dev_t dev;
	
	// Allocate, register and initialize char device
	error = alloc_chrdev_region(&dev, dev_counter, 1, MODULE_NAME);
	if(error) {
		printk(KERN_ERR "[%s] Allocation of char dev region failed!", MODULE_NAME);
		goto alloc_chardev_region_failed;
	}
	fdev->char_device = cdev_alloc();
	if(fdev->char_device == NULL) {
		printk(KERN_ERR "[%s] Allocation of char dev failed!", MODULE_NAME);
		goto cdev_alloc_failed;
	}
	cdev_init(fdev->char_device, &flink_fops);
	fdev->char_device->owner = THIS_MODULE;
	error = cdev_add(fdev->char_device, dev, 1);
	if(error) {
		printk(KERN_ERR "[%s] Adding the char dev to the system failed!", MODULE_NAME);
		goto cdev_add_failed;
	}
	
	// create device node
	fdev->sysfs_device = device_create(sysfs_class, NULL, dev, NULL, "flink%u", dev_counter);
	if(IS_ERR(fdev->sysfs_device)) {
		printk(KERN_ERR "[%s] Creation of sysfs device failed!", MODULE_NAME);
		goto device_create_failed;
	}
	
	#if defined(DBG)
		printk(KERN_DEBUG "[%s] Device node created: flink%u", MODULE_NAME, dev_counter);
	#endif
	
	dev_counter++;
	
	return 0;
	
	// Cleanup on error
device_create_failed:
	cdev_del(fdev->char_device);
		
cdev_add_failed:
	fdev->char_device = NULL;

cdev_alloc_failed:
	unregister_chrdev_region(dev, 1);
		
alloc_chardev_region_failed:
	// nothing to do
		
	return error;
}

/**
 * scan_for_subdevices() - scan flink device for subdevices
 * @fdev: the flink device to scan
 *
 * Scans the device for available subdevices and adds them to
 * the device structure. The number of added subdevices is returned.
 */
static unsigned int scan_for_subdevices(struct flink_device* fdev) {
	unsigned int subdevice_counter = 0;
	u32 current_address = 0;
	u32 last_address = current_address + fdev->bus_ops->address_space_size(fdev) - 1;
	u32 current_type = 0;
	u32 current_mem_size = 0;
	struct flink_subdevice* new_subdev;
	
	#if defined(DBG)
		printk(KERN_DEBUG "[%s] Scanning device #%u for subdevices...", MODULE_NAME, fdev->id);
		printk(KERN_DEBUG "  -> Start address:      0x%x", current_address);
		printk(KERN_DEBUG "  -> Last valid address: 0x%x", last_address);
	#endif
	while(current_address < last_address && subdevice_counter < MAX_NOF_SUBDEVICES) {
		current_type = (fdev->bus_ops->read32(fdev, current_address + SUBDEV_TYPE_OFFSET));
		current_mem_size = fdev->bus_ops->read32(fdev, current_address + SUBDEV_SIZE_OFFSET);
		if(current_mem_size > MAIN_HEADER_SIZE + SUB_HEADER_SIZE) {
			// Create and initialize new subdevice
			new_subdev = flink_subdevice_alloc();
			flink_subdevice_init(new_subdev);
			new_subdev->type_id = (u16)(current_type >> 16);
			new_subdev->sub_type_id = (u8)((current_type >> 8) & 0xFF);
			new_subdev->if_version = (u8)(current_type & 0xFF);
			new_subdev->base_addr = current_address;
			new_subdev->mem_size = current_mem_size;
			new_subdev->nof_channels = fdev->bus_ops->read32(fdev, current_address + SUBDEV_NOFCHANNELS_OFFSET);
			
			// Add subdevice to flink device
			flink_subdevice_add(fdev, new_subdev);
			subdevice_counter++;
			
			// Increment Address counter and reset temp variables
			current_address += current_mem_size;
			current_type = 0;
			current_mem_size = 0;
		}
		else {
			break;
		}
	}
	return subdevice_counter;
}

/**
 * flink_device_alloc() - allocate a flink_device structure
 *
 * Allocates and returns a flink_device structure, or NULL on failure.
 */
struct flink_device* flink_device_alloc(void) {
	struct flink_device* fdev = kmalloc(sizeof(struct flink_device), GFP_KERNEL);
	if(fdev) {
		INIT_LIST_HEAD(&(fdev->list));
	}
	return fdev;
}

/**
 * flink_device_init() - initialize a flink_device structure
 * @fdev: the structure to initialize
 * @bops: the flink_bus_ops for this device
 *
 * Initializes @fdev, remembering @bops, making it ready to add to the
 * system with flink_device_add().
 */
void flink_device_init(struct flink_device* fdev, struct flink_bus_ops* bus_ops, struct module* mod) {
	memset(fdev, 0, sizeof(*fdev));
	INIT_LIST_HEAD(&(fdev->list));
	INIT_LIST_HEAD(&(fdev->subdevices));
	fdev->bus_ops = bus_ops;
	fdev->appropriated_module = mod;
}

/**
 * flink_device_add() - add a flink device to the system
 * @fdev: the flink_device structure to register
 *
 * flink_device_add() adds the device represented by @fdev to the system, making it
 * live immediately. A negative error code is returned on failure.
 */
int flink_device_add(struct flink_device* fdev) {
	static unsigned int dev_counter = 0;
	unsigned int nof_subdevices = 0;
	if(fdev != NULL) {
		// Add device to list
		fdev->id = dev_counter++;
		list_add(&(fdev->list), &device_list);
		#if defined(DBG)
			printk(KERN_DEBUG "[%s] Device with id '%u' added to device list.", MODULE_NAME, fdev->id);
		#endif
		
		// Scan for subdevices
		nof_subdevices = scan_for_subdevices(fdev);
		#if defined(DBG)
			printk(KERN_DEBUG "[%s] %u subdevice(s) added to device #%u", MODULE_NAME, nof_subdevices, fdev->id);
		#endif
		
		// Create device node
		create_device_node(fdev);
		
		return fdev->id;
	}
	return UNKOWN_ERROR;
}

/**
 * flink_device_remove() - remove a flink device from the system
 * @fdev: the flink_device structure to remove
 *
 * flink_device_remove() removes the device represented by @fdev from
 * the system. A negative error code is returned on failure.
 */
int flink_device_remove(struct flink_device* fdev) {
	if(fdev != NULL) {
		
		// Remove device from list
		list_del(&(fdev->list));
		#if defined(DBG)
			printk(KERN_DEBUG "[%s] Device with id '%u' removed frome device list.", MODULE_NAME, fdev->id);
		#endif
		
		// Destroy device node and free char dev region
		device_destroy(sysfs_class, fdev->char_device->dev);
		cdev_del(fdev->char_device);
		unregister_chrdev_region(fdev->char_device->dev, 1);
		
		return 0;
	}
	return UNKOWN_ERROR;
}

/**
 * flink_device_delete() - deletes a flink device
 * @fdev: the flink_device structure to delete
 *
 * flink_device_delete() deletes the device represented by @fdev and
 * frees the allocated memory. All subdevices will be deleted,
 * using flink_subdevice_remove() and flink_subdevice_delete. A
 * negative error code is returned on failure.
 */
int flink_device_delete(struct flink_device* fdev) {
	if(fdev != NULL) {
		struct flink_subdevice* sdev;
		struct flink_subdevice* sdev_next;
		
		// Remove and delete all subdevices
		list_for_each_entry_safe(sdev, sdev_next, &(fdev->subdevices), list) {
			#if defined(DBG)
				printk(KERN_DEBUG "[%s] Removing and deleting subdevice #%u (from device #%u)", MODULE_NAME, sdev->id, fdev->id);
			#endif
			flink_subdevice_remove(sdev);
			flink_subdevice_delete(sdev);
		}
		
		// Free memory
		kfree(fdev);
		
		return 0;
	}
	return UNKOWN_ERROR;
}

/**
 * flink_get_device_by_id() - get a flink device by id
 * @id: the id of the flink device
 *
 * flink_get_device_by_id() returns the flink device structure
 * with the given id. NULL is returned if no device is found
 * with the given id.
 */
struct flink_device* flink_get_device_by_id(u8 id) {
	struct flink_device* fdev;
	#if defined(DBG)
		printk(KERN_DEBUG "[%s] Looking for device with id '%u'...", MODULE_NAME, id);
	#endif
	list_for_each_entry(fdev, &device_list, list) {
		if(fdev->id == id) {
			#if defined(DBG)
				printk(KERN_DEBUG "[%s] Device with id '%u' found!", MODULE_NAME, id);
			#endif
			return fdev;
		}
	}
	#if defined(DBG)
		printk(KERN_DEBUG "[%s] No device with id '%u' found!", MODULE_NAME, id);
	#endif
	return NULL;
}

/**
 * flink_get_device_by_cdev() - get a flink device by cdev
 * @char_device: pointer to the cdev struct of the flink device
 *
 * flink_get_device_by_cdev() returns the flink device structure
 * associated with the given cdev. NULL is returned if no suitable
 * device is found.
 */
struct flink_device* flink_get_device_by_cdev(struct cdev* char_device) {
	struct flink_device* fdev;
	#if defined(DBG)
		printk(KERN_DEBUG "[%s] Looking for device with cdev '%p'...", MODULE_NAME, char_device);
	#endif
	list_for_each_entry(fdev, &device_list, list) {
		if(fdev->char_device == char_device) {
			#if defined(DBG)
				printk(KERN_DEBUG "[%s] Device with cdev '%p' found!", MODULE_NAME, char_device);
			#endif
			return fdev;
		}
	}
	#if defined(DBG)
		printk(KERN_DEBUG "[%s] No device with cdev '%p' found!", MODULE_NAME, char_device);
	#endif
	return NULL;
}

/**
 * flink_get_device_list() - get the list with all devices
 *
 * flink_get_device_by_id() returns a pointer to a list containing
 * all flink devices. The list could be empty.
 */
struct list_head* flink_get_device_list() {
	return &device_list;
}

/**
 * flink_subdevice_alloc() - allocate a flink_subdevice structure
 *
 * Allocates and returns a flink_subdevice structure, or NULL on failure.
 */
struct flink_subdevice* flink_subdevice_alloc(void) {
	struct flink_subdevice* fsubdev = kmalloc(sizeof(struct flink_subdevice), GFP_KERNEL);
	if(fsubdev) {
		INIT_LIST_HEAD(&(fsubdev->list));
	}
	return fsubdev;
}

/**
 * flink_subdevice_init() - initialize a flink_subdevice structure
 * @fsubdev: the structure to initialize
 *
 * Initializes @fsubdev, making it ready to add to a flink_device
 * with flink_subdevice_add().
 */
void flink_subdevice_init(struct flink_subdevice* fsubdev) {
	memset(fsubdev, 0, sizeof(*fsubdev));
	INIT_LIST_HEAD(&(fsubdev->list));
}

/**
 * flink_subdevice_add() - add a flink subdevice to flink device
 * @fdev: the parent flink_device structure
 * @fsubdev: the flink_subdevice structure
 *
 * flink_subdevice_add() adds the subdevice represented by @fsubdev
 * to the parent flink device @fdev and making it available to use.
 * A negative error code is returned on failure.
 */
int flink_subdevice_add(struct flink_device* fdev, struct flink_subdevice* fsubdev) {
	if(fdev != NULL && fsubdev != NULL) {
		// Define subdevice id
		fsubdev->id = fdev->nof_subdevices++;
		
		// Set parent pointer
		fsubdev->parent = fdev;
		
		// Add subdevice to device
		list_add(&(fsubdev->list), &(fdev->subdevices));
		#if defined(DBG)
			printk(KERN_DEBUG "[%s] Subdevice with id '%u' added to device with id '%u'.", MODULE_NAME, fsubdev->id, fdev->id);
			printk(KERN_DEBUG "  -> Type:         0x%x/0x%x/0x%x", fsubdev->type_id, fsubdev->sub_type_id, fsubdev->if_version);
			printk(KERN_DEBUG "  -> Base address: 0x%x", fsubdev->base_addr);
			printk(KERN_DEBUG "  -> Size:         0x%x (%u bytes)", fsubdev->mem_size, fsubdev->mem_size);
			printk(KERN_DEBUG "  -> Nof Channels: %u", fsubdev->nof_channels);
		#endif
		
		// Register subdevice at interface module
		//if(fsubdev->register_hook != NULL) fsubdev->register_hook();
			
		return fsubdev->id;
	}
	return UNKOWN_ERROR;
}

/**
 * flink_subdevice_remove() - remove a flink subdevice from the parent device
 * @fsubdev: the flink_subdevice structure to remove
 *
 * flink_subdevice_remove() removes the subdevice represented by @fsubdev from
 * the parent device structure. A negative error code is returned on failure.
 */
int flink_subdevice_remove(struct flink_subdevice* fsubdev) {
	if(fsubdev != NULL) {
		
		// Remove device from list
		list_del(&(fsubdev->list));
		#if defined(DBG)
			printk(KERN_DEBUG "[%s] Subdevice with id '%u' removed from list.", MODULE_NAME, fsubdev->id);
		#endif
		
		// Delete parent pointer
		fsubdev->parent = NULL;
			
		return 0;
	}
	return UNKOWN_ERROR;
}

/**
 * flink_subdevice_delete() - deletes a flink subdevice
 * @fsubdev: the flink_subdevice structure to delete
 *
 * flink_subdevice_delete() deletes the subdevice represented by
 * @fsubdev and frees the allocated memory. A negative error
 * code is returned on failure.
 */
int flink_subdevice_delete(struct flink_subdevice* fsubdev) {
	if(fsubdev != NULL) {
		
		// Free memory
		kfree(fsubdev);
		
		return 0;
	}
	return UNKOWN_ERROR;
}

/**
 * flink_get_subdevice_by_id() - get a flink subdevice by id
 * @id: the id of the flink subdevice
 *
 * flink_get_subdevice_by_id() returns the flink subdevice structure
 * with the given id. NULL is returned if no subdevice is found
 * with the given id.
 */
struct flink_subdevice* flink_get_subdevice_by_id(struct flink_device* fdev, u8 id) {
	if(fdev != NULL) {
		struct flink_subdevice* subdev;
		#if defined(DBG)
			printk(KERN_DEBUG "[%s] Looking for subdevice with id '%u' in device %u...", MODULE_NAME, id, fdev->id);
		#endif
		list_for_each_entry(subdev, &(fdev->subdevices), list) {
			if(subdev->id == id) {
				#if defined(DBG)
					printk(KERN_DEBUG "[%s] Subdevice with id '%u' found!", MODULE_NAME, id);
				#endif
				return subdev;
			}
		}
		#if defined(DBG)
			printk(KERN_DEBUG "[%s] No subdevice with id '%u' found!", MODULE_NAME, id);
		#endif
	}
	return NULL;
}

/**
 * flink_get_sysfs_class() - returns the flink sysfs class.
 *
 * flink_get_sysfs_class() returns the flink sysfs class structure.
 */
struct class* flink_get_sysfs_class(void) {
	return sysfs_class;
}

int flink_select_subdevice(struct file* f, u8 subdevice, bool excl) {
	struct flink_private_data* pdata = (struct flink_private_data*)(f->private_data);
	if(pdata != NULL && pdata->fdev != NULL) {
		struct flink_device* fdev = pdata->fdev;
		pdata->current_subdevice = flink_get_subdevice_by_id(fdev, subdevice);
		// TODO exclusive access
		#if defined(DBG)
			printk(KERN_DEBUG "[%s] Selecting subdevice %u", MODULE_NAME, subdevice);
			if(excl) printk(KERN_DEBUG "  -> exclusive");
			else printk(KERN_DEBUG "  -> not exclusive");
		#endif
		return 0;
	}
	return UNKOWN_ERROR;
}

// ############ Let other modules do flink stuff ############
EXPORT_SYMBOL(flink_device_alloc);
EXPORT_SYMBOL(flink_device_init);
EXPORT_SYMBOL(flink_device_add);
EXPORT_SYMBOL(flink_device_remove);
EXPORT_SYMBOL(flink_device_delete);
EXPORT_SYMBOL(flink_get_device_by_id);
EXPORT_SYMBOL(flink_get_device_list);
EXPORT_SYMBOL(flink_subdevice_alloc);
EXPORT_SYMBOL(flink_subdevice_init);
EXPORT_SYMBOL(flink_subdevice_add);
EXPORT_SYMBOL(flink_subdevice_remove);
EXPORT_SYMBOL(flink_subdevice_delete);
EXPORT_SYMBOL(flink_get_subdevice_by_id);
EXPORT_SYMBOL(flink_select_subdevice);
EXPORT_SYMBOL(flink_get_sysfs_class);
