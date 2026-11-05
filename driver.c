#include <linux/module.h>
#include <linux/init.h>

// __init and __exit improves kernel memory usage by placing the function in .init section of ELF binary and freeing the section after initialization.
static int __init init_my_module(void){
	printk("Hello Kernel!\n");
	return 0;
}

static void __exit exit_my_module(void){
	printk("Exiting Kernel!\n");
}

module_init(init_my_module);
module_exit(exit_my_module);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Anuj Mongia <anujmongia1875@gmail.com>");
MODULE_DESCRIPTION("MPU6050 I2C Driver");
