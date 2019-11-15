#include "driver784.h"


MODULE_AUTHOR("Mathieu Fournier-Desrochers ell");
MODULE_LICENSE("Dual BSD/GPL");

monModule device;

int serie_major;
uint8_t transmission_enable = 0;
int serie_minor = 0;
int nbr_dvc = 1;

static int __init pilote_serie_init (void){
	
	int result;
	
	device.SerialBaseAdd = SerialPort_Address_0;
	device.SerialIRQnbr = SerialPort_IRQ_Address_0;
	
	result = alloc_chrdev_region(&device.dev,serie_minor,nbr_dvc,"PiloteSerie");
	if(result<0){
		printk(KERN_WARNING "Pilote: can't get major!");
		return -ENOTTY;
	}
	if(request_region(device.SerialBaseAdd, nbr_registres, "pilote_serie")){
		printk(KERN_WARNING "Pilote: error with request region!");
		return -ENOTTY;
	}
	if(request_irq(device.SerialIRQnbr, &my_interrupt, IRQF_SHARED, "pilote_serie",&(device))){
		printk(KERN_WARNING "Pilote: error with request IRQ!");
		return -ENOTTY;
	}

	//ioport_map(device.SerialBaseAdd,nbr_registres);

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
	cdev_add(&(device.mycdev), device.dev, nbr_dvc);

	
	//init_port(&device);
	
	printk(KERN_WARNING "Hello world!\n");
	return 0;
}

static void __exit pilote_serie_exit (void){
	//ioport_unmap((void *)(device.SerialBaseAdd));
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
		
			spin_lock_irq(&(module->Rxbuf.buffer_lock));
		
			while(module->Rxbuf.nbElement <= 0)
			{
				spin_unlock(&(module->Rxbuf.buffer_lock)); 
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
					spin_lock_irq(&(module->Rxbuf.buffer_lock));
				}
			}
		
			read_buffer(&BufR[i],&(module->Rxbuf));
			spin_unlock(&(module->Rxbuf.buffer_lock));
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
			change_ETBEI(1, module);

		}
		nb_data_write += i;
	}
	
	return nb_data_write;

}

ssize_t pilote_serie_ioctl(struct file *filp, unsigned int cmd, unsigned long arg){
	
	int retval = 0;
	if(_IOC_TYPE(cmd) != MAGIC_NUMBER_PILOTE){
		printk(KERN_WARNING"Error magic number");
		return -EAGAIN;
	}
	if(_IOC_NR(cmd) > MAX_NBR_IOCCMD ){
		printk(KERN_WARNING"command does not exist: %d",_IOC_NR(cmd));
		return -EAGAIN;
	}
	
	switch(cmd){
		
	case SET_BAUD_RATE:
		
		break;
	case SET_DATA_SIZE:
		break;
	case SET_PARITY:
		break;
	case GET_BUF_SIZE:
		retval = get_buffer_size(&(device.Wxbuf));
		printk(KERN_WARNING "GET_BUF_SIZE: %d",retval);
		break;
	case SET_BUF_SIZE:
		retval = resize_buffer(&(device.Rxbuf),&(device.Wxbuf),arg);
		printk(KERN_WARNING "SET_BUF_SIZE: %d",retval);
		break;
	default:
		return -EAGAIN;
	}
	return 0;
}

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


void init_buffer(uint8_t size, buffer *buff){
	
	buff->size = size;
	buff->idIn = 0;
	buff->idOut = 0;
	buff->nbElement = 0;
	buff->buffer= kmalloc(sizeof(uint8_t)*size, GFP_KERNEL);
	spin_lock_init(&(buff->buffer_lock));
}

void read_buffer(uint8_t* tempo, buffer *buff){
	
	*tempo = buff->buffer[buff->idOut];
	buff->idOut = (buff->idOut + 1)%buff->size;
	buff->nbElement--;	
		
}

void write_buffer(uint8_t tempo, buffer *buff){
	
	buff->buffer[buff->idIn] = tempo;
	buff->idIn = (buff->idIn + 1)%buff->size;
	buff->nbElement++;
	

		
}

int resize_buffer(buffer *buffrx, buffer *bufftx, size_t newSize){

	if(buffrx->nbElement > newSize || bufftx->nbElement > newSize){
		return -EAGAIN;	
	}
	
	bufftx->buffer = krealloc(bufftx->buffer,newSize, GFP_KERNEL);
	bufftx->size = newSize;
	buffrx->buffer = krealloc(buffrx->buffer,newSize, GFP_KERNEL);
	buffrx->size = newSize;
	
	return 0;
	
}

int get_buffer_size(buffer *buff){
	
	return (int)(buff->size);
}
