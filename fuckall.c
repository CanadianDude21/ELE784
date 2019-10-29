#include <linux/module.h>
#include <linux/init.h>

#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/kdev_t.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/device.h>

MODULE_AUTHOR("Mathieu Fournier-Desrochers");
MODULE_LICENSE("Dual BSD/GPL");

int pilote_serie_open(struct inode *inode,struct file *filp);
int pilote_serie_release(struct inode *inode,struct file *filp);

struct file_operations monModule_fops = {
	.owner   = THIS_MODULE,
	.open    = pilote_serie_open,
	//.write   = pilote_serie_write,
	//.read    = pilote_serie_read,
	.release = pilote_serie_release,
};

typedef struct {
	dev_t dev;
	struct class *cclass;
	struct cdev mycdev;
	//buffer roundTXbuf;
	//buffer roundRXbuf;
	int wr_mod;
	int rd_mod;
	wait_queue_head_t waitRX, waitTX;
	
}monModule;

monModule device;
int serie_major;
int serie_minor = 0;
int nbr_dvc = 1;

static int __init pilote_serie_init (void){
	int result;
	result = alloc_chrdev_region(&device.dev,serie_minor,nbr_dvc,"PiloteSerie");
	if(result<0){
		printk(KERN_WARNING "Pilote: can't get major!");
	}
	serie_major = MAJOR(device.dev);
	device.cclass = class_create(THIS_MODULE, "PiloteSerie");
	device_create(device.cclass,NULL,device.dev,NULL,"PiloteSerie");
	device.wr_mod = 0;
	device.rd_mod = 0;
	cdev_init(&(device.mycdev), &monModule_fops);
	cdev_add(&(device.mycdev), device.dev, serie_minor);
		
	
	printk(KERN_WARNING "Hello world!\n");
	return 0;
}

static void __exit pilote_serie_exit (void){
	device_destroy(device.cclass,device.dev);
	class_destroy(device.cclass);
	unregister_chrdev_region(device.dev,nbr_dvc);

	printk(KERN_ALERT "Goodby cruel world!\n");
}

int pilote_serie_open(struct inode *inode,struct file *filp){
	filp->private_data = &device;
	printk(KERN_WARNING"Pilote Opened!");
	if(device.wr_mod == 1){
		return -ENOTTY;
	}
	if(device.rd_mod == 1){
		return -ENOTTY;
	}
	if((filp->f_flags & O_ACCMODE) == O_WRONLY){
		device.wr_mod=1;
	}
	if((filp->f_flags & O_ACCMODE) == O_RDONLY){
		device.rd_mod=1;
	}
	if((filp->f_flags & O_ACCMODE) ==O_RDWR){
		device.wr_mod=1;
		device.rd_mod=1;
	}
	
	return 0;
}

int pilote_serie_release(struct inode *inode,struct file *filp){
	if((filp->f_flags & O_ACCMODE) == O_WRONLY){
		device.wr_mod=0;
	}
	if((filp->f_flags & O_ACCMODE) == O_RDONLY){
		device.rd_mod=0;
	}
	if((filp->f_flags & O_ACCMODE) ==O_RDWR){
		device.wr_mod=0;
		device.rd_mod=0;
	}
	
	printk(KERN_WARNING"Pilote Released!");
	return 0;
}
module_init(pilote_serie_init);
module_exit(pilote_serie_exit);
