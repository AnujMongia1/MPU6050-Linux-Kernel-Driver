#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/of.h>

static int major_device_number;

// i2c_device_id used for matching devices with legacy i2c instantiation method (non device tree/ manual client creation using sysfs)
static const struct i2c_device_id mpu6050_i2c_idtable[] = {
	{ "mpu6050", 0 },
	{ }
};

// MODULE_DEVICE_TABLE() macro exports the ID table information to module's metadata alias entries, which is then used for automatically loading module from disk, when a hardware device with the similar alias id appears. 
MODULE_DEVICE_TABLE(i2c, mpu6050_i2c_idtable);

// of_device_id used for matching devices with modern device tree supported instantiation method
static const struct of_device_id mpu6050_of_idtable[] = {
	{ .compatible = "invensense,mpu6050" },
	{ }
};

MODULE_DEVICE_TABLE(of, mpu6050_of_idtable); 

static int mpu6050_probe(struct i2c_client *client){
	pr_info("MPU6050 I2C DRIVER: Probe called!\n");
	return 0;
}

static void mpu6050_remove(struct i2c_client *client){
	pr_info("MPU6050 I2C DRIVER: Remove called!\n");
}

// Grouping the compatible device ids, probe and remove into a i2c_driver struct
static struct i2c_driver mpu6050_driver = {
	.driver = {
		.name = "mpu6050_driver",
		.of_match_table = mpu6050_of_idtable,
	},
	.probe = mpu6050_probe,
	.remove = mpu6050_remove,
	.id_table = mpu6050_i2c_idtable
};

// fp represents file structure pointer in kernel, opened when open callback called, and destroyed when release function is called.
static int mpu6050_open(struct inode *inode, struct file *fp){
	pr_info("mpu6050_chardevice: Character Device Opened!\n");
	pr_info("mpu6050_chardevice: Major: %d, Minor: %d\n", imajor(inode), iminor(inode));
	pr_info("mpu6050_chardevice: File Mode: %u\n", fp->f_mode);
	pr_info("mpu6050_chardevice: File Flags: %u\n", fp->f_flags);
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
	return i2c_add_driver(&mpu6050_driver);
}

static void __exit exit_my_module(void){
	unregister_chrdev(major_device_number, "mpu6050_chardevice");
	i2c_del_driver(&mpu6050_driver);
	pr_debug("Device unregistered with major number: %d\n", major_device_number);
	pr_info("Exiting Kernel!\n");
}

module_init(init_my_module);
module_exit(exit_my_module);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Anuj Mongia <anujmongia1875@gmail.com>");
MODULE_DESCRIPTION("MPU6050 I2C Driver");
