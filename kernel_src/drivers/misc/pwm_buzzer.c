/*pwm_buzzer.c
   ok335x's buzzer driver. 

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
#include <linux/pwm/pwm.h>
#include <linux/fs.h>  
#include <asm/gpio.h>


#define DEBUG 
#if defined(DEBUG)			
#define DPRINTK(fmt,arg...) printk(fmt,##arg); 
#else
#define DPRINTK(fmt,arg...)
#endif


#define BUZZER_FREQENCY 1
#define DEV_NAME	"buzzer"


/*pwm for this buzzer*/
struct pwm_device *pwm = NULL;

static int buzzer_open(struct inode *inode, struct file *filp)
{
	if(pwm != NULL)
		return -EBUSY;
#if defined(CONFIG_OK335XS2)	
	pwm = pwm_request("ehrpwm.1", 0, "buzzer");
#elif defined(CONFIG_OK335XS)
	pwm = pwm_request("ecap.0", -1, "buzzer");
#endif

//	DPRINTK("buzzer open\n");
	if(pwm == NULL)
	{
			DPRINTK("buzzer open error\n");
	}
	return 0;
}

static int buzzer_release(struct inode *inode, struct file *filp)
{
	pwm_stop(pwm);
	pwm_release(pwm);
	pwm = NULL;
   	DPRINTK("buzzer release\n");
	return 0;
}


static long buzzer_ioctl(struct file *filp,
                        unsigned int cmd, unsigned long arg)
{
	DPRINTK("buzzer ioctl with cmd:%d,arg:%d\n",cmd,arg);

	if(pwm == NULL)
		return -EINVAL;

	if(arg > 20000 || arg < 0)
		return -EINVAL;

	switch (cmd) {
	case BUZZER_FREQENCY:
		if(arg==0)
			pwm_stop(pwm);
		else
		{
#if defined(CONFIG_OK335XS2)
			pwm_set_period_ns(pwm, 1000000000/arg);
            pwm_set_duty_ns(pwm, 25000);
		//   pwm_set_period_ns(pwm, 500000);
		//	pwm_set_duty_ns(pwm, arg/2+20000);
#elif defined(CONFIG_OK335XS)
			pwm_set_period_ns(pwm, 1000000000/arg);
                        pwm_set_duty_ns(pwm, 25000);
#endif
			pwm_start(pwm);
		}
		
		break;
	default:
		break;
	}

	return 0;
}

static struct file_operations buzzer_fops = {
	.owner   = THIS_MODULE,
	.unlocked_ioctl	 = buzzer_ioctl,
	.open    = buzzer_open,
	.release = buzzer_release,
};

static struct miscdevice buzzer_miscdev =
{
	 .minor	= MISC_DYNAMIC_MINOR,
	 .name	= DEV_NAME,
	 .fops	= &buzzer_fops,
};


static int __init buzzer_init(void)
{
	misc_register(&buzzer_miscdev);
	return 0;
}

static void __exit buzzer_exit(void)
{
   misc_deregister(&buzzer_miscdev);
}

module_init(buzzer_init);
module_exit(buzzer_exit);

MODULE_AUTHOR("duyahui <duyahui@forlinx.com>");
MODULE_DESCRIPTION("pwm_buzzer driver");
MODULE_LICENSE("GPL");

