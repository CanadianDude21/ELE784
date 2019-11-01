#ifndef DRV784_H
#define DRV784_H

#include <generated/autoconf.h>
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
#include "bufcirc.h"
#include "cmdioctl.h"

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
int pilote_serie_ioctl(struct *inode, struct file *filp, unsigned int cmd, unsigned long arg);

struct file_operations monModule_fops = {
	.owner   = THIS_MODULE,
	.open    = pilote_serie_open,
	.write   = pilote_serie_write,
	.read    = pilote_serie_read,
	.release = pilote_serie_release,
	.ioctl   = pilote_serie_ioctl
};

static int pilote_serie_init(void);
static void pilote_serie_exit(void);

module_init(pilote_serie_init);
module_exit(pilote_serie_exit);

#endif /* DRV784_H */
