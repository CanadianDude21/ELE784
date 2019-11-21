/*
 ============================================================================
 Nom         : port_config.h
 Author      : Samuel Pesant et Mathieu Fournier-Desrochers
 Date 	     : 22-10-2019
 Description : Déclare les registres et les bits du port série
 ============================================================================
 */

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
#include <asm/io.h>
#include <linux/interrupt.h>
#include "bufcirc.h"

//Adresse portIO des ports séries
#define SerialPort_Address_0 0xc030
#define SerialPort_Address_1 0xc020

//Numéros d'interruption du port série
#define SerialPort_IRQ_Address_0 20
#define SerialPort_IRQ_Address_1 21


#define nbr_registres 8

//utile au BaudRates
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


//Déclaration de la structure personnel utile à chaque instance de pilote
typedef struct{
	dev_t dev;
	struct class *cclass;
	struct cdev mycdev;
	buffer Wxbuf;
	buffer Rxbuf;
	int wr_mod;
	int rd_mod;
	wait_queue_head_t waitRx, waitTx;
	uint32_t SerialBaseAdd;
	uint32_t SerialIRQnbr;
	spinlock_t acces_mod;
	int minor;
	
}monModule;


void init_port(monModule* module);
irqreturn_t my_interrupt(int irq_no, void *arg);
void SetParity(int parity, monModule* module);
void SetDataSize(int size, monModule* module);
void SetBaudRate(int baud_rate, monModule* module);
void change_ETBEI(int enable, monModule* module);
void change_ERBFI(int enable, monModule* module);
