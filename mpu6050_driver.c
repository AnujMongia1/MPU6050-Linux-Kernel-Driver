#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/of.h>
#include <linux/cdev.h>

#define BASE_MINOR 0
#define DEVICE_NUMBER_RESERVE_COUNT 1
#define MINOR_NUMBERS_REQUIRED_COUNT 1

static dev_t device_number;
static struct cdev mpu6050_cdev;

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
	.owner = THIS_MODULE, // this prevents kernel from unloading the driver module while a userspace application has the device file open
	.open = mpu6050_open,
	.release = mpu6050_release
};

static int mpu6050_probe(struct i2c_client *client){
	pr_info("[MPU6050 I2C DRIVER]: Probe called!\n");
	int status;

/* Character device registration process */
#ifdef STATIC_DEVICE_NUMBER
	device_number = STATIC_DEVICE_NUMBER;
	//reserve a device number with static allocation
	status = register_chrdev_region(device_number, DEVICE_NUMBER_RESERVE_COUNT, "mpu6050");
#else
	// reserve a device number with dynamic number allocation
	status = alloc_chrdev_region(&device_number, BASE_MINOR, MINOR_NUMBERS_REQUIRED_COUNT, "mpu6050");
#endif
	if(status < 0){
		pr_err("[MPU6050 I2C DRIVER]: Error reserving device numbers.\n");
		return status;
	}
	pr_info("[MPU6050 I2C DRIVER]: Character device numbers reservation successful. Device Major Number: %d, Device Minor Number: %d\n", MAJOR(device_number), MINOR(device_number));
	return 0;
	
	// connect character device to file operations struct
	cdev_init(&mpu6050_cdev, &mpu6050_fileops);
	mpu6050_cdev.owner = THIS_MODULE;

	// adding the character device to system to make it live
	status = cdev_add(&mpu6050_cdev, device_number, MINOR_NUMBERS_REQUIRED_COUNT);
	if(status < 0){
		pr_err("[MPU6050 I2C DRIVER]: Error adding character device. Device Major Number: %d, Device Minor Number: %d\n", MAJOR(device_number), MINOR(device_number));
		// goto used for error handling to release resources. goto labels are stacked in the reverse order of resource acquisistion.
		goto free_device_number;
	}
	pr_info("[MPU6050 I2C DRIVER]: Character device added sucessfully. Device Major Number: %d, Device Minor Number: %d\n", MAJOR(device_number), MINOR(device_number));

free_device_number:
	unregister_chrdev_region(device_number, DEVICE_NUMBER_RESERVE_COUNT);
	return status;
}

static void mpu6050_remove(struct i2c_client *client){
	pr_info("[MPU6050 I2C DRIVER]: Remove called!\n");
	cdev_del(&mpu6050_cdev);
	unregister_chrdev_region(device_number, DEVICE_NUMBER_RESERVE_COUNT);
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

// __init and __exit improves kernel memory usage by placing the function in .init section of ELF binary and freeing the section after initialization.
static int __init init_my_module(void){
	return i2c_add_driver(&mpu6050_driver);
}

static void __exit exit_my_module(void){
	i2c_del_driver(&mpu6050_driver);
	pr_info("Exiting Kernel!\n");
}

module_init(init_my_module);
module_exit(exit_my_module);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Anuj Mongia <anujmongia1875@gmail.com>");
MODULE_DESCRIPTION("MPU6050 I2C Driver");
