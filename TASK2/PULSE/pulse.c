#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/string.h>
#include <linux/device.h>
#include <linux/jiffies.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/param.h>
#include <linux/list.h>
#include <linux/semaphore.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/delay.h>	
#include <linux/workqueue.h>
#include <linux/interrupt.h>


#define DEVICE_NAME   "gmem"  										// device name to be created and registered

/* per device structure */
struct cdev cdev;                      									  /* The cdev structure */
static dev_t my_dev_number;												 /* Allotted device number */
struct class *my_dev_class; 											/* Tie with the device model */
	
struct work_struct *my_work;
static struct workqueue_struct *my_wq;

unsigned int blink_gpio = 14;
unsigned int interrupt_gpio = 15;

unsigned int irq_line;	
uint64_t ct, cts;
long int_time;
int int_flag = 1;
int ready_flag;
uint64_t tmp1, tmp2;

uint64_t tsc(void)               					  		   	 // to convert 32 bit timestamp output into 64 bits
{
	uint32_t a, d;
	//asm("cpuid");
	asm volatile("rdtsc" : "=a" (a), "=d" (d));
	return (( (uint64_t)a)|( (uint64_t)d)<<32 );
}


static irqreturn_t change_state_interrupt (int irq, void *dev_id)
{
	
	//printk(KERN_ALERT "Interrupt\n");
	ct = tsc();											
	queue_work(my_wq,(struct work_struct*)my_work);									//send work to be scheduled		
	return IRQ_HANDLED;
}

static void my_wq_function(struct work_struct *work)
{
	
	if ( int_flag == 1)
	{	
		//printk(KERN_ALERT "Rising Interrupt occured\n");
		tmp1 = ct;
		irq_set_irq_type(irq_line, IRQF_TRIGGER_FALLING);		
		int_flag = 0;
	}
	else if (int_flag == 0)
	{
		//printk(KERN_ALERT "Falling Interrupt occured\n");
		tmp2 = ct;
		cts = tmp2 - tmp1;
		int_time = (long)cts;
		ready_flag = 1;
		irq_set_irq_type(irq_line, IRQF_TRIGGER_RISING);
		int_flag = 1;

	}
		
	return;
}


/* ************ Open my driver**************** */


int my_driver_open(struct inode *inode, struct file *file)
{
	
	//printk(KERN_INFO "Driver: open()\n");
	/* Get the per-device structure that contains this cdev */	
	/* Easy access to dev from rest of the entry points */	
	return 0;
}



/* ********* Release my driver**************** */


int my_driver_release(struct inode *inode, struct file *file)
{
	
	//printk(KERN_INFO "Driver: close()\n");
	return 0;
}


/* ************ Read to my driver*************** */


ssize_t my_driver_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	int ret = 0;
	
	if ( ready_flag == 1)																	// check till work gets ready
	{
		ret = copy_to_user(buf, &int_time, 8);
		ready_flag = 0;
		return 0;

	}
	else 
		return -EBUSY;	
	
}

/* ************* Write to my driver**************** */

ssize_t my_driver_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
	gpio_set_value_cansleep(14, 0);
	msleep(100);
	gpio_set_value_cansleep(14, 1);
	msleep(10);
	gpio_set_value_cansleep(14, 0);

	return 0;
}	


/* File operations structure. Defined in linux/fs.h */

static struct file_operations my_dev_fops = {
	.owner = THIS_MODULE, 				/* Owner */
	.open = my_driver_open, 		/* Open method */
	.release = my_driver_release,	/* Release method */
	.write = my_driver_write, 		/* Write method */
	.read = my_driver_read, 		/* Read method */
};

/* **************** Driver Initialization****************** */

int __init my_driver_init(void)
{
	int ret, res, retval;	
	
	my_wq = create_workqueue("my_queue");
	if(!my_wq)
	{
		printk(KERN_INFO "work queue init failed\n");
		return -1;
	} 
	gpio_request(31, "muxselect");															//to enable SDA and SCL/ A4, A5
	gpio_direction_output(31,0);
	gpio_set_value_cansleep(31, 0);
	gpio_request(30, "muxselect1");															//to enable SDA and SCL/ A4, A5
	gpio_direction_output(30,0);
	gpio_set_value_cansleep(30, 0);
	
	gpio_request(15,"echo");																// enabling busy LED
	gpio_direction_input(15);
	gpio_request(14,"trigger");																// enabling busy LED
	gpio_direction_output(14,0);	
	gpio_set_value_cansleep(14, 0);

	if ( (irq_line = gpio_to_irq(15)) < 0)
	{
		printk(KERN_ALERT "Gpio %d cannot be used as interrupt",interrupt_gpio);
		kfree(my_wq);
		return -EINVAL;
	}
	if ( (res = request_irq(irq_line,change_state_interrupt, IRQF_TRIGGER_RISING,"gpio_change_state", NULL)) < 0)
	{
		printk("request irq call failed\n");
		if (res == -EBUSY)
		retval = res;
		else
		retval = -EINVAL;
		kfree(my_wq);
		return retval;
	}
	else
		int_flag = 1;																				// rising edge


	// create work queue
	my_work = (struct work_struct *)kmalloc(sizeof(struct work_struct),GFP_KERNEL);
	if (my_work == NULL)
	{
		kfree(my_wq);
		return -ENOMEM;
	}

	INIT_WORK( (struct work_struct *)my_work, my_wq_function );

	/* Request dynamic allocation of a device major number */
	if (alloc_chrdev_region(&my_dev_number, 0, 1, DEVICE_NAME) < 0)
	{
		printk(KERN_DEBUG "Can't register device\n"); return -1;
	}

	/* Populate sysfs entries */
	my_dev_class = class_create(THIS_MODULE, DEVICE_NAME);



	/* Send uevents to udev, so it'll create /dev nodes */
	device_create(my_dev_class, NULL ,my_dev_number, NULL, DEVICE_NAME);

	/* Connect the file operations with the cdev */
	cdev_init(&cdev, &my_dev_fops);
	cdev.owner = THIS_MODULE;

	/* Connect the major/minor number to the cdev */
	ret = cdev_add(&cdev, (my_dev_number), 1);

	if (ret)
	{
		printk("Bad cdev\n");
		return ret;
	}
	     
	printk("Driver initialized.\n");
	return 0;


}

/* ******************Driver Exit******************* */

void __exit my_driver_exit(void)
{
	cdev_del(&cdev);
	device_destroy (my_dev_class, MKDEV(MAJOR(my_dev_number), 0));
	class_destroy(my_dev_class);
	
	/* Release the major number */
	unregister_chrdev_region((my_dev_number), 1);
	free_irq(irq_line,NULL);

	/* Destroy device */
	
	
	/* Destroy driver_class */
	gpio_free(30);	
	gpio_free(31);	
	gpio_free(15);
	gpio_free(14);	
	kfree(my_wq);
	printk("Pulse driver removed.\n");
}



module_init (my_driver_init);
module_exit (my_driver_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Uma Kulkarni");
MODULE_DESCRIPTION("ESP Assignment 3- Fall 14");

//Refereces: http://mondi.web.cs.unibo.it/gpio_control.html
