/*
 ============================================================================
 Nom         : driver784.c
 Author      : Samuel Pesant et Mathieu Fournier-Desrochers
 Date 	     : 22-10-2019
 Description : Fonctions du pilote série
 ============================================================================
 */

#include "driver784.h"


MODULE_AUTHOR("Mathieu Fournier-Desrochers et Samuel Pesant");
MODULE_LICENSE("Dual BSD/GPL");




int serie_minor = 0;
int nbr_dvc = 2;

monModule device[2] = {{.SerialBaseAdd = SerialPort_Address_0,.SerialIRQnbr = SerialPort_IRQ_Address_0 },{.SerialBaseAdd = SerialPort_Address_1,.SerialIRQnbr = SerialPort_IRQ_Address_1}};

/* Description: Fonction d'initialisation du pilote
 *
 * Arguments  : Aucun
 *
 *
 * Return     : Code d'erreur ou 0 pour réussite
 */
static int __init pilote_serie_init (void){

	int i;
	int result;

	//initialisation des deux ports
	for(i = 0; i<nbr_dvc ; ++i){
		//init les modes d'ouverture à 0 et le spinlock d'ouverture
		device[i].wr_mod = 0;
		device[i].rd_mod = 0;
		spin_lock_init(&(device[i].acces_mod));
		//alloue la mémoire des deux buffer circulaire
		init_buffer(200,&(device[i].Wxbuf));
		init_buffer(200,&(device[i].Rxbuf));
		//init les waitqueue des buffer circulaire
		init_waitqueue_head(&(device[i].waitRx));
		init_waitqueue_head(&(device[i].waitTx));
		device[i].minor = i; //donne le minor
		
		//request le port I/O
		if(request_region(device[i].SerialBaseAdd,nbr_registres, "pilote") == NULL){
			printk(KERN_WARNING "Pilote: error with request region!");
			return -ENOTTY;
		}
		//request l'IRQ
		if(request_irq(device[i].SerialIRQnbr, &my_interrupt, IRQF_SHARED, "pilote_serie",&(device[i])) < 0){
			printk(KERN_WARNING "Pilote: error with request IRQ!");
			release_region(device[i].SerialBaseAdd,nbr_registres);
			return -ENOTTY;
		}
		//initialisation des registres du port série
		init_port(&(device[i]));


	}
	//allocation du numéro matériel
	result = alloc_chrdev_region(&device[serie_minor].dev,serie_minor,nbr_dvc,"PiloteSerie");
	if(result<0){
		printk(KERN_WARNING "Pilote: can't get major!");
		return -ENODEV;
	}
	//création du pilote
	device[0].cclass = class_create(THIS_MODULE, "PiloteSerie");
	for(i = 0; i<nbr_dvc ; ++i){
		device_create(device[serie_minor].cclass,NULL,device[serie_minor].dev +i,NULL,"SerialDev%d",i);
	}
	//installe le pilote
	cdev_init(&(device[serie_minor].mycdev), &monModule_fops);
	cdev_add(&(device[serie_minor].mycdev), device[serie_minor].dev, nbr_dvc);

	printk(KERN_WARNING "Hello world!\n");
	return 0;
}

/* Description: Fonction d'exit du pilote
 *
 *
 * Arguments  : Aucun
 *
 *
 * Return     : Aucun
 */
static void __exit pilote_serie_exit (void){
	int i;

	//élimine le pilote
	cdev_del(&(device[serie_minor].mycdev));
	for(i = 0; i<nbr_dvc ; ++i){
		device_destroy(device[serie_minor].cclass,device[serie_minor].dev+i);
	}

	class_destroy(device[serie_minor].cclass); //élimine la classe
	unregister_chrdev_region(device[serie_minor].dev,nbr_dvc); //désallocation du numéro matériel
	for(i = 0; i<nbr_dvc ; ++i){
		free_irq(device[i].SerialIRQnbr, &(device[i])); //libère l'IRQ
		release_region(device[i].SerialBaseAdd,nbr_registres); //libère le port I/O

		//désallocation des buffer circulaire
		kfree(device[i].Wxbuf.buffer);
		kfree(device[i].Rxbuf.buffer);
	}

	printk(KERN_ALERT "Goodby cruel world!\n");
}

/* Description: Fonction d'ouverture du pilote
 *
 * Arguments  :
 *
 *
 * Return     : Code d'erreur ou 0 pour réussite
 */
int pilote_serie_open(struct inode *inode,struct file *filp){
	//identifie l'unité matériel
	int minor = iminor(inode);
	filp->private_data = &device[minor];
	printk(KERN_ALERT"Pilote Opened!\n");

	//capture le spinlock pour l'ouverture
	spin_lock(&(device[minor].acces_mod));
	//Si le port est déjà ouvert en read ou en write, on n'ouvre pas une deuxième fois
	if(device[minor].wr_mod == 1){
		spin_unlock(&(device[minor].acces_mod));
		return -ENOTTY;
	}
	if(device[minor].rd_mod == 1){
		spin_unlock(&(device[minor].acces_mod));
		return -ENOTTY;
	}
	//on change le mode d'ouverture selon la demande de l'usager
	if((filp->f_flags & O_ACCMODE) == O_WRONLY){
		device[minor].wr_mod=1;
	}
	if((filp->f_flags & O_ACCMODE) == O_RDONLY){
		device[minor].rd_mod=1;
		change_ERBFI(1,&(device[minor])); //change le mode de réception
	}
	if((filp->f_flags & O_ACCMODE) ==O_RDWR){
		device[minor].wr_mod=1;
		device[minor].rd_mod=1;
		change_ERBFI(1,&(device[minor])); //change le mode de réception
	}
	//relâche le spinlock pour l'ouverture
	spin_unlock(&(device[minor].acces_mod));
	return 0;
}

/* Description: Fonction de libération du pilote
 *
 * Arguments  :
 *
 *
 * Return     : Code d'erreur ou 0 pour réussite
 */
static int pilote_serie_release(struct inode *inode,struct file *filp){
	int minor = iminor(inode);

	//capture le spinlock pour la fermeture
	spin_lock(&(device[minor].acces_mod));

	//désactive le mode d'ouverture en quoi il était ouvert
	if((filp->f_flags & O_ACCMODE) == O_WRONLY){
		device[minor].wr_mod=0;
	}
	if((filp->f_flags & O_ACCMODE) == O_RDONLY){
		device[minor].rd_mod=0;
		change_ERBFI(0,&(device[minor])); //change le mode de réception
	}
	if((filp->f_flags & O_ACCMODE) == O_RDWR){
		device[minor].wr_mod=0;
		device[minor].rd_mod=0;
		change_ERBFI(0,&(device[minor])); //change le mode de réception
	}

	//relâche le spinlock pour la fermeture
	spin_unlock(&(device[minor].acces_mod));
	printk(KERN_ALERT"Pilote Released!\n");
	return 0;
}

/* Description: Fonction de lecture du pilote
 *
 * Arguments  :
 * 				-char *buf    : pointeur du buffer circulaire à lire
 * 				-size_t count : nombre de données à lire
 *
 *
 * Return     : Code d'erreur ou le nombre de données lues
 */
ssize_t pilote_serie_read(struct file *filp, char *buf, size_t count, loff_t *f_pos)
{
	monModule *module = (monModule*)filp->private_data;
	int nb_byte_max = 8;
	uint8_t BufR[nb_byte_max];
	int nb_data_read = 0;
	int nb_a_transmettre;
	int nb_disponible;
	int i;

	//regarde le nombre d'élément dans le buffer circulaire de lecture
	spin_lock_irq(&(module->Rxbuf.buffer_lock));
	nb_disponible = module->Rxbuf.nbElement;
	spin_unlock_irq(&(module->Rxbuf.buffer_lock));

	if(filp->f_flags & O_NONBLOCK) //lecture non-bloquante
	{
		if(nb_disponible == 0)
		{ 
			return -EAGAIN; //pas de donnée dans le buffer circulaire de lecture
		}
		else
		{
			count = nb_disponible; //lire toutes les données
		}					
	}

	while(count){

		//on ne peut lire que 8 données à la fois
		nb_a_transmettre = (count > nb_byte_max)?nb_byte_max:count;

		//capture le spinlock du buffer circulaire de lecture
		spin_lock_irq(&(module->Rxbuf.buffer_lock));

		if(!(filp->f_flags & O_NONBLOCK)){ //lecture bloquante

			//lis toute les données demandées
			while(module->Rxbuf.nbElement < nb_a_transmettre )
			{
				spin_unlock_irq(&(module->Rxbuf.buffer_lock));
				//s'il manque des données on attend qu'elles arrivent
				wait_event_interruptible(module->waitRx,module->Rxbuf.nbElement >= nb_a_transmettre);
				spin_lock_irq(&(module->Rxbuf.buffer_lock));
			}
		}

		//met les données lues dans un buffer intermediaire
		for(i=0;i<nb_a_transmettre;++i){
			read_buffer(&BufR[i],&(module->Rxbuf));	
		}
		//libere le spinlock du buffer circulaire de lecture
		spin_unlock_irq(&(module->Rxbuf.buffer_lock));	

		//envoie les données à l'usager
		if(copy_to_user(&buf[nb_data_read],BufR,i)){
			printk(KERN_ALERT" Read error copy to user");
			return -EAGAIN;
		}

		//ajuste le nombre de données qui reste à lire
		nb_data_read += nb_a_transmettre;
		count -= nb_a_transmettre;
	}
	return nb_data_read;

}

/* Description: Fonction d'écriture du pilote
 *
 * Arguments  :
 * 				-char *buf    : pointeur du buffer circulaire où écrire
 * 				-size_t count : nombre de données à écrire
 *
 *
 * Return     : Code d'erreur ou le nombre de données écrites
 */
static ssize_t pilote_serie_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos) {

	monModule *module = (monModule*)filp->private_data;
	int nb_byte_max = 8;
	int nb_data_write = 0;
	uint8_t BufW[nb_byte_max];
	int nb_a_transmettre;
	int nb_disponible;
	int i;

	//regarde le nombre d'élément dans le buffer circulaire d'écriture
	spin_lock_irq(&(module->Wxbuf.buffer_lock));
	nb_disponible = module->Wxbuf.size - module->Wxbuf.nbElement;
	spin_unlock_irq(&(module->Wxbuf.buffer_lock));

	if(filp->f_flags & O_NONBLOCK) //écriture non-bloquante
	{
		if(nb_disponible == 0)
		{ 
			return -EAGAIN; //pas de place dans le buffer d'écriture
		}
		else
		{
			count = nb_disponible; //on écrit dans l'espace dispo
		}					
	}

	while(count){

		//on écrit seulement 8 données à la fois
		nb_a_transmettre = (count > nb_byte_max)?nb_byte_max:count;

		//on copie les données à écrire dans un buffer intermediaire
		if(copy_from_user(BufW,&(buf[nb_data_write]),nb_a_transmettre)){
			printk(KERN_ALERT"Write error copy from user");
			return -EAGAIN;
		}

		//capture le spinlock du buffer circulaire d'écriture
		spin_lock_irq(&(module->Wxbuf.buffer_lock));

		if(!(filp->f_flags & O_NONBLOCK)){ //écriture bloquante

			while((module->Wxbuf.size - module->Wxbuf.nbElement) < nb_a_transmettre )
			{
				spin_unlock_irq(&(module->Wxbuf.buffer_lock));
				wait_event_interruptible(module->waitTx,(module->Wxbuf.size - module->Wxbuf.nbElement) >= nb_a_transmettre);
				spin_lock_irq(&(module->Wxbuf.buffer_lock));
			}
		}

		if(module->Wxbuf.nbElement == 0){ //change le mode de transmission
			change_ETBEI(1, module);	
		}
		for(i=0;i<nb_a_transmettre;++i){ //écrit les données dans le buffer circulaire d'écriture
			write_buffer(BufW[i],&(module->Wxbuf));	
		}

		//libère le spinlock du buffer circulaire d'écriture
		spin_unlock_irq(&(module->Wxbuf.buffer_lock));		

		//ajuste le nombre de données qui reste à écrire
		nb_data_write += nb_a_transmettre;
		count -= nb_a_transmettre;
	}


	return nb_data_write;

}

/* Description: Fonction de IOCTL du pilote
 *
 * Arguments  :
 * 				-unsigned int cmd  : numéro de commande
 * 				-unsigned long arg : arguments venant avec la commande
 *
 *
 * Return     : Code d'erreur ou résultat de la commande demandée
 */
long pilote_serie_ioctl(struct file *filp, unsigned int cmd, unsigned long arg){

	monModule *module = (monModule*)filp->private_data;
	long retval = 0;

	//vérifie si c'est une commande du pilote
	if(_IOC_TYPE(cmd) != MAGIC_NUMBER_PILOTE){
		printk(KERN_WARNING"Error magic number");
		return -ENOTTY;
	}
	//vérifie si c'est une commande qui existe
	if(_IOC_NR(cmd) > MAX_NBR_IOCCMD ){
		printk(KERN_WARNING"command does not exist: %d",_IOC_NR(cmd));
		return -ENOTTY;
	}

	switch(cmd){

	case SET_BAUD_RATE:
		if((int)arg > 115200 || (int)arg < 50){
			retval = -ENOTTY;		
		}
		else{
			SetBaudRate((int)arg, module);
		}	
		break;
	case SET_DATA_SIZE:
		if((int)arg > 8 || (int)arg < 5){
			retval = -ENOTTY;		
		}
		else{
			SetDataSize((int)arg, module);
		}
		break;
	case SET_PARITY:
		if((int)arg > 2 || (int)arg < 0){
			retval = -ENOTTY;		
		}
		else{
			SetParity((int)arg, module);
		}
		break;
	case GET_BUF_SIZE:
		retval = (long)get_buffer_size(&(module->Wxbuf));
		break;
	case SET_BUF_SIZE:
		if(!(capable(CAP_SYS_ADMIN))){ //vérifie si l'usager a les droits
			printk(KERN_WARNING"Error: User does not have permission\n");
			return -EPERM;
		}
		spin_lock_irq(&(module->Wxbuf.buffer_lock));
		retval = resize_buffer(&(module->Rxbuf),&(module->Wxbuf),(int)arg);
		spin_unlock_irq(&(module->Wxbuf.buffer_lock));
		break;
	default:
		return -EAGAIN;
	}
	return retval;
}
