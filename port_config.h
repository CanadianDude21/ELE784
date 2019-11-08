#include <asm/io.h>

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



void init_port(struct monModule* module);
irqreturn_t my_interrupt(int irq_no, void *arg);
void SetParity(int parity, struct monModule* module);
void SetDataSize(int size, struct monModule* module);
void SetBaudRate(int baud_rate, struct monModule* module);
