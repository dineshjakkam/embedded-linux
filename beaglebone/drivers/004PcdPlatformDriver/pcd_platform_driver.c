#include<linux/module.h>
#include<linux/cdev.h>
#include<linux/fs.h>
#include<linux/device.h>
#include<linux/kdev_t.h>
#include<linux/platform_device.h>
#include<linux/err.h>
#include<linux/slab.h>
#include<linux/uaccess.h>
#include "platform.h"

#undef pr_fmt
#define pr_fmt(fmt) "%s:" fmt, __func__

#define MAX_DEVICES 4

/* Device private data structure */
struct pcdev_private_data{
	struct pcdev_platform_data pdata;
	char* buffer;
	dev_t dev_num;
	struct cdev cdev;
};

/* Driver private data structure */
struct pcdrv_private_data{
	int total_devices;
	dev_t device_num_base;
	struct class* class_pcd;
	struct device* device_pcd;
};

struct pcdrv_private_data pcdrv_data;

loff_t pcd_lseek(struct file *filep, loff_t off, int whence){
	struct pcdev_private_data* pcdev_data = (struct pcdev_private_data *)filep->private_data;
	
	int max_data = pcdev_data->pdata.size;
	off_t tmp;
	pr_info("lseek requested\n");
	pr_info("Current file position = %lld\n", filep->f_pos);
	
	switch(whence){
		case SEEK_SET:
			if( (off > max_data) || (off < 0) )
				return -EINVAL;
			filep->f_pos = off;
			break;
		case SEEK_CUR:
			tmp = filep->f_pos + off;
			if( (tmp > max_data) || (tmp<0))
				return -EINVAL;
			filep->f_pos = tmp;
			break;
		case SEEK_END:
		default:
			return -EINVAL;
	};
			
	pr_info("Current file position after lseek = %lld\n", filep->f_pos);
	return 0;
}

ssize_t pcd_read(struct file *filep, char __user *buffer, size_t count, loff_t *f_pos){
	struct pcdev_private_data* pcdev_data = (struct pcdev_private_data *)filep->private_data;
	
	int max_data = pcdev_data->pdata.size;
	pr_info("Read requested for %zu bytes\n", count);
	pr_info("Initial file position = %lld\n", *f_pos);
	
	/* Adjust the count */
	if((*f_pos + count) > max_data)
		count = max_data - *f_pos;

	/* Copy to user */
	if(copy_to_user(buffer, &pcdev_data->buffer[*f_pos], count)){
		return -EFAULT;
	}
	
	/* Uodate the current file position */
	*f_pos += count;
	
	pr_info("Number of bytes successfully read is %zu\n", count);
	pr_info("Updated file position = %lld\n", *f_pos);
	
	/* Return numbe rof bytes successfully read */
	return count;
}

ssize_t pcd_write(struct file *filep, const char __user *buffer, size_t count, loff_t *f_pos){
	struct pcdev_private_data* pcdev_data = (struct pcdev_private_data *)filep->private_data;
	
	int max_data = pcdev_data->pdata.size;
	
	pr_info("Write requested for %zu bytes\n", count);
	pr_info("Initial file position = %lld\n", *f_pos); 

	if((*f_pos + count) > max_data || (count < 0)){
		count = max_data - *f_pos;
	}

	if(!count){
		pr_warning("No space remaining on device to write new bytes\n");
		return -ENOMEM;
	}
	
	if(copy_from_user(&pcdev_data->buffer[*f_pos], buffer, count)){
		return -EFAULT;
	}
	*f_pos += count;

	pr_info("Number of bytes successfully written = %zu\n", count);
	pr_info("Updated file position = %lld\n", *f_pos);

	return count;
}

int check_permission(int perm, int mode){
	if(perm == RDWR){
		return 0;
	}
	else if(perm == RDONLY){
		if((mode & FMODE_READ) && !(mode & FMODE_WRITE)){
			return 0;
		}
	}
	else if(perm == WRONLY){
		if(!(mode & FMODE_READ) && (mode & FMODE_WRITE)){
			return 0;
		}
	}
	return -EPERM;
}

int pcd_open(struct inode *p_inode, struct file *filep){
	int ret, minor_n;
	struct pcdev_private_data* pcdev_data;

	/* Find out which device file open was attempted by the uer space */
	minor_n = MINOR(p_inode->i_rdev);
	pr_info("Minor access = %d\n", minor_n);
	
	/* Get device's private data structure */
	pcdev_data = container_of(p_inode->i_cdev, struct pcdev_private_data, cdev);

	/* To supply device private data to other methods of the driver */
	filep->private_data = pcdev_data;

	pr_info("permission is %x\n", pcdev_data->pdata.perm);
	/* check permissions */
	ret = check_permission(pcdev_data->pdata.perm, filep->f_mode);

	(!ret) ? pr_info("Open was successful\n") : pr_info("Open was unsuccessfull\n");
	
	return ret;
}

int pcd_release(struct inode *p_inode, struct file *filep){
	pr_info("Close was successful\n");
	return 0;
}

/* struct to hold the file operations of the driver */
struct file_operations pcd_fops = {
	.open = pcd_open,
	.write = pcd_write,
	.read = pcd_read,
	.release = pcd_release,
	.llseek = pcd_lseek,
};

/* Gets called when the device is removed from the platform */
int pcd_platform_driver_remove(struct platform_device* pdev){
        struct pcdev_private_data* dev_data = dev_get_drvdata(&pdev->dev);
	
	/* Remove device that was created with device_create() */
	device_destroy(pcdrv_data.class_pcd, dev_data->dev_num);

	/* Remove cdev entry from the system */
	cdev_del(&dev_data->cdev);
	
	pcdrv_data.total_devices--;
	
	pr_info("A device is removed\n");
	return 0;
}

/* Gets called when the matched platform device is found */
int pcd_platform_driver_probe(struct platform_device* pdev){
	
	int ret;
	struct pcdev_private_data *dev_data;
	struct pcdev_platform_data *pdata;
	
	pr_info("A device is detected\n");
	
	/* 1. Get the platform data */
	pdata = (struct pcdev_platform_data*)dev_get_platdata(&pdev->dev);
	if(!pdata){
		pr_info("No platform data available\n");
		ret = -EINVAL;
		goto out;
	}
	
	/* Dynamically allocate memory for the device private data */
	dev_data = devm_kzalloc(&pdev->dev, sizeof(*dev_data), GFP_KERNEL);
	if(dev_data == NULL){
		pr_info("Cannot allocate memory for device data structure\n");
		ret = -ENOMEM;
		goto out;
	}
	
	dev_set_drvdata(&pdev->dev, dev_data);	

	dev_data->pdata.size = pdata->size;
	dev_data->pdata.perm = pdata->perm;
	dev_data->pdata.serial_number = pdata->serial_number;

	pr_info("Device serial number = %s\n", dev_data->pdata.serial_number);
	pr_info("Device size = %d\n", dev_data->pdata.size);
	pr_info("Device permission = %d\n", dev_data->pdata.perm);

	/* Dynamically allocate memory for the device buffer using size
	information from the platform data */
	dev_data->buffer = devm_kzalloc(&pdev->dev, sizeof(dev_data->pdata.size), GFP_KERNEL);
        if(dev_data->buffer == NULL){
                pr_info("Cannot allocate memory for device buffer\n");
                ret = -ENOMEM;
                goto dev_data_free;
        }

	/* Get the device number */
	dev_data->dev_num = pcdrv_data.device_num_base + pdev->id;

	/* Do cdev init and cdev add */
	cdev_init(&dev_data->cdev, &pcd_fops);

	dev_data->cdev.owner = THIS_MODULE;
	ret = cdev_add(&dev_data->cdev, dev_data->dev_num, 1);
	if(ret < 0 ){
		pr_err("Cdev add failed\n");
		goto buffer_free;
	}

	/* Create device file for the detected platform device */
	pcdrv_data.device_pcd = device_create(pcdrv_data.class_pcd, NULL, dev_data->dev_num, NULL, "pcdev-%d", pdev->id);
	if(IS_ERR(pcdrv_data.device_pcd)){
		pr_err("Device create failed \n");
		ret = PTR_ERR(pcdrv_data.device_pcd);
		goto cdev_del;
	}

	pcdrv_data.total_devices++;
	
	pr_info("The probe is successful\n");
	return 0;

cdev_del:
	cdev_del(&dev_data->cdev);;
buffer_free:
	devm_kfree(&pdev->dev, dev_data->buffer);
dev_data_free:
	devm_kfree(&pdev->dev, dev_data);
out:
	pr_info("Device probe failed\n");
	return ret;
}

struct platform_driver pcd_platform_driver = {
	.probe = pcd_platform_driver_probe,
	.remove = pcd_platform_driver_remove,
	.driver = {
		.name = "pseudo-char-device"
	}
};

static int __init pcd_platform_driver_init(void){
	
	int ret;

	/* 1. Dynamically allocate a device number for MAX DEVICES */
	ret = alloc_chrdev_region(&pcdrv_data.device_num_base, 0, MAX_DEVICES, "pcd_devices");
	if(ret < 0){
		pr_warn("Alloc chrdev  failed\n");
		return ret;
	}
	
	/* Create device class under /sys/class */
	pcdrv_data.class_pcd = class_create(THIS_MODULE, "pcd_class");
	if(IS_ERR(pcdrv_data.class_pcd)){
		pr_err("Class creation failed\n");
		ret = PTR_ERR(pcdrv_data.class_pcd);
		unregister_chrdev_region(pcdrv_data.device_num_base, MAX_DEVICES);
	}	
	
	/* Register a platform driver */
	platform_driver_register(&pcd_platform_driver);
	pr_info("PCD platform driver loaded\n");

	return 0;
}


static void __exit pcd_platform_driver_exit(void){
	platform_driver_unregister(&pcd_platform_driver);
        class_destroy(pcdrv_data.class_pcd);
	unregister_chrdev_region(pcdrv_data.device_num_base, MAX_DEVICES);

	pr_info("PCD platform driver unloaded\n");
	
}

module_init(pcd_platform_driver_init);
module_exit(pcd_platform_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jakkampudi Venkata Dinesh");
MODULE_DESCRIPTION("A device driver for multiple pseudo character device");
MODULE_INFO(board, "BEAGLE BONE BLACK");
