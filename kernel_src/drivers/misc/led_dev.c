/*led_dev.c
   ok335x's led driver. 

   Copyright (c) 2013 forlinx co.ltd
   By DUYAHUI  <duyahui@forlinx.com> 
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

#define DEBUG 
#if defined(DEBUG)			
#define DPRINTK(fmt,arg...) printk(fmt,##arg); 
#else
#define DPRINTK(fmt,arg...)
#endif

#define GPIO_TO_PIN(bank, gpio) (32 * (bank) + (gpio))

#define LED_ON 1
#define LED_OFF 0
#define DEV_NAME	"led"

#if defined(CONFIG_OK335XD)
static void led_on(int num)
{
	//gpio had been requested so just use it
	gpio_set_value(GPIO_TO_PIN(1, 16 + num), 0);
	DPRINTK("led num %d turn on \n",num);
}
static void led_off(int num)
{
	gpio_set_value(GPIO_TO_PIN(1, 16 + num), 1);
	DPRINTK("led num %d turn off \n",num);
}
#elif defined(CONFIG_OK335XS)
static void led_on(int num)
{
	//gpio had been requested so just use it
	gpio_set_value(GPIO_TO_PIN(3, 16 + num), 0);
}
static void led_off(int num)
{
	gpio_set_value(GPIO_TO_PIN(3, 16 + num), 1);
}
#elif defined(CONFIG_OK335XS2)
static void led_on(int num)
{
	//gpio had been requested so just use it
	gpio_set_value(GPIO_TO_PIN(1, 16 + num), 0);
	DPRINTK("led num %d turn on \n",num);
}
static void led_off(int num)
{
	gpio_set_value(GPIO_TO_PIN(1, 16 + num), 1);
	DPRINTK("led num %d turn off \n",num);
}
#endif


static int led_open(struct inode *inode, struct file *filp)
{
	DPRINTK("led open\n");
	return 0;
}

static int led_release(struct inode *inode, struct file *filp)
{
    DPRINTK("led release\n");
	return 0;
}


static long led_ioctl(struct file *filp,
                        unsigned int cmd, unsigned long arg)
{
	DPRINTK("led ioctl with cmd:%d,arg:%d\n",cmd,arg);

#if defined(CONFIG_OK335XD)
	if(arg <0 || arg >3)
		return -EINVAL;
#elif defined(CONFIG_OK335XS)
	if(arg <0 || arg >0)
		return -EINVAL;
#elif defined(CONFIG_OK335XS2)
	if(arg <0 || arg >1)
	    return -EINVAL;
#endif
	
	switch (cmd) {
	case LED_ON:
		led_on(arg);
		break;
	case LED_OFF:
		led_off(arg);
		break;
	default:
		break;
	}

	return 0;
}

static struct file_operations led_fops = {
	.owner   = THIS_MODULE,
	.open    = led_open,
	.release = led_release,
	.unlocked_ioctl	 = led_ioctl,
};

static struct miscdevice led_miscdev =
{
	 .minor	= MISC_DYNAMIC_MINOR,
	 .name	= DEV_NAME,
	 .fops	= &led_fops,
};


static int __init led_init(void)
{
	misc_register(&led_miscdev);
	return 0;
}

static void __exit led_exit(void)
{
   misc_deregister(&led_miscdev);
}

module_init(led_init);
module_exit(led_exit);

MODULE_AUTHOR("duyahui <duyahui@forlinx.com>");
MODULE_DESCRIPTION("led driver");
MODULE_LICENSE("GPL");

