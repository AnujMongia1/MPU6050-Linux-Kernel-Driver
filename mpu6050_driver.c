#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/of.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>

// MPU6050 Register Values Reference: MPU-6000 and MPU-6050 Register Map and Descriptions Revision 4.0 (https://cdn.sparkfun.com/datasheets/Sensors/Accelerometers/RM-MPU-6000A.pdf)

#define BASE_MINOR 0
#define DEVICE_NUMBER_RESERVE_COUNT 1
#define MINOR_NUMBERS_REQUIRED_COUNT 1

#define MPU6050_REGISTER_WHO_AM_I 0x75
#define MPU6050_REGISTER_PWR_MGMT_1 0x6B
#define MPU6050_REGISTER_ACCEL_XOUT_H 0x3B

#define MPU6050_SENSOR_READING_BYTES_COUNT 14 // Number of bytes for reading starting at address 0x3B covering accelerometer, gyroscope, and temperature.

#define PWR_MGMT_1_SLEEP_BIT 6

#define MPU6050_VALUE_WHO_AM_I 0x68
#define MPU6050_VALUE_WHO_AM_I_2 0x70 // alternate value used by some clone MPU6050 sensors

#define BITMASK(n) (1U << (n))
#define SET_BIT_0(x, n) ((x) & ~BITMASK(n))
#define SET_BIT_1(x, n) ((x) | BITMASK(n))
#define IS_BIT_0(x, n) (((x) & BITMASK(n)) == 0)

// i2c_device_id used for matching devices with legacy i2c instantiation method (non device tree/ manual client creation using sysfs)
static const struct i2c_device_id mpu6050_i2c_idtable[] = {
	{ "mpu6050-unique", 0 },
	{ }
};

// MODULE_DEVICE_TABLE() macro exports the ID table information to module's metadata alias entries, which is then used for automatically loading module from disk, when a hardware device with the similar alias id appears. 
MODULE_DEVICE_TABLE(i2c, mpu6050_i2c_idtable);

// of_device_id used for matching devices with modern device tree supported instantiation method
static const struct of_device_id mpu6050_of_idtable[] = {
	{ .compatible = "invensense,mpu6050-unique" },
	{ }
};

MODULE_DEVICE_TABLE(of, mpu6050_of_idtable); 

// struct to manage device context
struct mpu6050 {
	struct i2c_client* client;
	dev_t device_number;
	struct cdev mpu6050_cdev;
	struct class* mpu6050_class;
	struct device* mpu6050_device;
};

// fp represents file structure pointer in kernel, opened when open callback called, and destroyed when release function is called.
static int mpu6050_open(struct inode *inode, struct file *fp){
	pr_info("[MPU6050 I2C DRIVER]: Character Device Opened!\n");
	pr_info("[MPU6050 I2C DRIVER]: Major: %d, Minor: %d\n", imajor(inode), iminor(inode));
	pr_info("[MPU6050 I2C DRIVER]: File Mode: %u\n", fp->f_mode);
	pr_info("[MPU6050 I2C DRIVER]: File Flags: %u\n", fp->f_flags);

	// container_of macro retrieves the pointer to a parent structure when given pointer to one of its internal member.
	// mpu6050_read required client to read data from sensor which is the handle inside the mpu6050_context struct that is dynamically allocated inside probe function, the pointer to which is lost outside the scope of probe function, this container_of helps get the pointer to that struct back, store it in file->private_data which is accessible by the read function.
	struct mpu6050* mpu6050_context = container_of(inode->i_cdev, struct mpu6050, mpu6050_cdev);
	fp->private_data = mpu6050_context;
	return 0;
}

static int mpu6050_release(struct inode *inode, struct file *fp){
	pr_info("[MPU6050 I2C DRIVER]: Character Device Closed!\n");
	return 0;
}

static ssize_t mpu6050_read(struct file *fp, char __user *user_buf, size_t length, loff_t *offset){

	// The full sensor reading is seven values Accelermeter (X, Y, Z), Temperature, Gyroscope(X, Y, Z), where each value is 2 bytes, stored in 14 continious bytes from 0x3B to 0x48.
	// Any reading requested that is not of length 14 bytes treated as invalid.
	if(length != MPU6050_SENSOR_READING_BYTES_COUNT){
		return -EINVAL;
	}
	struct mpu6050* mpu6050_context = fp->private_data;
	u8 data[MPU6050_SENSOR_READING_BYTES_COUNT];
	int read_return_value = i2c_smbus_read_i2c_block_data(mpu6050_context->client, MPU6050_REGISTER_ACCEL_XOUT_H, MPU6050_SENSOR_READING_BYTES_COUNT, data);
	if (read_return_value < 0){
		pr_err("[MPU6050 I2C DRIVER]: Error reading data from MPU6050.\n");
		return -EIO;
	}

	if (read_return_value != 14){
		pr_err("[MPU6050 I2C DRIVER]: Could not read 14 bytes of sensor reading start at 0x3B.\n");
		return -EIO;
	}

	if(copy_to_user(user_buf, data, MPU6050_SENSOR_READING_BYTES_COUNT)){
		return -EFAULT;
	}

	return MPU6050_SENSOR_READING_BYTES_COUNT; // read function will always return 14 bytes of sensor reading
}

static struct file_operations mpu6050_fileops = {
	.owner = THIS_MODULE, // this prevents kernel from unloading the driver module while a userspace application has the device file open
	.open = mpu6050_open,
	.release = mpu6050_release,
	.read = mpu6050_read
};

static int mpu6050_probe(struct i2c_client *client){
	pr_info("[MPU6050 I2C DRIVER]: Probe called!\n");

	struct mpu6050* mpu6050_context;
	s32 register_return_value;

/* Dynamic memory allocation for tracking device context in custom struct, and binding it to i2c_client structure  */

	// Dyamically allocating a struct for each device state. devm_kzalloc provides a memory managed zero allocated memory in kernel. The availablity of the allocated memory is binded to the device. When the device disappears from the system the memory if freed automatically.
	mpu6050_context = devm_kzalloc(&client->dev, sizeof(struct mpu6050), GFP_KERNEL);
	if (!mpu6050_context){
		pr_err("[MPU6050 I2C DRIVER]: Error allocating data for mpu_private_data struct\n");
		return -ENOMEM; // error code for failed dynamic memory allocation
	}

	// Preserving i2c device handle
	mpu6050_context->client = client;

	// Linking custom device context struct (struct mpu6050) to i2c_client structure
	i2c_set_clientdata(client, mpu6050_context);

/* Confirming connectivity and communication to MPU6050 */

	// First step to check WHO_AM_I register on the MPU6050 sensor to check if properly wired and responding before character device registration process
	register_return_value = i2c_smbus_read_byte_data(mpu6050_context->client, MPU6050_REGISTER_WHO_AM_I);
	if (register_return_value < 0){
		pr_err("[MPU6050 I2C DRIVER]: WHO_AM_I (0x75) register value on MPU6050 could not be read successfully. Please check device connection.\n");
		return register_return_value;
	}

	if(!(register_return_value == MPU6050_VALUE_WHO_AM_I || register_return_value == MPU6050_VALUE_WHO_AM_I_2)){
		pr_err("[MPU6050 I2C DRIVER]: Device did not respond with a known WHO_AM_I register value (0x68, 0x70). Unknown Device.\n");
		return -ENODEV; // error code for problem with the device found
	}

	pr_info("[MPU6050 I2C DRIVER]: Device found successfully. Returned WHO_AM_I register (0x75) value: 0x%02X\n", register_return_value);

/* Waking up the MPU6050 chip before reading data */

	// Reading the PWR_MGMT_1 register (0x6B) containing the sleep bit required to wakeup board before reading data
	s32 pwr_mgmt_1_ret = i2c_smbus_read_byte_data(mpu6050_context->client, MPU6050_REGISTER_PWR_MGMT_1);

	if (pwr_mgmt_1_ret < 0){
		pr_err("[MPU6050 I2C DRIVER]: PWR_MGMT_1 (0x6B) register value on MPU6050 could not be read successfully.\n");
		return pwr_mgmt_1_ret;
	}

	// Setting the SLEEP bit (6) in PWR_MGMT_1 register to 0 to wake up the chip
	u8 pwr_mgmt_value_current = pwr_mgmt_1_ret;
	u8 pwr_mgmt_value_sleep_bit_off = SET_BIT_0(pwr_mgmt_value_current, PWR_MGMT_1_SLEEP_BIT);
	pwr_mgmt_1_ret = i2c_smbus_write_byte_data(mpu6050_context->client, MPU6050_REGISTER_PWR_MGMT_1, pwr_mgmt_value_sleep_bit_off);

	if(pwr_mgmt_1_ret < 0){
		pr_err("[MPU6050 I2C DRIVER]: Error setting the SLEEP bit (6) to 0 on PWR_MGMT_1 (0x6B) register on MPU6050.\n");
		return pwr_mgmt_1_ret;
	}

	// Confirming SLEEP bit(6) was written sucessfully to 0
	pwr_mgmt_1_ret = i2c_smbus_read_byte_data(mpu6050_context->client, MPU6050_REGISTER_PWR_MGMT_1);

	if(pwr_mgmt_1_ret < 0){
		pr_err("[MPU6050 I2C DRIVER]: Error reading back the SLEEP bit (6) to 0 for confirmation after write on PWR_MGMT_1 (0x6B) register on MPU6050.\n");
		return pwr_mgmt_1_ret;
	}

	if(!IS_BIT_0(pwr_mgmt_1_ret, PWR_MGMT_1_SLEEP_BIT)){
		pr_err("[MPU6050 I2C DRIVER]: SLEEP bit (6) still set to 1 on PWR_MGMT_1 (0x6B) register on MPU6050 after attempt to write to 0.\n");
		return -EIO;
	}

	pr_info("[MPU6050 I2C DRIVER]: SLEEP bit written to 0 sucessfully. The sensor is ready to read values. PWR_MGMT_1 Value Before: 0x%02X, PWR_MGMT_1 Value After: 0x%02X\n", pwr_mgmt_value_current, pwr_mgmt_value_sleep_bit_off);

/* Character device registration process */

	int status;

#ifdef STATIC_DEVICE_NUMBER
	mpu6050_context->device_number = STATIC_DEVICE_NUMBER;
	// reserve a device number with static allocation
	status = register_chrdev_region(mpu6050_context->device_number, DEVICE_NUMBER_RESERVE_COUNT, "mpu6050");
#else
	// reserve a device number with dynamic number allocation
	status = alloc_chrdev_region(&mpu6050_context->device_number, BASE_MINOR, MINOR_NUMBERS_REQUIRED_COUNT, "mpu6050");
#endif
	if(status < 0){
		pr_err("[MPU6050 I2C DRIVER]: Error reserving device numbers.\n");
		return status;
	}
	pr_info("[MPU6050 I2C DRIVER]: Character device numbers reservation successful. Device Major Number: %d, Device Minor Number: %d\n", MAJOR(mpu6050_context->device_number), MINOR(mpu6050_context->device_number));
	
	// connect character device to file operations struct
	cdev_init(&mpu6050_context->mpu6050_cdev, &mpu6050_fileops);
	mpu6050_context->mpu6050_cdev.owner = THIS_MODULE;

	// adding the character device to system to make it live
	status = cdev_add(&mpu6050_context->mpu6050_cdev, mpu6050_context->device_number, MINOR_NUMBERS_REQUIRED_COUNT);
	if(status < 0){
		pr_err("[MPU6050 I2C DRIVER]: Error adding character device. Device Major Number: %d, Device Minor Number: %d\n", MAJOR(mpu6050_context->device_number), MINOR(mpu6050_context->device_number));
		// goto used for error handling to release resources. goto labels are stacked in the reverse order of resource acquisistion. If at any point we fail to acquire any any resources we must release all other resources acquired before that.
		goto free_device_number;
	}
	pr_info("[MPU6050 I2C DRIVER]: Character device added sucessfully. Device Major Number: %d, Device Minor Number: %d\n", MAJOR(mpu6050_context->device_number), MINOR(mpu6050_context->device_number));

	// creating device class which appears under /sys/class/
	mpu6050_context->mpu6050_class = class_create("mpu6050_class");
	if(IS_ERR(mpu6050_context->mpu6050_class)){
		status = PTR_ERR(mpu6050_context->mpu6050_class);
		pr_err("[MPU6050 I2C DRIVER]: Error adding device class under /sys/class\n");
		goto delete_cdev;
	}

	pr_info("[MPU6050 I2C DRIVER]: Device class created successfuly under /sys/class/mpu6050_class\n");

	// creating the device file using the device class and the device number allocated above
	mpu6050_context->mpu6050_device = device_create(mpu6050_context->mpu6050_class, NULL, mpu6050_context->device_number, NULL, "mpu6050");
	if(IS_ERR(mpu6050_context->mpu6050_device)){
		status = PTR_ERR(mpu6050_context->mpu6050_device);
		pr_err("[MPU6050 I2C DRIVER]: Error adding device file under /dev\n");
		goto delete_device_class;
	}

	pr_info("[MPU6050 I2C DRIVER]: Device file created successfully under /dev/mpu6050\n");

	return 0;

delete_device_class:
	class_destroy(mpu6050_context->mpu6050_class);
delete_cdev:
	cdev_del(&mpu6050_context->mpu6050_cdev);
free_device_number:
	unregister_chrdev_region(mpu6050_context->device_number, DEVICE_NUMBER_RESERVE_COUNT);
	return status;
}

static void mpu6050_remove(struct i2c_client *client){
	pr_info("[MPU6050 I2C DRIVER]: Remove called!\n");
	struct mpu6050* mpu6050_context = i2c_get_clientdata(client);
	device_destroy(mpu6050_context->mpu6050_class, mpu6050_context->device_number);
	class_destroy(mpu6050_context->mpu6050_class);
	cdev_del(&mpu6050_context->mpu6050_cdev);
	unregister_chrdev_region(mpu6050_context->device_number, DEVICE_NUMBER_RESERVE_COUNT);

	// Putting chip back to sleep
	s32 pwr_mgmt_1_ret = i2c_smbus_read_byte_data(mpu6050_context->client, MPU6050_REGISTER_PWR_MGMT_1);

	if (pwr_mgmt_1_ret < 0){
		pr_err("[MPU6050 I2C DRIVER]: PWR_MGMT_1 (0x6B) register value on MPU6050 could not be read successfully.\n");
		return;
	}

	// Setting the SLEEP bit (6) in PWR_MGMT_1 register to 1 to put the chip to sleep
	u8 pwr_mgmt_value_current = pwr_mgmt_1_ret;
	u8 pwr_mgmt_value_sleep_bit_on = SET_BIT_1(pwr_mgmt_value_current, PWR_MGMT_1_SLEEP_BIT);
	pwr_mgmt_1_ret = i2c_smbus_write_byte_data(mpu6050_context->client, MPU6050_REGISTER_PWR_MGMT_1, pwr_mgmt_value_sleep_bit_on);

	if(pwr_mgmt_1_ret < 0){
		pr_err("[MPU6050 I2C DRIVER]: Error setting the SLEEP bit (6) back to 1 on PWR_MGMT_1 (0x6B) register on MPU6050.\n");
		return;
	}
	pr_info("[MPU6050 I2C DRIVER]: MPU6050 PWR_MGMT_1 SLEEP bit (6) set back to 1 to put board to sleep.\n");
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
