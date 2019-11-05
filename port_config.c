#include "port_config.h"


void init_port(struct monModule* module){
	
	uint32_t serial_add_copy;
	uint8_t lcr_cpy;
	uint16_t dl_cpy;
	uint8_t fcr_cpy;
	uint8_t ier_cpy;


	serial_add_copy = module->SerialBaseAdd;

	lcr_cpy = ioread8(serial_add_copy + LCR);
	lcr_cpy = lcr_cpy | LCR_DLAB;
	iowrite8(lcr_cpy, serial_add_copy + LCR);
	
	dl_cpy = BAUD_115200; 
	iowrite16(dl_cpy, serial_add_copy + DL);

	lcr_cpy = ioread8(serial_add_copy + LCR);
	lcr_cpy = lcr_cpy & ~(LCR_DLAB) & ~(LCR_EPS) & ~(LCR_PEN) & ~(LCR_STB)| LCR_WLS1 | 		LCR_WLS2;
	iowrite8(lcr_cpy, serial_add_copy + LCR);
	fcr_cpy = fcr_cpy | fcr_FIFOEN;
	iowrite8(lcr_cpy, serial_add_copy + FCR);
	fcr_cpy = fcr_cpy & ~(FCR_RCVRTRM) & ~(FCR_RCVRTRL) | FCR_RCVRRE;
	iowrite8(lcr_cpy, serial_add_copy + FCR);

	ier_cpy = ioread8(serial_add_copy + IER);
	ier_cpy = ier_cpy & ~(IER_ETBEI) | IER_ERBFI;
	iowrite(ier_cpy, serial_add_copy + IER);
	
}



irqreturn_t my_interrupt(int irq_no, void *arg){

	struct monModule *module = (struct monModule*)arg; 

	return IRQ_HANDLED;

}

