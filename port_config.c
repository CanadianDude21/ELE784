#include "port_config.h"

int transmission_enable[2];

void init_port(monModule* module){
	
	uint64_t serial_add_copy;
	uint8_t lcr_cpy;
	uint8_t fcr_cpy;
	uint8_t ier_cpy;


	SetBaudRate(9600, module);
	SetDataSize(8, module);
	SetParity(1, module);
	
	serial_add_copy = module->SerialBaseAdd;

	lcr_cpy = inb(serial_add_copy + LCR);
	lcr_cpy = lcr_cpy  & ~(LCR_STB);
	outb(lcr_cpy, serial_add_copy + LCR);

	fcr_cpy = inb((serial_add_copy + FCR));
	fcr_cpy = fcr_cpy | FCR_FIFOEN;
	outb(lcr_cpy, (serial_add_copy + FCR));
	fcr_cpy = (fcr_cpy & ~(FCR_RCVRTRM) & ~(FCR_RCVRTRL)) | FCR_RCVRRE;
	outb(lcr_cpy, (serial_add_copy + FCR));

	ier_cpy = inb((serial_add_copy + IER));
	ier_cpy = (ier_cpy & ~(IER_ETBEI)) | IER_ERBFI;
	outb(ier_cpy, (serial_add_copy + IER));
	transmission_enable[module->minor] = 0;




	
}

extern irqreturn_t my_interrupt(int irq_no, void *arg){
	
	uint8_t RBR_cpy;
	uint8_t THR_cpy;	
	monModule *module = (monModule*)arg; 
	uint8_t LSR_cpy = inb((module->SerialBaseAdd + LSR));
	uint8_t LCR_cpy = inb((module->SerialBaseAdd + LCR));
		
	printk(KERN_WARNING "interupt");	

	if((LSR_cpy & LSR_FE) || (LSR_cpy & LSR_PE) || (LSR_cpy & LSR_OE)){
		
		printk(KERN_WARNING "Pilote: error with LSR!");

	}
	if(LSR_cpy & LSR_DR){
		
		if(LCR_cpy & LCR_DLAB){	
			outb((LCR_cpy & ~(LCR_DLAB)),(module->SerialBaseAdd + LCR));
		}
		RBR_cpy = inb((module->SerialBaseAdd));
		spin_lock(&(module->Rxbuf.buffer_lock));
		write_buffer(RBR_cpy,&(module->Rxbuf));
		spin_unlock(&(module->Rxbuf.buffer_lock));
		wake_up_interruptible(&(module->waitRx));
				
	}
	if(LSR_cpy & LSR_THRE){
		
		if(LCR_cpy & LCR_DLAB){	
			outb((LCR_cpy & ~(LCR_DLAB)),(module->SerialBaseAdd + LCR));
		}
	        spin_lock(&(module->Wxbuf.buffer_lock));
		read_buffer(&THR_cpy,&(module->Wxbuf));
		if(module->Wxbuf.nbElement == 0 && transmission_enable[module->minor] == 1){
			change_ETBEI(0, module);
		}
		spin_unlock(&(module->Wxbuf.buffer_lock));
		printk(KERN_WARNING"envoie : %d",THR_cpy);
		outb(THR_cpy,(module->SerialBaseAdd));
		wake_up_interruptible(&(module->waitTx));
	}
	return IRQ_HANDLED;

}


void SetBaudRate(int baud_rate, monModule* module){
	
	uint8_t dl_LSB;
	uint8_t dl_MSB;
	uint16_t dl_cpy;
	uint8_t lcr_cpy = inb((module->SerialBaseAdd + LCR));
	lcr_cpy = lcr_cpy | LCR_DLAB;
	outb(lcr_cpy, (module->SerialBaseAdd + LCR));
	
	dl_cpy = f_clk/ (baud_rate*16);
	dl_LSB = (uint8_t)(dl_cpy & 0x0f);
	printk(KERN_WARNING"lsb : %d",dl_LSB);
	dl_MSB = (uint8_t)(dl_cpy & 0xf0);
	printk(KERN_WARNING"msb : %d",dl_MSB);
	outb(dl_LSB, (module->SerialBaseAdd + DL));
	outb(dl_MSB, (module->SerialBaseAdd + DL + 0x01));

	dl_LSB = inb((module->SerialBaseAdd + DL));
	printk(KERN_WARNING"lsb : %d",dl_LSB);
	dl_MSB = inb((module->SerialBaseAdd + DL + 0x01));
	printk(KERN_WARNING"msb : %d",dl_MSB);

	lcr_cpy = lcr_cpy & ~(LCR_DLAB);
	outb(lcr_cpy, (module->SerialBaseAdd + LCR));
	
		
}

void SetDataSize(int size, monModule* module){
	
	uint8_t LCR_cpy = inb((module->SerialBaseAdd + LCR));
	
	switch(size){
		
		case 5:
			LCR_cpy = LCR_cpy & ~(LCR_WLS1) & ~(LCR_WLS0);
			break;
		case 6:
			LCR_cpy = (LCR_cpy & ~(LCR_WLS1)) | LCR_WLS0;
			break;		
		case 7:
			LCR_cpy = (LCR_cpy | LCR_WLS1) & ~(LCR_WLS0);
			break;		
		case 8:
			LCR_cpy = LCR_cpy | LCR_WLS1 | LCR_WLS0;
			break;		
	}
	
	outb(LCR_cpy, (module->SerialBaseAdd + LCR));
		
}

void SetParity(int parity, monModule* module){
	
	uint8_t LCR_cpy = inb((module->SerialBaseAdd + LCR));
	
	switch(parity){
		
		case 0:
			LCR_cpy = LCR_cpy & ~(LCR_PEN);
			break;
		case 1:
			LCR_cpy = (LCR_cpy | LCR_PEN) & ~(LCR_EPS);
			break;		
		case 2:
			LCR_cpy = LCR_cpy | LCR_PEN | LCR_EPS;
			break;		
	
	}
	
	outb(LCR_cpy, (module->SerialBaseAdd + LCR));
	
}


void change_ETBEI(int enable, monModule* module){
	
	uint8_t ier_cpy = inb((module->SerialBaseAdd + IER));
	printk(KERN_WARNING "change transmission mode");
	if(enable){
		ier_cpy = ier_cpy | (IER_ETBEI);
		transmission_enable[module->minor] = 1;
	}
	else{
		ier_cpy = ier_cpy & ~(IER_ETBEI);
		transmission_enable[module->minor] = 0;
	}

	outb(ier_cpy, (module->SerialBaseAdd + IER));

}



