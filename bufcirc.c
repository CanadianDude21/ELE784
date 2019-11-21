/*
 ============================================================================
 Nom         : bufcirc.c
 Author      : Samuel Pesant et Mathieu Fournier-Desrochers
 Date 	     : 22-10-2019
 Description : Toute les fonction utile à l'utilisation d'un buffer circulaire
 ============================================================================
 */

#include "bufcirc.h"


//*************************************************************************************
/* 	Description: Initialisation d'un buffer circulaire


	Paramètres : la grandeur du buffer
				 pointeur sur la structure

	Return     : void
 */
void init_buffer(uint8_t size, buffer *buff){

	//initialisation de chaque élément de la structure
	buff->size = size;
	buff->idIn = 0;
	buff->idOut = 0;
	buff->nbElement = 0;
	buff->buffer= kmalloc(sizeof(uint8_t)*size, GFP_ATOMIC); //espace mémoir dynamique
	spin_lock_init(&(buff->buffer_lock)); //création du spin lock
}
//*************************************************************************************


//*************************************************************************************
/* 	Description: Permet de lire une valeur du buffer


	Paramètres : pointeur ou placer la donné
				 pointeur sur le buffer

	Return     : void
 */
void read_buffer(uint8_t* tempo, buffer *buff){
	//prendre la valeur et ajuster les indices et la nombre d'élément
	*tempo = buff->buffer[buff->idOut];
	buff->idOut = (buff->idOut + 1)%buff->size;
	buff->nbElement--;	

}
//*************************************************************************************



//*************************************************************************************
/* 	Description: Permet de d'écrire une valeur du buffer


	Paramètres : valeur à ajouter
				 pointeur sur le buffer

	Return     : void
 */
void write_buffer(uint8_t tempo, buffer *buff){
	//placer la valeur et ajuster les indices et la nombre d'élément
	buff->buffer[buff->idIn] = tempo;
	buff->idIn = (buff->idIn + 1)%buff->size;
	buff->nbElement++;	

}
//*************************************************************************************



//*************************************************************************************
/* 	Description: Permet de de redimensionner la grandeur des buffer


	Paramètres : buffer de réception
	 	 	 	 buffer de transmission
				 Nouvelle taille

	Return     : 0 quand réussi et -EAGAIN pour erreur
 */
int resize_buffer(buffer *buffrx, buffer *bufftx, size_t newSize){

	uint8_t *tempBuffrx;
	uint8_t *tempBufftx;
	int i = 0;
	int j = bufftx->idOut;

	//ici on devrait prendre le Spinlock mais nous avons jamais été en mesure
	//de réussir la fonction avec le spin lock (crash)

	//vérifie que le nombre d'élement e4st inférieur à la nouvelle taille
	if(buffrx->nbElement > newSize || bufftx->nbElement > newSize){
		return -EAGAIN;
	}

	//création des deux nouveau buffers
	tempBufftx = kmalloc(sizeof(uint8_t)*newSize, GFP_ATOMIC);
	tempBuffrx = kmalloc(sizeof(uint8_t)*newSize, GFP_ATOMIC);

	//copie des valeur du buffer dans le nouveau
	while(j != bufftx->idIn){
		tempBufftx[i] = bufftx->buffer[j];
		j = (j+1)%bufftx->size;
		i++;
	}
	//libère l'espace du premier buffer et ajust les nouveaux indices
	kfree(bufftx->buffer);
	bufftx->buffer = tempBufftx;
	bufftx->idOut = 0;
	bufftx->idIn = i;
	bufftx->size = newSize;

	i = 0;
	j = buffrx->idOut;

	//copie des valeur du buffer dans le nouveau
	while(j != buffrx->idIn){
		tempBuffrx[i] = buffrx->buffer[j];
		j = (j+1)%buffrx->size;
		i++;
	}
	//libère l'espace du premier buffer et ajust les nouveaux indices
	kfree(buffrx->buffer);
	buffrx->buffer = tempBuffrx;
	buffrx->idOut = 0;
	buffrx->idIn = i;
	buffrx->size = newSize;

	return 0;

}
//*************************************************************************************

//*************************************************************************************
/* 	Description: Retourne la grandeur du buffer


	Paramètres : le buffer


	Return     : la grandeur
 */
int get_buffer_size(buffer *buff){

	return (int)(buff->size);
}
//*************************************************************************************
