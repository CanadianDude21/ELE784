/*
 ============================================================================
 Nom         : port_config.c
 Author      : Samuel Pesant et Mathieu Fournier-Desrochers
 Date 	     : 22-10-2019
 Description : PErmet la modification des registres du port série
 ============================================================================
 */


#include "port_config.h"
#include <asm/barrier.h>

//un variable qui maintient l'état de la transmission
int transmission_enable[2];



//*************************************************************************************
/* 	Description: La fonction initialise les registres du ports série en question


	Paramètres : structure monModule qui permet d'identifier le port

	Return     : void
 */
void init_port(monModule* module){

	uint8_t lcr_cpy;
	uint8_t fcr_cpy = 0;


	SetBaudRate(115200, module); //initialise le baudrate 115200
	SetDataSize(8, module); //initialise la taille de donnée à 8 bits
	SetParity(2, module); //initialise la parité pair

	//met 1 bit d'arrêt
	lcr_cpy = inb(module->SerialBaseAdd + LCR);
	rmb();
	lcr_cpy = lcr_cpy  & ~(LCR_STB);
	outb(lcr_cpy, module->SerialBaseAdd + LCR);
	wmb();

	//initialise le registre fcr pour controler la fifo
	fcr_cpy = fcr_cpy | FCR_FIFOEN;
	outb(fcr_cpy, (module->SerialBaseAdd + FCR));
	fcr_cpy = (fcr_cpy & ~(FCR_RCVRTRM) & ~(FCR_RCVRTRL)) | FCR_RCVRRE | FCR_FIFOEN;
	outb(fcr_cpy, (module->SerialBaseAdd + FCR));
	wmb();

	change_ETBEI(0,module); //désactive l'interruption de transmission
	change_ERBFI(0,module); //désactive l'interruption de Réception

}
//*************************************************************************************



//*************************************************************************************
/* 	Description: La fonction d'interruption qui est déclanché par la l'IRQ
				 correspondante


	Paramètres : irq_no, le numéro irq de l'interruption.
				 arg : pointeur sur notre structure de donnée.

	Return     : Irqreturn None ou handled
 */
extern irqreturn_t my_interrupt(int irq_no, void *arg){
	uint8_t ier_cpy;
	uint8_t RBR_cpy;
	uint8_t THR_cpy;
	uint8_t LSR_cpy;
	uint8_t LCR_cpy;	
	monModule *module = (monModule*)arg;

	//lire la valeur de LSR et LCR
	LSR_cpy = inb((module->SerialBaseAdd + LSR));
	rmb();
	LCR_cpy = inb((module->SerialBaseAdd + LCR));
	rmb();

	//Si l'interruption est déclanché par une donnée prêt à être lu.
	if(LSR_cpy & LSR_DR){
		//s'assurer que DLAB est 0
		if(LCR_cpy & LCR_DLAB){	
			outb((LCR_cpy & ~(LCR_DLAB)),(module->SerialBaseAdd + LCR));
			wmb();
		}
		//lecture de la donnée
		RBR_cpy = inb((module->SerialBaseAdd));
		rmb();

		//vérifie qu'elle n'est pas null (pas supposé être nécessaire,
		// mais plein d'interruption sont levé pour aucune raison en read
		if(RBR_cpy != 0){
			//capture le spin lock
			spin_lock(&(module->Rxbuf.buffer_lock));
			write_buffer(RBR_cpy,&(module->Rxbuf)); //Écrit la valeur dans le buffer de réception
			spin_unlock(&(module->Rxbuf.buffer_lock));
			wake_up_interruptible(&(module->waitRx)); //Réveil la file d'attente en réception
		}
		return IRQ_HANDLED;		
	}
	//Si l'interruption est déclanché par le transmetteur prêt à écrire.
	if(LSR_cpy & LSR_THRE){

		//s'assurer que DLAB est 0
		if(LCR_cpy & LCR_DLAB){	
			outb((LCR_cpy & ~(LCR_DLAB)),(module->SerialBaseAdd + LCR));
			wmb();
		}
		//capture le spin lock
		spin_lock(&(module->Wxbuf.buffer_lock));
		//vérifie le nombre de donnée pour éviter les erreurs
		if(module->Wxbuf.nbElement == 0){
			//plus de donnée, on désactive la transmission.
			spin_unlock(&(module->Wxbuf.buffer_lock));
			ier_cpy  = inb((module->SerialBaseAdd + IER));
			rmb();
			ier_cpy = ier_cpy & ~(IER_ETBEI);
			outb(ier_cpy, (module->SerialBaseAdd + IER));
			wmb();
			transmission_enable[module->minor] = 0;
		}
		else{

			//lire la valeur du buffer et la placé dans le registre
			read_buffer(&THR_cpy,&(module->Wxbuf));	
			spin_unlock(&(module->Wxbuf.buffer_lock));
			outb(THR_cpy,(module->SerialBaseAdd));
			wmb();
			wake_up_interruptible(&(module->waitTx)); //Réveil la file d'attente en écriture
		}

		return IRQ_HANDLED;
	}
	return IRQ_NONE;
}
//*************************************************************************************


//*************************************************************************************
/* 	Description: La fonction permet de changer le baud rate de la communication


	Paramètres : Valeur du Baudrate
				 Strcuture module pour connaître le port

	Return     : void
 */
void SetBaudRate(int baud_rate, monModule* module){

	uint8_t dl_LSB;
	uint8_t dl_MSB;
	uint16_t dl_cpy;
	//s'assure que DLAB est à 1
	uint8_t lcr_cpy = inb((module->SerialBaseAdd + LCR));
	rmb();
	lcr_cpy = lcr_cpy | LCR_DLAB;
	outb(lcr_cpy, (module->SerialBaseAdd + LCR));
	wmb();

	//Calcule de la valeur du registre en fonction du BaudRate
	dl_cpy = f_clk/ (baud_rate*16);
	//Séparation des deux registres
	dl_LSB = (uint8_t)(dl_cpy & 0x0f);
	dl_MSB = (uint8_t)(dl_cpy & 0xf0);
	//Écriture dans les registres
	outb(dl_LSB, (module->SerialBaseAdd + DL));
	wmb();
	outb(dl_MSB, (module->SerialBaseAdd + DL + 0x01));
	wmb();

	//Remet le DLAB à 0
	lcr_cpy = lcr_cpy & ~(LCR_DLAB);
	outb(lcr_cpy, (module->SerialBaseAdd + LCR));
	wmb();

}
//*************************************************************************************


//*************************************************************************************
/* 	Description: La fonction permet de changer le nombre de donnée


	Paramètres : nombre de donné
				 Strcuture module pour connaître le port

	Return     : void
 */
void SetDataSize(int size, monModule* module){

	//lecture du registre
	uint8_t LCR_cpy = inb((module->SerialBaseAdd + LCR));
	rmb();

	// Inscrire la bonne valeurs dans le registres selon la taille demandé.
	switch(size){

	case 5: // 5 bits
		LCR_cpy = LCR_cpy & ~(LCR_WLS1) & ~(LCR_WLS0);
		break;
	case 6: // 6 bits
		LCR_cpy = (LCR_cpy & ~(LCR_WLS1)) | LCR_WLS0;
		break;
	case 7: // 7 bits
		LCR_cpy = (LCR_cpy | LCR_WLS1) & ~(LCR_WLS0);
		break;
	case 8: // 8 bits
		LCR_cpy = LCR_cpy | LCR_WLS1 | LCR_WLS0;
		break;
	}
	//Écriture du dans le registre
	outb(LCR_cpy, (module->SerialBaseAdd + LCR));
	wmb();

}
//*************************************************************************************




//*************************************************************************************
/* 	Description: permet de changer la parité ou de l'enlevé


	Paramètres : parité voulu.
				 Strcuture module pour connaître le port

	Return     : void
 */
void SetParity(int parity, monModule* module){

	//lecture du registre
	uint8_t LCR_cpy = inb((module->SerialBaseAdd + LCR));
	rmb();
	// Inscrire la bonne valeurs dans le registres selon la parité.
	switch(parity){

	case 0: // Pas de parité
		LCR_cpy = LCR_cpy & ~(LCR_PEN);
		break;
	case 1: // parité impaire
		LCR_cpy = (LCR_cpy | LCR_PEN) & ~(LCR_EPS);
		break;
	case 2: // parité paire
		LCR_cpy = LCR_cpy | LCR_PEN | LCR_EPS;
		break;

	}
	//Écriture du dans le registre
	outb(LCR_cpy, (module->SerialBaseAdd + LCR));
	wmb();

}
//*************************************************************************************


//*************************************************************************************
/* 	Description: Change le déclenchement d'interruption en transmission


	Paramètres : enable ou disable
				 Strcuture module pour connaître le port

	Return     : void
 */
void change_ETBEI(int enable, monModule* module){
	//lecture du registre
	uint8_t ier_cpy = inb((module->SerialBaseAdd + IER));
	rmb();
	//changer le bit en fonction du paramètre
	if(enable){
		ier_cpy = ier_cpy | (IER_ETBEI);
		transmission_enable[module->minor] = 1;
	}
	else{
		ier_cpy = ier_cpy & ~(IER_ETBEI);
		transmission_enable[module->minor] = 0;
	}
	//écriture du registre
	outb(ier_cpy, (module->SerialBaseAdd + IER));
	wmb();

}
//*************************************************************************************


//*************************************************************************************
/* 	Description: Change le déclenchement d'interruption en réception


	Paramètres : enable ou disable
				 Strcuture module pour connaître le port

	Return     : void
 */
void change_ERBFI(int enable, monModule* module){
	//lecture du registre
	uint8_t ier_cpy = inb((module->SerialBaseAdd + IER));
	rmb();
	//changer le bit en fonction du paramètre
	if(enable){
		ier_cpy = ier_cpy | (IER_ERBFI);
	}
	else{
		ier_cpy = ier_cpy & ~(IER_ERBFI);
	}
	//écriture du registre
	outb(ier_cpy, (module->SerialBaseAdd + IER));
	wmb();

}
//*************************************************************************************
