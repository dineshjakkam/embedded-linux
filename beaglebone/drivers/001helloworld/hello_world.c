#include<linux/module.h>

static int __init hello_world_init(void){
	pr_info("Hello world!\n");
	return 0;
}

static void __exit hello_world_exit(void){
	pr_info("Good Bye world!\n");
}

module_init(hello_world_init);
module_exit(hello_world_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jakkampudi Venkata Dinesh");
MODULE_DESCRIPTION("A simple hello world Kernel Module");
MODULE_INFO(board, "BEAGLE BONE BLACK");
MODULE_INFO(Testing, "Tetsing this info");
