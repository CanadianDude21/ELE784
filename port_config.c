#include "port_config.h"

int transmission_enable;

void init_port(monModule* module){
	
	uint64_t serial_add_copy;
	uint8_t lcr_cpy;
	uint8_t fcr_cpy;
	uint8_t ier_cpy;

	SetBaudRate(115200, module);
	SetDataSize(8, module);
	SetParity(1, module);
	
	serial_add_copy = module->SerialBaseAdd;

	lcr_cpy = ioread8((void *)(serial_add_copy + LCR));
	lcr_cpy = lcr_cpy  & ~(LCR_STB);
	iowrite8(lcr_cpy, (void *)(serial_add_copy + LCR));

	fcr_cpy = ioread8((void *)(serial_add_copy + FCR));
	fcr_cpy = fcr_cpy | FCR_FIFOEN;
	iowrite8(lcr_cpy, (void *)(serial_add_copy + FCR));
	fcr_cpy = (fcr_cpy & ~(FCR_RCVRTRM) & ~(FCR_RCVRTRL)) | FCR_RCVRRE;
	iowrite8(lcr_cpy, (void *)(serial_add_copy + FCR));

	ier_cpy = ioread8((void *)(serial_add_copy + IER));
	ier_cpy = (ier_cpy & ~(IER_ETBEI)) | IER_ERBFI;
	iowrite8(ier_cpy, (void *)(serial_add_copy + IER));
	transmission_enable = 0;

	
}

extern irqreturn_t my_interrupt(int irq_no, void *arg){
	
	uint8_t RBR_cpy;
	uint8_t THR_cpy;	
	monModule *module = (monModule*)arg; 
	uint8_t LSR_cpy = ioread8((void *)(module->SerialBaseAdd + LSR));
	uint8_t LCR_cpy = ioread8((void *)(module->SerialBaseAdd + LCR));
		
		
	if((LSR_cpy & LSR_FE) || (LSR_cpy & LSR_PE) || (LSR_cpy & LSR_OE)){
		
		printk(KERN_WARNING "Pilote: error with LSR!");

	}
	if(LSR_cpy & LSR_DR){
		
		if(LCR_cpy & LCR_DLAB){	
			iowrite8((LCR_cpy & ~(LCR_DLAB)),(void *)(module->SerialBaseAdd + LCR));
		}
		RBR_cpy = ioread8((void *)(module->SerialBaseAdd));
		spin_lock(&(module->Rxbuf.buffer_lock));
		write_buffer(RBR_cpy,&(module->Rxbuf));
		spin_unlock(&(module->Rxbuf.buffer_lock));
		wake_up_interruptible(&(module->waitRx));
				
	}
	if(LSR_cpy & LSR_THRE){
		
		if(LCR_cpy & LCR_DLAB){	
			iowrite8((LCR_cpy & ~(LCR_DLAB)),(void *)(module->SerialBaseAdd + LCR));
		}
	        spin_lock(&(module->Wxbuf.buffer_lock));
		read_buffer(&THR_cpy,&(module->Wxbuf));
		if(module->Wxbuf.nbElement == 0 && transmission_enable == 1){
			change_ETBEI(0, module);
		}
		spin_unlock(&(module->Wxbuf.buffer_lock));
		iowrite8(THR_cpy,(void *)(module->SerialBaseAdd));
		wake_up_interruptible(&(module->waitTx));
	}
	return IRQ_HANDLED;

}


void SetBaudRate(int baud_rate, monModule* module){
	
	uint16_t dl_cpy;
	uint8_t lcr_cpy = ioread8((void *)(module->SerialBaseAdd + LCR));
	lcr_cpy = lcr_cpy | LCR_DLAB;
	iowrite8(lcr_cpy, (void *)(module->SerialBaseAdd + LCR));
	
	dl_cpy = f_clk/(16*baud_rate);
	iowrite16(dl_cpy, (void *)(module->SerialBaseAdd + DL));
	
	lcr_cpy = lcr_cpy & ~(LCR_DLAB);
	iowrite8(lcr_cpy, (void *)(module->SerialBaseAdd + LCR));
		
}

void SetDataSize(int size, monModule* module){
	
	uint8_t LCR_cpy = ioread8((void *)(module->SerialBaseAdd + LCR));
	
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
	
	iowrite8(LCR_cpy, (void *)(module->SerialBaseAdd + LCR));
		
}

void SetParity(int parity, monModule* module){
	
	uint8_t LCR_cpy = ioread8((void *)(module->SerialBaseAdd + LCR));
	
	switch(parity){
		
		case 0:
			LCR_cpy = LCR_cpy & ~(LCR_PEN);
			break;
		case 1:
			LCR_cpy = (LCR_cpy | LCR_PEN) & ~(LCR_STB);
			break;		
		case 2:
			LCR_cpy = LCR_cpy | LCR_PEN | LCR_STB;
			break;		
	
	}
	
	iowrite8(LCR_cpy, (void *)(module->SerialBaseAdd + LCR));
	
}


void change_ETBEI(int enable, monModule* module){

	uint8_t ier_cpy = ioread8((void *)(module->SerialBaseAdd + IER));

	if(enable){
		ier_cpy = ier_cpy | (IER_ETBEI);
		transmission_enable = 1;
	}
	else{
		ier_cpy = ier_cpy & ~(IER_ETBEI);
		transmission_enable = 0;
	}

	iowrite8(ier_cpy, (void *)(module->SerialBaseAdd + IER));

}



