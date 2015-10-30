#include <linux/init.h>
#include <linux/module.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/compat.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>
#include <linux/uaccess.h>
#include <linux/delay.h>

#include "ioctl_basic.h" 

#define SPIDEV_MAJOR                    153     /* assigned */
#define N_SPI_MINORS                    32      /* ... up to 256 */
 
static DECLARE_BITMAP(minors, N_SPI_MINORS);
#define SPI_MODE_MASK           (SPI_CPHA | SPI_CPOL | SPI_CS_HIGH \
                                 | SPI_LSB_FIRST | SPI_3WIRE | SPI_LOOP \
                                 | SPI_NO_CS | SPI_READY | SPI_TX_DUAL \
                                 | SPI_TX_QUAD | SPI_RX_DUAL | SPI_RX_QUAD)
 
struct spidev_data 
{
	dev_t                   devt;
	spinlock_t              spi_lock;
	struct spi_device       *spi;
	struct list_head        device_entry;
 
	/* buffer is NULL unless this device is open (users > 0) */
	struct mutex            buf_lock;
	unsigned                users;
	u8                      *buffer;
};
  
static LIST_HEAD(device_list);
static DEFINE_MUTEX(device_list_lock);
  
static unsigned bufsiz = 4096;
module_param(bufsiz, uint, S_IRUGO);
MODULE_PARM_DESC(bufsiz, "data bytes in biggest supported SPI message");
uint8_t clear_display[26] ={	0x0F, 0x00,													// to clear led display
								0x0C, 0x01,
								0x09, 0x00,
								0x0A, 0x01,
								0x0B, 0x07,
								0x01, 0x00,
								0x02, 0x00,	
								0x03, 0x00,
								0x04, 0x00,
								0x05, 0x00,
								0x06, 0x00,
								0x07, 0x00,
								0x08, 0x00,
};	
uint8_t my_array_to_send[1][2];
uint8_t array_received[10][26]; 


static struct workqueue_struct *my_wq;	

typedef struct 
{																								// work structre
	struct work_struct my_work;
	struct spidev_data      *spidev;
	int sequence_array[22];
	int ele_count;
	
}my_work_t;
my_work_t *work_w;
//int stop_command = 0;
int chek_flag;
/*-------------------------------------------------------------------------*/
  
long ioctl_funcs(struct file *file,unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	switch(cmd) 
	{
		case IOCTL_PATTERN:
		{
			ret = copy_from_user(&array_received,(void *) arg, sizeof(array_received));
			if(ret)
				return -EFAULT;
		}
		break;

							
	}
return ret;
}



static void spidev_complete(void *arg)
{
	complete(arg);
}

 
static ssize_t spidev_sync(struct spidev_data *spidev, struct spi_message *message)
{
	DECLARE_COMPLETION_ONSTACK(done);
	int status;
 
	message->complete = spidev_complete;
	message->context = &done;

	spin_lock_irq(&spidev->spi_lock);
	if (spidev->spi == NULL)
		status = -ESHUTDOWN;
	else
		status = spi_async(spidev->spi, message);
	spin_unlock_irq(&spidev->spi_lock);
 
	if (status == 0) 
	{
		wait_for_completion(&done);
		status = message->status;
		if (status == 0)
			status = message->actual_length;
	}
	return status;
}

 



 
 /*-------------------------------------------------------------------------*/
static inline ssize_t spidev_sync_write(struct spidev_data *spidev, size_t len)
{
	struct spi_transfer t = {
		.tx_buf         = spidev->buffer,
		.rx_buf 		= 0,
		.len            = 2,
		.delay_usecs	= 1,
		.speed_hz 		= 10000000,
		.bits_per_word	= 8,
		.cs_change 		= 1,

	};

	struct spi_message m;
 	int ret;
	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	ret = spidev_sync(spidev, &m);
	return ret;
}


/******************************my workqueue function for non blocking implementation****************/

static void my_write_wq_function(struct work_struct *work)
{

	int i, k, b;
	int j = 0;
	int number_of_elements;
	int my_delay;
	int ret;
	//my_work_t *my_work = (my_work_t*)work_w;
	
	number_of_elements = work_w->ele_count/sizeof(int);
	b = work_w->ele_count;
    
    //run the loop till termination sequence is received

	for(k = 0; k < number_of_elements && (work_w->sequence_array[0] != 0 || work_w->sequence_array[1] != 0);)
	{
		if (work_w->sequence_array[k] == 0 && work_w->sequence_array[k+1] == 0)
		{
			k = 0;
			continue;
		}
	
		j = work_w->sequence_array[k];
		my_delay = work_w->sequence_array[k+1];
		for ( i = 0; i < 25; i= i+2)
		{
			work_w->spidev->buffer[0]= array_received[j][i];
			work_w->spidev->buffer[1]= array_received[j][i+1];

			ret = spidev_sync_write(work_w->spidev, 1);
		}
		msleep(my_delay);
		k = k+2;
		
	}
    // to clear display
	for ( i = 0; i < 25; i= i+2)
		{
			work_w->spidev->buffer[0]= clear_display[i];
			work_w->spidev->buffer[1]= clear_display[i+1];
			ret = spidev_sync_write(work_w->spidev, 1);
		}
	return;
	

}


static ssize_t spidev_write(struct file *filp, const char __user *buf,
                 size_t count, loff_t *f_pos)
{
	
	int ret = 0;
	int res;
	int sequence_array[22]; 
	struct spidev_data      *spidev;
	spidev = filp->private_data;
	//stop_command = 0;
	mutex_lock(&spidev->buf_lock);
	res =  copy_from_user((void*)sequence_array, (void __user *)buf, sizeof(sequence_array));
	if (res) 
	{
			return -EFAULT;
	}
	work_w = (my_work_t *)kmalloc(sizeof(my_work_t), GFP_KERNEL);
	if (work_w == NULL)
			return -ENOMEM;
	work_w->spidev = kmalloc(sizeof(struct spidev_data), GFP_KERNEL);

	memcpy(work_w->spidev, spidev, sizeof(struct spidev_data));
	memcpy(work_w->sequence_array, sequence_array, sizeof(sequence_array));
	work_w->ele_count = count;

	INIT_WORK( (struct work_struct *)work_w, my_write_wq_function );
	ret = queue_work(my_wq,(struct work_struct*)work_w);            // queue the work
	
	mutex_unlock(&spidev->buf_lock);
	return ret;

}
 
 
 
static int spidev_open(struct inode *inode, struct file *filp)
{
	struct spidev_data      *spidev;
	int                     status = -ENXIO;
 
	mutex_lock(&device_list_lock);
 
	list_for_each_entry(spidev, &device_list, device_entry) {
	if (spidev->devt == inode->i_rdev) {
	                        status = 0;
                         break;
                 }
         }
        if (status == 0) {
                 if (!spidev->buffer) {
                         spidev->buffer = kmalloc(bufsiz, GFP_KERNEL);
                         if (!spidev->buffer) {
                                 dev_dbg(&spidev->spi->dev, "open/ENOMEM\n");
                                 status = -ENOMEM;
                         }
                 }
                 if (status == 0) {
                         spidev->users++;
                         filp->private_data = spidev;
                         nonseekable_open(inode, filp);
                 }
         } else
                 pr_debug("spidev: nothing for minor %d\n", iminor(inode));
 
        mutex_unlock(&device_list_lock);
         return status;
}
 
static int spidev_release(struct inode *inode, struct file *filp)
{
         struct spidev_data      *spidev;
         int                     status = 0;
 	
         mutex_lock(&device_list_lock);
         spidev = filp->private_data;
         filp->private_data = NULL;
         /* last close? */
         spidev->users--;
         if (!spidev->users) {
                 int             dofree;
 
                 /* ... after we unbound from the underlying device? */
                 spin_lock_irq(&spidev->spi_lock);
                 dofree = (spidev->spi == NULL);
                 spin_unlock_irq(&spidev->spi_lock);
 
                 if (dofree)
                         kfree(spidev);
         }
         mutex_unlock(&device_list_lock);
 
         return status;
}
 
static const struct file_operations spidev_fops = {
         .owner =        THIS_MODULE,
         .write =        spidev_write,
         //.read =         spidev_read,
         .unlocked_ioctl	 = ioctl_funcs,
         //.compat_ioctl = spidev_compat_ioctl,
         .open =         spidev_open,
         .release =      spidev_release,
         .llseek =       no_llseek,
};
 
static struct class *spidev_class;
 
static int spidev_probe(struct spi_device *spi)
{
         struct spidev_data      *spidev;
         int                     status;
         unsigned long           minor;
 
         /* Allocate driver data */
         spidev = kzalloc(sizeof(*spidev), GFP_KERNEL);
         if (!spidev)
                 return -ENOMEM;
 
         /* Initialize the driver data */
         spidev->spi = spi;
         spin_lock_init(&spidev->spi_lock);
         mutex_init(&spidev->buf_lock);
 
         INIT_LIST_HEAD(&spidev->device_entry);
         mutex_lock(&device_list_lock);
         minor = find_first_zero_bit(minors, N_SPI_MINORS);
         if (minor < N_SPI_MINORS) {
                 struct device *dev;
 
                 spidev->devt = MKDEV(SPIDEV_MAJOR, minor);
                 dev = device_create(spidev_class, &spi->dev, spidev->devt,
                                     spidev, "spidev%d.%d",
                                     spi->master->bus_num, spi->chip_select);
		status = 0;                 
		//status = PTR_ERR_OR_ZERO(dev);
         } else {
                 dev_dbg(&spi->dev, "no minor number available!\n");
                 status = -ENODEV;
         }
         if (status == 0) {
                 set_bit(minor, minors);
                 list_add(&spidev->device_entry, &device_list);
         }
         mutex_unlock(&device_list_lock);
 
         if (status == 0)
                 spi_set_drvdata(spi, spidev);
         else
                 kfree(spidev);
         return status;
}
 
static int spidev_remove(struct spi_device *spi)
{
         struct spidev_data      *spidev = spi_get_drvdata(spi);
 
         /* make sure ops on existing fds can abort cleanly */
         spin_lock_irq(&spidev->spi_lock);
         spidev->spi = NULL;
        spin_unlock_irq(&spidev->spi_lock);
 
         /* prevent new opens */
         mutex_lock(&device_list_lock);
         list_del(&spidev->device_entry);
         device_destroy(spidev_class, spidev->devt);
         clear_bit(MINOR(spidev->devt), minors);
         if (spidev->users == 0)
                 kfree(spidev);
         mutex_unlock(&device_list_lock);
 
         return 0;
}
 
static const struct of_device_id spidev_dt_ids[] = {
         { .compatible = "rohm,dh2228fv" },
         {},
};
 
MODULE_DEVICE_TABLE(of, spidev_dt_ids);
 
static struct spi_driver spidev_spi_driver = {
         .driver = {
                 .name =         "spidev",
                 .owner =        THIS_MODULE,
                 .of_match_table = of_match_ptr(spidev_dt_ids),
         },
         .probe =        spidev_probe,
         .remove =       spidev_remove,
};
 
static int __init spidev_init(void)
{
        int status;
         BUILD_BUG_ON(N_SPI_MINORS > 256);
         status = register_chrdev(SPIDEV_MAJOR, "spi", &spidev_fops);
         if (status < 0)
                 return status;
 
         spidev_class = class_create(THIS_MODULE, "spidev");
         if (IS_ERR(spidev_class)) {
                 unregister_chrdev(SPIDEV_MAJOR, spidev_spi_driver.driver.name);
                 return PTR_ERR(spidev_class);
         }
 
         status = spi_register_driver(&spidev_spi_driver);
         if (status < 0) {
                 class_destroy(spidev_class);
                 unregister_chrdev(SPIDEV_MAJOR, spidev_spi_driver.driver.name);
         }
	printk("SPI driver registered\n");
	my_wq = create_workqueue("my_queue");
        return status;
}
module_init(spidev_init);
 
static void __exit spidev_exit(void)
{
		spi_unregister_driver(&spidev_spi_driver);
		class_destroy(spidev_class);
		unregister_chrdev(SPIDEV_MAJOR, spidev_spi_driver.driver.name);
		printk("SPI driver removed\n");
}

module_exit(spidev_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Uma Kulkarni");
MODULE_DESCRIPTION("ESP Assignment 3- Fall 14");

//Reference: http://lxr.free-electrons.com/source/drivers/spi/spidev.c
/*	Copyright (C) 2006 SWAPP
	Andrea Paterniani <a.paterniani@swapp-eng.it>
	Copyright (C) 2007 David Brownell (simplification, cleanup)*/	
