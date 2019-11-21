#include "port_config.h"

int transmission_enable[2];

void init_port(monModule* module){
	
	uint32_t serial_add_copy;
	uint8_t lcr_cpy;
	uint8_t fcr_cpy;


	SetBaudRate(115200, module);
	SetDataSize(8, module);
	SetParity(2, module);
	
	serial_add_copy = module->SerialBaseAdd;
	

	lcr_cpy = inb(serial_add_copy + LCR);
	lcr_cpy = lcr_cpy  & ~(LCR_STB);
	outb(lcr_cpy, serial_add_copy + LCR);

	fcr_cpy = inb((serial_add_copy + FCR));
	fcr_cpy = fcr_cpy | FCR_FIFOEN;
	outb(fcr_cpy, (serial_add_copy + FCR));
	fcr_cpy = (fcr_cpy & ~(FCR_RCVRTRM) & ~(FCR_RCVRTRL)) | FCR_RCVRRE;
	outb(fcr_cpy, (serial_add_copy + FCR));
	printk(KERN_WARNING"fcr : %d",lcr_cpy);
	change_ETBEI(0,module);
	change_ERBFI(0,module);
	


	
}

extern irqreturn_t my_interrupt(int irq_no, void *arg){
	uint8_t ier_cpy;
	uint8_t RBR_cpy;
	uint8_t THR_cpy;	
	monModule *module = (monModule*)arg; 
	uint8_t LSR_cpy = inb((module->SerialBaseAdd + LSR));
	uint8_t LCR_cpy = inb((module->SerialBaseAdd + LCR));
	
	if(LSR_cpy & LSR_DR){

		if(LCR_cpy & LCR_DLAB){	
			outb((LCR_cpy & ~(LCR_DLAB)),(module->SerialBaseAdd + LCR));
		}
		RBR_cpy = inb((module->SerialBaseAdd));
		if(RBR_cpy != 0){
			printk(KERN_WARNING"read : %d",RBR_cpy);
			spin_lock(&(module->Rxbuf.buffer_lock));
			write_buffer(RBR_cpy,&(module->Rxbuf));
			spin_unlock(&(module->Rxbuf.buffer_lock));
			wake_up_interruptible(&(module->waitRx));
		}
		
		return IRQ_HANDLED;		
	}
	if(LSR_cpy & LSR_THRE){
		
		printk(KERN_WARNING "write");
		if(LCR_cpy & LCR_DLAB){	
			outb((LCR_cpy & ~(LCR_DLAB)),(module->SerialBaseAdd + LCR));
		}
	        spin_lock(&(module->Wxbuf.buffer_lock));
		
		if(module->Wxbuf.nbElement == 0){
			spin_unlock(&(module->Wxbuf.buffer_lock));
			printk(KERN_WARNING "plus de place");
			ier_cpy  = inb((module->SerialBaseAdd + IER));
			ier_cpy = ier_cpy & ~(IER_ETBEI);
			transmission_enable[module->minor] = 0;
			outb(ier_cpy, (module->SerialBaseAdd + IER));
		}
		else{
			read_buffer(&THR_cpy,&(module->Wxbuf));	
			spin_unlock(&(module->Wxbuf.buffer_lock));
			outb(THR_cpy,(module->SerialBaseAdd));
			wake_up_interruptible(&(module->waitTx));
			printk(KERN_WARNING"envoie : %d",THR_cpy);
		}
		
		return IRQ_HANDLED;
	}
	return IRQ_NONE;

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

void change_ERBFI(int enable, monModule* module){

	uint8_t ier_cpy = inb((module->SerialBaseAdd + IER));
	printk(KERN_WARNING "change receive mode");
	if(enable){
		ier_cpy = ier_cpy | (IER_ERBFI);
		transmission_enable[module->minor] = 1;
	}
	else{
		ier_cpy = ier_cpy & ~(IER_ERBFI);
		transmission_enable[module->minor] = 0;
	}

	outb(ier_cpy, (module->SerialBaseAdd + IER));

}

