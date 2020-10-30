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

struct pcdrv_private_data pcdrv_data = {
	.total_devices = NUMBER_OF_DEVICES,
	.pcdev_data = {
		[0] = {
			.buffer = device_buffer_dev1,
			.size = MEM_SIZE_MAX_DEV1,
			.serial_number = "PCDEV1XYZ123",
			.perm = 0x1 /* RDONLY */
		},
		[1] = {
			.buffer = device_buffer_dev2,
			.size = MEM_SIZE_MAX_DEV2,
			.serial_number = "PCDEV2XYZ123",
			.perm = 0x10 /* WRONLY */
		},
		[2] = {
			.buffer = device_buffer_dev3,
			.size = MEM_SIZE_MAX_DEV3,
			.serial_number = "PCDEV3XYZ123",
			.perm = 0x11 /* RDWR */
		},
		[3] = {
			.buffer = device_buffer_dev4,
			.size = MEM_SIZE_MAX_DEV4,
			.serial_number = "PCDEV4XYZ123",
			.perm = 0x11 /* RDWR */
		}
	}
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

#endif

/* struct to hold the file operations of the driver */
struct file_operations pcd_fops;

static int __init pcd_driver_init(void){
	int ret, i;

	/* Dynamically allocate a device number */
	ret = alloc_chrdev_region(&pcdrv_data.device_num, 0, NUMBER_OF_DEVICES, "pcd_devices");
	if(ret < 0){
		pr_warn("Device number allocation failed\n");
		goto out;
	}

	/* Create device class under /sys/class */
	pcdrv_data.class_pcd = class_create(THIS_MODULE, "pcd_class");
	if(IS_ERR(pcdrv_data.class_pcd)){
		pr_err("Class creation failed\n");
		ret = PTR_ERR(pcdrv_data.class_pcd);
		goto unreg_chrdev;
	}	
	
	for(i=0; i<NUMBER_OF_DEVICES; i++){
		pr_info("Device number <major>:<minor> = %d:%d\n", MAJOR(pcdrv_data.device_num+i), MINOR(pcdrv_data.device_num+i));
	
		/* Initialize character device with the device structure and fops structure */
		cdev_init(&pcdrv_data.pcdev_data[i].cdev, &pcd_fops);
		pcdrv_data.pcdev_data[i].cdev.owner = THIS_MODULE;

		/* Register device (cdev structure) with the VFS */
		ret = cdev_add(&pcdrv_data.pcdev_data[i].cdev, pcdrv_data.device_num+1, NUMBER_OF_DEVICES);
		if(ret < 0){
			pr_warn("Device addition failed\n");
			goto class_del;
		}

		/* Populate the sysfs with device information */
		pcdrv_data.device_pcd = device_create(pcdrv_data.class_pcd, NULL, pcdrv_data.device_num+i, NULL, "pcd-%d",i+1);
		if(IS_ERR(pcdrv_data.device_pcd)){
                	pr_err("Device creation failed\n");
                	ret = PTR_ERR(pcdrv_data.device_pcd);
                	goto cdev_del;
        	}
	}
	pr_info("Module loaded succesfully\n");
	return 0;

	
cdev_del:
	cdev_del(&pcdrv_data.pcdev_data[i].cdev);

class_del:
	if(i>0){
		for(i=i-1; i>=0; i--){
			pr_info("Destroying device %d\n", i);
			device_destroy(pcdrv_data.class_pcd, pcdrv_data.device_num+i);
			cdev_del(&pcdrv_data.pcdev_data[i].cdev);
		}
	}
	class_destroy(pcdrv_data.class_pcd);

unreg_chrdev:
	unregister_chrdev_region(pcdrv_data.device_num, NUMBER_OF_DEVICES);

out:
	pr_warn("Module insertion failed\n");
	return ret;
}

static void __exit pcd_driver_exit(void){
	int i;
	for(i=0; i<NUMBER_OF_DEVICES; i++){
		device_destroy(pcdrv_data.class_pcd, pcdrv_data.device_num+i);	
		cdev_del(&pcdrv_data.pcdev_data[i].cdev);
	}	
		class_destroy(pcdrv_data.class_pcd);
		unregister_chrdev_region(pcdrv_data.device_num, NUMBER_OF_DEVICES);
		pr_info("%s module unloaded\n", THIS_MODULE->name);
}

module_init(pcd_driver_init);
module_exit(pcd_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jakkampudi Venkata Dinesh");
MODULE_DESCRIPTION("A device driver for multiple pseudo character device");
MODULE_INFO(board, "BEAGLE BONE BLACK");
	

