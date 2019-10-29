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

typedef struct {
	dev_t dev;
	struct class *cclass;
	struct cdev mycdev;
	buffer Wxbuf;
	buffer Rxbuf;
	int wr_mod;
	int rd_mod;
	wait_queue_head_t waitRx, waitTx;
	
}monModule;

int pilote_serie_open(struct inode *inode,struct file *filp);
int pilote_serie_release(struct inode *inode,struct file *filp);
ssize_t pilote_serie_read(struct file *filp, char *buff, size_t count, loff_t *f_pos);
static ssize_t pilote_serie_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos);
static void init_buffer(uint8_t size, buffer *buff);
static void read_buffer(uint8_t* tempo, buffer *buff);
static void write_buffer(uint8_t tempo, buffer *buff);
static int resize_buffer(buffer *buffrx, buffer *bufftx, size_t size);
static int get_buffer_size(buffer *buff);

struct file_operations monModule_fops = {
	.owner   = THIS_MODULE,
	.open    = pilote_serie_open,
	.write   = pilote_serie_write,
	.read    = pilote_serie_read,
	.release = pilote_serie_release,
};

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
	device_create(device.cclass,NULL,device.dev,NULL,"SerialDev0");
	device.wr_mod = 0;
	device.rd_mod = 0;
	init_buffer(200,&(device.Wxbuf));
	init_buffer(200,&(device.Rxbuf));
	init_waitqueue_head(&(device.waitRx));
	init_waitqueue_head(&(device.waitTx));
	cdev_init(&(device.mycdev), &monModule_fops);
	cdev_add(&(device.mycdev), device.dev, 1);
	
	
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
	printk(KERN_ALERT"Pilote Opened!\n");
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
	
	printk(KERN_ALERT"Pilote Released!\n");
	return 0;
}

ssize_t pilote_serie_read(struct file *filp, char *buf, size_t count, loff_t *f_pos)
{
	monModule *module = (monModule*)filp->private_data;
	int nb_byte_max = 8;
	uint8_t BufR[nb_byte_max];
	int nb_data_read = 0;
	int i;
	
	while(nb_data_read < count){
		
		for(i = 0; i< min((int)count-nb_data_read,nb_byte_max);++i){
		
			spin_lock_irq(&(module->Wxbuf.buffer_lock));
		
			while(module->Wxbuf.nbElement <= 0)
			{
				spin_unlock(&(module->Wxbuf.buffer_lock)); 
				if(filp->f_flags & O_NONBLOCK)
				{
					if(nb_data_read == 0){
						
						return -EAGAIN;
					}
					else{
						if(copy_to_user(buf,BufR,i))
						{
							printk(KERN_ALERT" Read error copy to user");
							return -EAGAIN;
						}
						nb_data_read += i;
						return nb_data_read;
					}					
				}
				else
				{
					wait_event_interruptible(module->waitRx,module->Wxbuf.nbElement > 0);
					spin_lock_irq(&(module->Wxbuf.buffer_lock));
				}
			}
		
			read_buffer(&BufR[i],&(module->Wxbuf));
			spin_unlock(&(module->Wxbuf.buffer_lock));
		}
		if(copy_to_user(&buf[nb_data_read],BufR,i)){
			printk(KERN_ALERT" Read error copy to user");
			return -EAGAIN;
		}
		nb_data_read += i;
	}
    return nb_data_read; 
}

static ssize_t pilote_serie_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos) {

    monModule *module = (monModule*)filp->private_data;
	int nb_byte_max = 8;
	int nb_data_write = 0;
	uint8_t BufW[count];
    int nb_a_transmettre;
	int i;

	

	
	while(nb_data_write < count){
		
		nb_a_transmettre = min((int)count-nb_data_write,nb_byte_max);

		if(copy_from_user(BufW,&(buf[nb_data_write]),nb_a_transmettre)){
			printk(KERN_ALERT"Write error copy from user");
			return -EAGAIN;
		}

		for(i = 0;i<nb_a_transmettre ; ++i){
			
			spin_lock_irq(&(module->Wxbuf.buffer_lock));
		
			while(module->Wxbuf.nbElement >= module->Wxbuf.size)
			{
				spin_unlock(&(module->Wxbuf.buffer_lock));
				if(filp->f_flags & O_NONBLOCK)
				{
					if(nb_data_write == 0)
					{ 
						return -EAGAIN;
					}
					else
					{
						return nb_data_write+i;
					}					
				}
				else
				{
					wait_event_interruptible(module->waitTx,module->Wxbuf.nbElement <module->Wxbuf.size);
					spin_lock_irq(&(module->Wxbuf.buffer_lock));
				}
			}
			write_buffer(BufW[i],&(module->Wxbuf));
			spin_unlock(&(module->Wxbuf.buffer_lock));
		}
		nb_data_write += i;
	}
	
	return nb_data_write;

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

static int resize_buffer(buffer *buffrx, buffer *bufftx, size_t size){

	if(buffrx->nbElement > size || bufftx->nbElement > size){
		return -EAGAIN;	
	}
	
	krealloc(bufftx->buffer,size, GFP_KERNEL);
	krealloc(buffrx->buffer,size, GFP_KERNEL);
	
	
}

static int get_buffer_size(buffer *buff){

	return (int)(buff->size);
}


module_init(pilote_serie_init);
module_exit(pilote_serie_exit);
