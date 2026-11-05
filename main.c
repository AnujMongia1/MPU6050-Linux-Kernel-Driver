#include <linux/module.h>
#include <linux/init.h>

int init_module(){
	printk("Hello Kernel!");
	return 0;
}

int exit_module(){
	print("Exiting Kernel!");
	return 0;
}

module_init(init_module);
module_exit(exit_module);

MODULE_LICENCE("GPL");

