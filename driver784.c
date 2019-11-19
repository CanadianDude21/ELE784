#include "driver784.h"


MODULE_AUTHOR("Mathieu Fournier-Desrochers ell");
MODULE_LICENSE("Dual BSD/GPL");




int serie_minor = 0;
int nbr_dvc = 2;
monModule device[2];
//monModule device[nbr_dvc] = {{.SerialBaseAdd = SerialPort_Address_0,.SerialIRQnbr = SerialPort_IRQ_Address_0 },{.SerialBaseAdd = SerialPort_Address_1,.SerialIRQnbr = SerialPort_IRQ_Address_1}};


static int __init pilote_serie_init (void){
	
	int i;
	int result;

	serie_major = MAJOR(device.dev);
	device.wr_mod = 0;
	device.rd_mod = 0;
	init_buffer(200,&(device.Wxbuf));
	init_buffer(200,&(device.Rxbuf));
	init_waitqueue_head(&(device.waitRx));
	init_waitqueue_head(&(device.waitTx));
	device.SerialBaseAdd = SerialPort_Address_0;
	device.SerialIRQnbr = SerialPort_IRQ_Address_0;

	if(request_region(device.SerialBaseAdd,nbr_registres, "pilote") == NULL){
		printk(KERN_WARNING "Pilote: error with request region!");
		return -ENODEV;
	}
	
	device[0].SerialBaseAdd = SerialPort_Address_0;
	device[1].SerialBaseAdd = SerialPort_Address_1;
	device[0].SerialIRQnbr = SerialPort_IRQ_Address_0; 
	device[1].SerialIRQnbr = SerialPort_IRQ_Address_1;
	
	for(i = 0; i<nbr_dvc ; ++i){
		device[i].wr_mod = 0;
		device[i].rd_mod = 0;
		spin_lock_init(&(device[i].acces_mod));
		init_buffer(200,&(device[i].Wxbuf));
		init_buffer(200,&(device[i].Rxbuf));
		init_waitqueue_head(&(device[i].waitRx));
		init_waitqueue_head(&(device[i].waitTx));
		
		
		if(request_region(device[i].SerialBaseAdd,nbr_registres, "pilote") == NULL){
			printk(KERN_WARNING "Pilote: error with request region!");
			return -ENOTTY;
		}
		
		if(request_irq(device[i].SerialIRQnbr, &my_interrupt, IRQF_SHARED, "pilote_serie",&(device[i])) < 0){
			printk(KERN_WARNING "Pilote: error with request IRQ!");
			release_region(device[i].SerialBaseAdd,nbr_registres);
			return -ENOTTY;
		}
		init_port(&(device[i]));
		

	}
	result = alloc_chrdev_region(&device[0].dev,serie_minor,nbr_dvc,"PiloteSerie");
	if(result<0){
		printk(KERN_WARNING "Pilote: can't get major!");
		return -ENODEV;
	}

	device[0].cclass = class_create(THIS_MODULE, "PiloteSerie");
	for(i = 0; i<nbr_dvc ; ++i){
		device_create(device[0].cclass,NULL,device[0].dev +i,NULL,"SerialDev%d",i);
	}
	cdev_init(&(device[0].mycdev), &monModule_fops);
	cdev_add(&(device[0].mycdev), device[0].dev, nbr_dvc);

	printk(KERN_WARNING "Hello world!\n");
	return 0;
}

static void __exit pilote_serie_exit (void){
	int i;

	cdev_del(&(device[0].mycdev));
	for(i = 0; i<nbr_dvc ; ++i){
		device_destroy(device[0].cclass,device[0].dev+i);
	}
	
	class_destroy(device[0].cclass);
	unregister_chrdev_region(device[0].dev,nbr_dvc);
	for(i = 0; i<nbr_dvc ; ++i){
		free_irq(device[i].SerialIRQnbr, &(device[i]));
		release_region(device[i].SerialBaseAdd,nbr_registres);
		
		kfree(device[i].Wxbuf.buffer);
		kfree(device[i].Rxbuf.buffer);
	}
	
	printk(KERN_ALERT "Goodby cruel world!\n");
}

int pilote_serie_open(struct inode *inode,struct file *filp){
	int minor = iminor(inode);
	filp->private_data = &device[minor];
	printk(KERN_ALERT"Pilote Opened!\n");
	spin_lock(&(device[minor].acces_mod));
	if(device[minor].wr_mod == 1){
		spin_unlock(&(device[minor].acces_mod));
		return -ENOTTY;
	}
	if(device[minor].rd_mod == 1){
		spin_unlock(&(device[minor].acces_mod));
		return -ENOTTY;
	}
	if((filp->f_flags & O_ACCMODE) == O_WRONLY){
		device[minor].wr_mod=1;
	}
	if((filp->f_flags & O_ACCMODE) == O_RDONLY){
		device[minor].rd_mod=1;
	}
	if((filp->f_flags & O_ACCMODE) ==O_RDWR){
		device[minor].wr_mod=1;
		device[minor].rd_mod=1;
	}
	spin_unlock(&(device[minor].acces_mod));
	return 0;
}

static int pilote_serie_release(struct inode *inode,struct file *filp){
	int minor = iminor(inode);

	spin_lock(&(device[minor].acces_mod));
	if((filp->f_flags & O_ACCMODE) == O_WRONLY){
		device[minor].wr_mod=0;
	}
	if((filp->f_flags & O_ACCMODE) == O_RDONLY){
		device[minor].rd_mod=0;
	}
	if((filp->f_flags & O_ACCMODE) ==O_RDWR){
		device[minor].wr_mod=0;
		device[minor].rd_mod=0;
	}
	spin_unlock(&(device[minor].acces_mod));
	printk(KERN_ALERT"Pilote Released!\n");
	return 0;
}

ssize_t pilote_serie_read(struct file *filp, char *buf, size_t count, loff_t *f_pos)
{
	monModule *module = (monModule*)filp->private_data;
	int nb_byte_max = 8;
	uint8_t BufR[nb_byte_max];
	int nb_data_read = 0;
	int nb_a_transmettre;
    	int nb_disponible;
	int i;

	spin_lock_irq(&(module->Rxbuf.buffer_lock));
	nb_disponible = module->Rxbuf.nbElement;
	spin_unlock_irq(&(module->Rxbuf.buffer_lock));
	
	if(filp->f_flags & O_NONBLOCK)
	{
		if(nb_disponible == 0)
		{ 
			return -EAGAIN;
		}
		else
		{
			count = nb_disponible;
		}					
	}
	
	while(count){
		
		nb_a_transmettre = (count > nb_byte_max)?nb_byte_max:count;

		spin_lock_irq(&(module->Rxbuf.buffer_lock));
		
		if(!(filp->f_flags & O_NONBLOCK)){

	  		while(module->Rxbuf.nbElement < nb_a_transmettre )
			{
				spin_unlock_irq(&(module->Rxbuf.buffer_lock));
				wait_event_interruptible(module->waitRx,module->Rxbuf.nbElement >= nb_a_transmettre);
				spin_lock_irq(&(module->Rxbuf.buffer_lock));
			}
		}
		
		
		for(i=0;i<nb_a_transmettre;++i){
			read_buffer(&BufR[i],&(module->Rxbuf));	
		}
		spin_unlock(&(module->Rxbuf.buffer_lock));	

		if(copy_to_user(&buf[nb_data_read],BufR,i)){
			printk(KERN_ALERT" Read error copy to user");
			return -EAGAIN;
		}
		nb_data_read += nb_a_transmettre;
		count -= nb_a_transmettre;
	}
	return nb_data_read;
 
}

static ssize_t pilote_serie_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos) {

    	monModule *module = (monModule*)filp->private_data;
	int nb_byte_max = 8;
	int nb_data_write = 0;
	uint8_t BufW[nb_byte_max];
	int nb_a_transmettre;
    	int nb_disponible;
	int i;

	spin_lock_irq(&(module->Wxbuf.buffer_lock));
	nb_disponible = module->Wxbuf.size - module->Wxbuf.nbElement;
	spin_unlock_irq(&(module->Wxbuf.buffer_lock));
	
	if(filp->f_flags & O_NONBLOCK)
	{
		if(nb_disponible == 0)
		{ 
			return -EAGAIN;
		}
		else
		{
			count = nb_disponible;
		}					
	}
	
	while(count){
		
		nb_a_transmettre = (count > nb_byte_max)?nb_byte_max:count;

		if(copy_from_user(BufW,&(buf[nb_data_write]),nb_a_transmettre)){
			printk(KERN_ALERT"Write error copy from user");
			return -EAGAIN;
		}

	
		spin_lock_irq(&(module->Wxbuf.buffer_lock));
		
		if(!(filp->f_flags & O_NONBLOCK)){
	  		while((module->Wxbuf.size - module->Wxbuf.nbElement) < nb_a_transmettre )
			{
				spin_unlock_irq(&(module->Wxbuf.buffer_lock));
				wait_event_interruptible(module->waitTx,(module->Wxbuf.size - module->Wxbuf.nbElement) >= nb_a_transmettre);
				spin_lock_irq(&(module->Wxbuf.buffer_lock));
			}
		}
		
		if(module->Wxbuf.nbElement == 0){
			change_ETBEI(1, module);	
		}
		for(i=0;i<nb_a_transmettre;++i){
			write_buffer(BufW[i],&(module->Wxbuf));	
		}
		
		spin_unlock(&(module->Wxbuf.buffer_lock));		
		nb_data_write += nb_a_transmettre;
		count -= nb_a_transmettre;
	}
	
	
	return nb_data_write;

}

long pilote_serie_ioctl(struct file *filp, unsigned int cmd, unsigned long arg){
	
	monModule *module = (monModule*)filp->private_data;
	long retval = 0;
	if(_IOC_TYPE(cmd) != MAGIC_NUMBER_PILOTE){
		printk(KERN_WARNING"Error magic number");
		return -ENOTTY;
	}
	if(_IOC_NR(cmd) > MAX_NBR_IOCCMD ){
		printk(KERN_WARNING"command does not exist: %d",_IOC_NR(cmd));
		return -ENOTTY;
	}
	
	switch(cmd){
		
	case SET_BAUD_RATE:
		SetBaudRate((int)arg, module);
		break;
	case SET_DATA_SIZE:
		SetDataSize((int)arg, module);
		break;
	case SET_PARITY:
		SetParity((int)arg, module);
		break;
	case GET_BUF_SIZE:
		retval = get_buffer_size(&(module->Wxbuf));
		break;
	case SET_BUF_SIZE:
		retval = resize_buffer(&(module->Rxbuf),&(module->Wxbuf),arg);
		break;
	default:
		return -EAGAIN;
	}
	return retval;
}
