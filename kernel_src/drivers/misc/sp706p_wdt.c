/*sp706p_wat.c
   ok335x's hardware sp706p wdt driver. 

   Copyright (c) 2013 forlinx co.ltd
   By ZHANGRUIJIE@FORLINX   
*/

#include <linux/init.h>
#include <linux/export.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/errno.h>
#include <linux/miscdevice.h>
#include <linux/types.h>
#include <linux/io.h>
#include <linux/fs.h>  
#include <asm/gpio.h>
#include <linux/miscdevice.h>
#include <linux/watchdog.h>

#define GPIO_TO_PIN(bank, gpio) (32 * (bank) + (gpio))

#define DEBUG 
#if defined(DEBUG)			
#define DPRINTK(fmt,arg...) printk(fmt,##arg); 
#else
#define DPRINTK(fmt,arg...)
#endif

#define DEV_NAME	"sp706p_wdt"

#define ENABLE_SP706P    0
#define DISABLE_SP706P 1
static int sp706p_wdt_disable(void)
{
	DPRINTK("sp706p_wdt disable\n");
	gpio_set_value(GPIO_TO_PIN(3, 7), 1);
	return 0;
}


static int sp706p_wdt_enable(void)
{
	DPRINTK("sp706p_wdt enable\n");
	gpio_set_value(GPIO_TO_PIN(3, 7), 0);
	return 0;
}


static int sp706p_wdt_open(struct inode *inode, struct file *filp)
{
	DPRINTK("sp706p_wdt open\n");
	if(!gpio_is_valid(GPIO_TO_PIN(3,7))){
		printk("open gpio 3-7 is unvalid\n");
		return -1;
	}
	if(gpio_request(GPIO_TO_PIN(3,7),"gpio_test")){
		printk("gpio 3-7 request fail\n");
		return -1;
	}

	gpio_direction_output(GPIO_TO_PIN(3, 7),0);

	gpio_set_value(GPIO_TO_PIN(3, 7), 1);
	return 0;
}

static int sp706p_wdt_release(struct inode *inode, struct file *filp)
{
    DPRINTK("sp706p_wdt release\n");
	sp706p_wdt_disable();
	gpio_free(GPIO_TO_PIN(3,7));
	return 0;
}


static long sp706p_wdt_ioctl(struct file *filp,
                        unsigned int cmd, unsigned long arg)
{
	DPRINTK("sp706p_wdt ioctl with cmd:%d,arg:%d\n",cmd,arg);

	switch (cmd) {
	case ENABLE_SP706P:
	  	sp706p_wdt_enable(); 
		break;
	case DISABLE_SP706P:
		sp706p_wdt_disable();
		break;
	default:
		break;
	}

	return 0;
}

static struct file_operations sp706p_wdt_fops = {
	.owner   = THIS_MODULE,
	.open    = sp706p_wdt_open,
	.release = sp706p_wdt_release,
	.unlocked_ioctl	 = sp706p_wdt_ioctl,
};

static struct miscdevice sp706p_wdt_miscdev =
{
	 .minor	= MISC_DYNAMIC_MINOR,
	 .name	= DEV_NAME,
	 .fops	= &sp706p_wdt_fops,
};


static int __init sp706p_wdt_init(void)
{
	misc_register(&sp706p_wdt_miscdev);
	return 0;
}

static void __exit sp706p_wdt_exit(void)
{
   misc_deregister(&sp706p_wdt_miscdev);
}

module_init(sp706p_wdt_init);
module_exit(sp706p_wdt_exit);

MODULE_AUTHOR("zhangruijie @ forlinx");
MODULE_DESCRIPTION("sp706p wdt driver");
MODULE_LICENSE("GPL");


