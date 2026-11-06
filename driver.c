#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>

static int major_device_number;
static struct file_operations fileops = {};

// __init and __exit improves kernel memory usage by placing the function in .init section of ELF binary and freeing the section after initialization.
static int __init init_my_module(void){

	// Each major device number can have 256 minor device numbers. 0 for dynamic allocation.
	major_device_number = register_chrdev(0, "test_chardevice", &fileops);
	if(major_device_number < 0){
		printk(KERN_ERR "Error registering character device (test_chardevice)\n");
		return major_device_number;		
	}
	printk(KERN_DEBUG "Device registration successful. Device Major number: %d\n", major_device_number);
	return 0;
}

static void __exit exit_my_module(void){
	printk(KERN_INFO "Exiting Kernel!\n");
}

module_init(init_my_module);
module_exit(exit_my_module);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Anuj Mongia <anujmongia1875@gmail.com>");
MODULE_DESCRIPTION("MPU6050 I2C Driver");
