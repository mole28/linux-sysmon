#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Student");
MODULE_DESCRIPTION("A simple Hello World kernel module");

static int __init mymodule_init(void)
{
    printk(KERN_INFO "Hello World\n");
    return 0;
}

static void __exit mymodule_exit(void)
{
    printk(KERN_INFO "Bye Bye World\n");
}

module_init(mymodule_init);
module_exit(mymodule_exit);
