/*gpio test dev.c
   Copyright (c) 2013 forlinx co.ltd
   By zrj  <zhangruijie@forlinx.com> 
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
#include <linux/delay.h>
#define DEBUG 
#if defined(DEBUG)			
#define DPRINTK(fmt,arg...) printk(fmt,##arg); 
#else
#define DPRINTK(fmt,arg...)
#endif

#define GPIO_TO_PIN(bank, gpio) (32 * (bank) + (gpio))

#define LED_ON 1
#define LED_OFF 0
#define MINI_OFF 3
#define MINI_ON 4
#define DEV_NAME	"iotest"

extern void mini_setup(void);
struct minidbg_pin_struct{
		int port;
		int pin;
		int led_num;
};


struct minidbg_pin_struct minidbg_pin[]={
		{1,29,1},//led1
		{3,4,4},//led4
		{3,10,5},
		{3,9,6},
		{1,23,7},
		{1,27,8},
		{1,26,9},
		{1,24,10},
		{0,31,11},//
		{0,17,12},
		{0,16,13},
		{2,19,14},
		{2,18,15},
		{0,14,16},
		{0,15,17},
		{0,3,18},
		{0,2,19},//
		{1,9,20},
		{1,8,21},
		{0,12,22},
		{0,13,23},
		{1,19,24},
		{3,14,25},
		{0,19,26},
		{3,15,27},
		{3,16,28},//led28
		{0,27,29},
		{0,26,30},
		{0,23,31},
		{0,22,32},
		{0,5,44},
		{0,4,45},
		{1,31,33},
		{1,30,34},
		{2,1,35},
		{1,12,36},
		{1,13,37},
		{1,14,38},
		{1,15,39},
		{3,18,40},
		{3,20,41},
		{3,21,42},
		{3,19,43},//led43
		{1,22,46},
		{1,20,47},
		{1,21,48},
};



static void led_on(int num)
{
	//gpio had been requested so just use it
	gpio_set_value(GPIO_TO_PIN(1, 18 + num), 0);
	DPRINTK("led num %d turn on \n",num);
}
static void led_off(int num)
{
	gpio_set_value(GPIO_TO_PIN(1, 18 + num), 1);
	DPRINTK("led num %d turn off \n",num);
}

static int led_open(struct inode *inode, struct file *filp)
{
	int i;
	DPRINTK("led open\n");
	mini_setup();
	gpio_free(GPIO_TO_PIN(2,1));//for s2 sdio wifi
	for(i=0;i<sizeof(minidbg_pin)/sizeof(struct minidbg_pin_struct);i++){
		if(gpio_is_valid(GPIO_TO_PIN(minidbg_pin[i].port,minidbg_pin[i].pin)))
		{
			if(!gpio_request(GPIO_TO_PIN(minidbg_pin[i].port,minidbg_pin[i].pin),"mini"))
			{
					gpio_direction_output(GPIO_TO_PIN(minidbg_pin[i].port,minidbg_pin[i].pin),0);
					gpio_set_value(GPIO_TO_PIN(minidbg_pin[i].port,minidbg_pin[i].pin),1);
			}		
			else
			{
					printk("gpio_request err!\n");
					
			}
		}
		else
				printk("gpio is not valid!\n");
	}
	return 0;

}

static int led_release(struct inode *inode, struct file *filp)
{
    int i;
	DPRINTK("led release\n");
	for(i=0;i<sizeof(minidbg_pin)/sizeof(struct minidbg_pin_struct);i++)
	{
		gpio_free(GPIO_TO_PIN(minidbg_pin[i].port,minidbg_pin[i].pin));
	}
	return 0;
}


static long led_ioctl(struct file *filp,
                        unsigned int cmd, unsigned long arg)
{
	DPRINTK("led ioctl with cmd:%d,arg:%d\n",cmd,arg);

	switch (cmd) {
	case LED_ON:
		led_on(arg);
		break;
	case LED_OFF:
		led_off(arg);
		break;
	case MINI_OFF:
		DPRINTK(" led%d off \n",minidbg_pin[arg].led_num);
				gpio_set_value(GPIO_TO_PIN(minidbg_pin[arg].port, minidbg_pin[arg].pin), 1);
		break;
	case MINI_ON:
		DPRINTK(" led%d on \n",minidbg_pin[arg].led_num);
				gpio_set_value(GPIO_TO_PIN(minidbg_pin[arg].port, minidbg_pin[arg].pin), 0);
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
	return misc_register(&led_miscdev);
}

static void __exit led_exit(void)
{
   misc_deregister(&led_miscdev);
}

module_init(led_init);
module_exit(led_exit);

MODULE_AUTHOR("zrj <zhangruijie@forlinx.com>");
MODULE_DESCRIPTION("gpiotest driver");
MODULE_LICENSE("GPL");

