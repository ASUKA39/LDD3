#include <linux/init.h>
#include <linux/module.h>
MODULE_LICENSE("Dual BSD/GPL");

static char *saying = "Hello";
module_param(saying, charp, S_IRUGO);
MODULE_PARM_DESC(saying, "What to say");

static char *whom[] = { "world", "kernel", "module", "driver", "linux" };
static int whoms = sizeof(whom) / sizeof(whom[0]);
module_param_array(whom, charp, &whoms, S_IRUGO);
MODULE_PARM_DESC(whom, "Who to say hello to");

static int howmany = 1;
module_param(howmany, int, S_IRUGO);
MODULE_PARM_DESC(howmany, "how many times to say hello");

static int __init hello_init(void) {
    int i, j;
    for (i = 0; i < howmany; i++) {
        for (j = 0; j < whoms; j++) {
            printk(KERN_ALERT "%s %s\n", saying, whom[j]);
        }
    }
    return 0;
}

static void __exit hello_exit(void) {
    printk(KERN_ALERT "Exit\n");
}

module_init(hello_init);
module_exit(hello_exit);
