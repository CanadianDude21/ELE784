#include <linux/module.h>
#include <linux/init.h>

#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/kdev_t.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/slab.h>


MODULE_AUTHOR("Mathieu Fournier-Desrochers ell");
MODULE_LICENSE("Dual BSD/GPL");


typedef struct {
	uint8_t *buffer;
	uint8_t size;
	uint8_t idIn;
	uint8_t idOut;
	uint8_t nbElement;
	spinlock_t buffer_lock;
}buffer;


typedef struct mon_Module{
	dev_t mydev;
	struct class *cclass;
	struct cdev *mycdev;
	buffer Wxbuf;
	buffer Rxbuf;
	int wr_mod;
	int rd_mod;
	wait_queue_head_t waitRx, waitTx;
	
}monModule;

static int pilote_serie_open(struct inode *inode,struct file *filp);
static int pilote_serie_release(struct inode *inode,struct file *filp);
ssize_t pilote_serie_read(struct file *filp, char *buff, size_t count, loff_t *f_pos);
static ssize_t pilote_serie_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos);
static void init_buffer(uint8_t size, buffer *buff);
static void read_buffer(uint8_t* tempo, buffer *buff);
static void write_buffer(uint8_t tempo, buffer *buff);

struct file_operations monModule_fops = {
	.owner   = THIS_MODULE,
	.open    = pilote_serie_open,
	.write   = pilote_serie_write,
	.read    = pilote_serie_read,
	.release = pilote_serie_release,
};

monModule deviceTest;
int serie_major;
int serie_minor = 0;
int nbr_dvc = 1;

static int __init pilote_serie_init (void){
	int result;
	int error;
	struct device *deverror;

	deviceTest.mycdev = cdev_alloc();
	result = alloc_chrdev_region(&deviceTest.mydev,serie_minor,nbr_dvc,"SerialDev0");
	if(result<0){
		printk(KERN_WARNING "Pilote: can't get major!");
	}
	serie_major = MAJOR(deviceTest.mydev);
	deviceTest.cclass = class_create(THIS_MODULE, "SerialDev0");
	deverror = device_create(deviceTest.cclass,NULL,deviceTest.mydev,NULL,"SerialDev0");
	if(deverror==NULL){
		printk(KERN_ALERT"ERROR AT DEVICE CREATE");
	}
	deviceTest.wr_mod = 0;
	deviceTest.rd_mod = 0;
	//init_buffer(16,&(device.Wxbuf));
	//init_buffer(16,&(device.Rxbuf));
	init_waitqueue_head(&(deviceTest.waitRx));
	init_waitqueue_head(&(deviceTest.waitTx));
	cdev_init(deviceTest.mycdev, &monModule_fops);
	//cdev_init(&(deviceTest.mycdev), &monModule_fops);
	deviceTest.mycdev->owner = THIS_MODULE;	
	//deviceTest.mycdev.owner = THIS_MODULE;
	deviceTest.mycdev->ops = &monModule_fops;
	//deviceTest.mycdev.ops = &monModule_fops;
	error = cdev_add(deviceTest.mycdev, deviceTest.mydev, serie_minor);
	//error = cdev_add(&(deviceTest.mycdev), deviceTest.mydev, serie_minor);
	if(error<=0){
		printk(KERN_ALERT"%i",error);
	}
	
	printk(KERN_WARNING "Hello world!\n");
	return 0;
}

static void __exit pilote_serie_exit (void){
	device_destroy(deviceTest.cclass,deviceTest.mydev);
	class_destroy(deviceTest.cclass);
	unregister_chrdev_region(deviceTest.mydev,nbr_dvc);

	printk(KERN_ALERT "Goodby cruel world!\n");
}

static int pilote_serie_open(struct inode *inode,struct file *filp){
	/*printk(KERN_ALERT"Pilote Opened!\n");
	filp->private_data = &deviceTest;
	if(deviceTest.wr_mod == 1){
		return -ENOTTY;
	}
	if(deviceTest.rd_mod == 1){
		return -ENOTTY;
	}
	if((filp->f_flags & O_ACCMODE) == O_WRONLY){
		deviceTest.wr_mod=1;
	}
	if((filp->f_flags & O_ACCMODE) == O_RDONLY){
		deviceTest.rd_mod=1;
	}
	if((filp->f_flags & O_ACCMODE) ==O_RDWR){
		deviceTest.wr_mod=1;
		deviceTest.rd_mod=1;
	}*/
	
	return 0;
}

static int pilote_serie_release(struct inode *inode,struct file *filp){
	if((filp->f_flags & O_ACCMODE) == O_WRONLY){
		deviceTest.wr_mod=0;
	}
	if((filp->f_flags & O_ACCMODE) == O_RDONLY){
		deviceTest.rd_mod=0;
	}
	if((filp->f_flags & O_ACCMODE) ==O_RDWR){
		deviceTest.wr_mod=0;
		deviceTest.rd_mod=0;
	}
	
	printk(KERN_ALERT"Pilote Released!\n");
	return 0;
}

ssize_t pilote_serie_read(struct file *filp, char *buf, size_t count, loff_t *f_pos)
{
	monModule *module = (monModule*)filp->private_data;
	int nb_byte_max = 8;
	uint8_t BufR[min(nb_byte_max,(int)count)];
	int i;

	count = min(nb_byte_max,(int)count);
	
	
	for(i = 0; i<count;++i) 
	{
		spin_lock(&(module->Rxbuf.buffer_lock));
	
		while(module->Rxbuf.nbElement <= 0)
		{
			spin_unlock(&(module->Rxbuf.buffer_lock)); 
			if(filp->f_flags & O_NONBLOCK)
			{
				if(i == 0){
					
					return -EAGAIN;
				}
				else{
				    if(copy_to_user(buf,BufR,i))
					{
						printk(KERN_ALERT" Read error copy to user");
						return -EAGAIN;
					}
					return i;
				}					
			}
			else
			{
				wait_event_interruptible(module->waitRx,module->Rxbuf.nbElement > 0);
				spin_lock(&(module->Rxbuf.buffer_lock));
			}
		}
		read_buffer(&BufR[i],&(module->Rxbuf));
		spin_unlock(&(module->Rxbuf.buffer_lock));

	}
	
	if(copy_to_user(buf,BufR,count)){
		printk(KERN_ALERT" Read error copy to user");
		return -EAGAIN;
	}
    return count; 
}

static ssize_t pilote_serie_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos) {

    	monModule *module = (monModule*)filp->private_data;
	int nb_byte_max = 8;
	uint8_t BufW[min(nb_byte_max,(int)count)];
	int i;
	
	count = min(nb_byte_max,(int)count);

	if(copy_from_user(BufW,buf,count)){
		printk(KERN_ALERT"Write error copy from user");
		return -EAGAIN;
	}

	for(i = 0;i<count ; ++i){
		
		spin_lock(&(module->Wxbuf.buffer_lock));
	
		while(module->Wxbuf.nbElement >= module->Wxbuf.size)
		{
			spin_unlock(&(module->Wxbuf.buffer_lock));
			if(filp->f_flags & O_NONBLOCK)
			{
				if(i == 0){ 
					return -EAGAIN;
				}
				else{
					return i;
				}					
			}
			else
			{
				wait_event_interruptible(module->waitTx,module->Wxbuf.nbElement <module->Wxbuf.size);
				spin_lock(&(module->Wxbuf.buffer_lock));
			}
		}
		write_buffer(BufW[i],&(module->Wxbuf));
		spin_unlock(&(module->Wxbuf.buffer_lock));
	}
	
	return count;

}

static void init_buffer(uint8_t size, buffer *buff){
	
	buff->size = size;
	buff->idIn = 0;
	buff->idOut = 0;
	buff->nbElement = 0;
	buff->buffer= kmalloc(sizeof(uint8_t)*size, GFP_KERNEL);
	spin_lock_init(&(buff->buffer_lock));
}

static void read_buffer(uint8_t* tempo, buffer *buff){
	
	*tempo = buff->buffer[buff->idOut];
	buff->idOut = (buff->idOut + 1)%buff->size;
	buff->nbElement--;	
		
}

static void write_buffer(uint8_t tempo, buffer *buff){
	
	buff->buffer[buff->idIn] = tempo;
	buff->idIn = (buff->idIn + 1)%buff->size;
	buff->nbElement++;	
		
}

module_init(pilote_serie_init);
module_exit(pilote_serie_exit);
