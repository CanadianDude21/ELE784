#include <generated/autoconf.h>
#include <linux/module.h>
#include <linux/init.h>
#include <asm/io.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/kdev_t.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include "bufcirc.h"
#include "cmdioctl.h"
//#include "port_config.h"


#define SerialPort_Address_0 0xc030
#define SerialPort_Address_1 0xc020

#define SerialPort_IRQ_Address_0 21
#define SerialPort_IRQ_Address_1 22

#define nbr_registres 8

#define f_clk 1843200


#define DL  0x00

#define IER 0x01
#define IER_ERBFI 0x01
#define IER_ETBEI 0x02

#define FCR 0x02
#define FCR_FIFOEN 0x01
#define FCR_RCVRRE 0x02
#define FCR_RCVRTRL 0x40
#define FCR_RCVRTRM 0x80

#define LCR         0x03
#define LCR_WLS0    0x01
#define LCR_WLS1    0x02
#define LCR_STB     0x04
#define LCR_PEN     0x08
#define LCR_EPS     0x10
#define LCR_DLAB    0x80

#define LSR              0x05
#define LSR_DR           0x01
#define LSR_OE           0x02
#define LSR_PE           0x04
#define LSR_FE           0x08
#define LSR_THRE         0x20
#define LSR_TEMT         0x40

typedef struct {
	dev_t dev;
	struct class *cclass;
	struct cdev mycdev;
	buffer Wxbuf;
	buffer Rxbuf;
	int wr_mod;
	int rd_mod;
	wait_queue_head_t waitRx, waitTx;
	int SerialBaseAdd;
	int SerialIRQnbr;
	
}monModule;

void init_port(monModule* module);
extern irqreturn_t my_interrupt(int irq_no, void *arg);
void SetParity(int parity, monModule* module);
void SetDataSize(int size, monModule* module);
void SetBaudRate(int baud_rate, monModule* module);

int pilote_serie_open(struct inode *inode,struct file *filp);
int pilote_serie_release(struct inode *inode,struct file *filp);
ssize_t pilote_serie_read(struct file *filp, char *buff, size_t count, loff_t *f_pos);
static ssize_t pilote_serie_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos);
ssize_t pilote_serie_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);

struct file_operations monModule_fops = {
	.owner   = THIS_MODULE,
	.open    = pilote_serie_open,
	.write   = pilote_serie_write,
	.read    = pilote_serie_read,
	.release = pilote_serie_release,
	.unlocked_ioctl   = pilote_serie_ioctl,
};

static int pilote_serie_init(void);
static void pilote_serie_exit(void);

module_init(pilote_serie_init);
module_exit(pilote_serie_exit);

