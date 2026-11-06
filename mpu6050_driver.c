#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>

static int major_device_number;

// fp represents file structure pointer in kernel, opened when open callback called, and destroyed when release function is called.
static int mpu6050_open(struct inode *inode, struct file *fp){
	pr_info("mpu6050_chardevice: Character Device Opened!\n");
	pr_info("mpu6050_chardevice: Major: %d, Minor: %d\n", imajor(inode), iminor(inode));
	pr_info("mpu6050_chardevice: File Mode: %u\n", fp->f_mode);
	pr_info("mpu6050_chardevice: File Flags: %u\n" fp->f_flags);
	return 0;
}

static int mpu6050_release(struct inode *inode, struct file *fp){
	pr_info("mpu6050_chardevice: Character Device Closed!");
	return 0;
}

static struct file_operations mpu6050_fileops = {
	.open = mpu6050_open,
	.release = mpu6050_release
};

// __init and __exit improves kernel memory usage by placing the function in .init section of ELF binary and freeing the section after initialization.
static int __init init_my_module(void){

	// Each major device number can have 256 minor device numbers. 0 for dynamic allocation.
	major_device_number = register_chrdev(0, "mpu6050_chardevice", &mpu6050_fileops);
	if(major_device_number < 0){
		pr_err("Error registering character device mpu6050_chardevice\n");
		return major_device_number;		
	}
	pr_debug("Device registration successful. Device Major number: %d\n", major_device_number);
	return 0;
}

static void __exit exit_my_module(void){
	unregister_chrdev(major_device_number, "mpu6050_chardevice");
	pr_debug("Device unregistered with major number: %d\n", major_device_number);
	pr_info("Exiting Kernel!\n");
}

module_init(init_my_module);
module_exit(exit_my_module);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Anuj Mongia <anujmongia1875@gmail.com>");
MODULE_DESCRIPTION("MPU6050 I2C Driver");
