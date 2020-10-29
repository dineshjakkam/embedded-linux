#include<linux/module.h>
#include<linux/cdev.h>
#include<linux/fs.h>
#include<linux/device.h>
#include<linux/kdev_t.h>
#include<linux/err.h>
#include<linux/uaccess.h>

#undef pr_fmt
#define pr_fmt(fmt) "%s:" fmt, __func__

#define NUMBER_OF_DEVICES 4
#define MEM_SIZE_MAX_DEV1 1024
#define MEM_SIZE_MAX_DEV2 1024
#define MEM_SIZE_MAX_DEV3 1024
#define MEM_SIZE_MAX_DEV4 1024

/* psuedo devices memory */
char device_buffer_dev1[MEM_SIZE_MAX_DEV1];
char device_buffer_dev2[MEM_SIZE_MAX_DEV2];
char device_buffer_dev3[MEM_SIZE_MAX_DEV3];
char device_buffer_dev4[MEM_SIZE_MAX_DEV4];

/* Device private data structure */
struct pcdev_private_data{
	char* buffer;
	unsigned int size;
	const char* serial_number;
	unsigned short int perm;
	struct cdev cdev;
};

/* Driver private data structure */
struct pcdrv_private_data{
	int total_devices;
	dev_t device_num;
	struct class* class_pcd;
	struct device* device_pcd;
	struct pcdev_private_data pcdev_data[NUMBER_OF_DEVICES];
};

#if 0
loff_t pcd_lseek(struct file *filep, loff_t off, int whence){
	loff_t tmp;
	pr_info("lseek requested\n");
	pr_info("Current file position = %lld\n", filep->f_pos);
	
	switch(whence){
		case SEEK_SET:
			if( (off > DEVICE_MEM_SIZE) || (off < 0) )
				return -EINVAL;
			filep->f_pos = off;
			break;
		case SEEK_CUR:
			tmp = filep->f_pos + off;
			if( (tmp > DEVICE_MEM_SIZE) || (tmp<0))
				return -EINVAL;
			filep->f_pos = tmp;
			break;
		case SEEK_END:
		deafult:
			return -EINVAL;
	};
			
	pr_info("Current file position after lseek = %lld\n", filep->f_pos);
	return 0;
}
        
ssize_t pcd_read(struct file *filep, char __user *buffer, size_t count, loff_t *f_pos){
	pr_info("Read requested for %zu bytes\n", count);
	pr_info("Initial file position = %lld\n", *f_pos);
	
	/* Adjust the count */
	if((*f_pos + count) > DEVICE_MEM_SIZE)
		count = DEVICE_MEM_SIZE - *f_pos;

	/* Copy to user */
	if(copy_to_user(buffer, &device_buffer[*f_pos], count)){
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
	pr_info("Write requested for %zu bytes\n", count);
	pr_info("Initial file position = %lld\n", *f_pos); 

	if((*f_pos + count) > DEVICE_MEM_SIZE || (count < 0)){
		count = DEVICE_MEM_SIZE - *f_pos;
	}

	if(!count){
		pr_warning("No space remaining on device to write new bytes\n");
		return -ENOMEM;
	}
	
	if(copy_from_user(&device_buffer[*f_pos], buffer, count)){
		return -EFAULT;
	}
	*f_pos += count;

	pr_info("Number of bytes successfully written = %zu\n", count);
	pr_info("Updated file position = %lld\n", *f_pos);

	return count;
}

int pcd_open(struct inode *p_inode, struct file *filep){
	pr_info("Open was successful\n");
	return 0;
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
	.llseek = pcd_lseek,
	.release = pcd_release,
	.owner = THIS_MODULE
};
#endif

static int __init pcd_driver_init(void){
	int ret;
#if 0
	/* Dynamically allocate a device number */
	ret = alloc_chrdev_region(&device_num, 0, 1, "pcd_devices");
	if(ret < 0){
		pr_warn("Device number allocation failed\n");
		goto out;
	}

	pr_info("Device number <major>:<minor> = %d:%d\n", MAJOR(device_num), MINOR(device_num));
	
	/* Initialize character device with the device structure and fops structure */
	cdev_init(&pcd_dev, &pcd_fops);
	pcd_dev.owner = THIS_MODULE;

	/* Register device (cdev structure) with the VFS */
	ret = cdev_add(&pcd_dev, device_num, 1);
	if(ret < 0){
		pr_warn("Device addition failed\n");
		goto unreg_chrdev;
	}

	/* Create device class under /sys/class */
	class_pcd = class_create(THIS_MODULE, "pcd_class");
	if(IS_ERR(class_pcd)){
		pr_err("Class creation failed\n");
		ret = PTR_ERR(class_pcd);
		goto cdev_del;
	}	

	/* Populate the sysfs with device information */
	device_pcd = device_create(class_pcd, NULL, device_num, NULL, "pcd");
	if(IS_ERR(device_pcd)){
                pr_err("Device creation failed\n");
                ret = PTR_ERR(device_pcd);
                goto class_del;
        }

	pr_info("Module loaded succesfully\n");
	return 0;

	
class_del:
	class_destroy(class_pcd);

cdev_del:
	cdev_del(&pcd_dev);

unreg_chrdev:
	unregister_chrdev_region(device_num, 1);

out:
	pr_warn("Module insertion failed\n");
	return ret;
#endif
 return 0;
}

static void __exit pcd_driver_exit(void){
#if 0
	device_destroy(class_pcd, device_num);
	class_destroy(class_pcd);
	cdev_del(&pcd_dev);
	unregister_chrdev_region(device_num, 1);
	pr_info("%s module unloaded\n", THIS_MODULE->name);
#endif
}

module_init(pcd_driver_init);
module_exit(pcd_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jakkampudi Venkata Dinesh");
MODULE_DESCRIPTION("A device driver for multiple pseudo character device");
MODULE_INFO(board, "BEAGLE BONE BLACK");
	

