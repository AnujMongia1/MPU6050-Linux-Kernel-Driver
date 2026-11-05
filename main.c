#include <linux/module.h>
#include <linux/init.h>

static int init_my_module(void){
	printk("Hello Kernel!\n");
	return 0;
}

static void exit_my_module(void){
	printk("Exiting Kernel!\n");
}

module_init(init_my_module);
module_exit(exit_my_module);

MODULE_LICENSE("GPL");
