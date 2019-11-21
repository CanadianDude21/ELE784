/*
 ============================================================================
 Nom         : bufcirc.c
 Author      : Samuel Pesant et Mathieu Fournier-Desrochers
 Date 	     : 22-10-2019
 Description : Toute les fonction utilie à l'utilisation d'un buffer circulaire
 ============================================================================
 */

#include "bufcirc.h"

void init_buffer(uint8_t size, buffer *buff){
	
	buff->size = size;
	buff->idIn = 0;
	buff->idOut = 0;
	buff->nbElement = 0;
	buff->buffer= kmalloc(sizeof(uint8_t)*size, GFP_ATOMIC);
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

	uint8_t *tempBuffrx;
	uint8_t *tempBufftx;
	int i = 0;
	int j = bufftx->idOut;
	
	tempBufftx = kmalloc(sizeof(uint8_t)*newSize, GFP_ATOMIC);
	tempBuffrx = kmalloc(sizeof(uint8_t)*newSize, GFP_ATOMIC);
	
	if(buffrx->nbElement > newSize || bufftx->nbElement > newSize){
		return -EAGAIN;	
	}
	while(j != bufftx->idIn){
		tempBufftx[i] = bufftx->buffer[j];
		j = (j+1)%bufftx->size;
		i++;
	}
	kfree(bufftx->buffer);
	bufftx->buffer = tempBufftx;
	bufftx->idOut = 0;
	bufftx->idIn = i;
	bufftx->size = newSize;
	
	i = 0;
	
	j = buffrx->idOut;
	while(j != buffrx->idIn){
		tempBuffrx[i] = buffrx->buffer[j];
		j = (j+1)%buffrx->size;
		i++;
	}
	kfree(buffrx->buffer);
	buffrx->buffer = tempBuffrx;
	buffrx->idOut = 0;
	buffrx->idIn = i;
	buffrx->size = newSize;
	
	return 0;
	
}

int get_buffer_size(buffer *buff){

	return (int)(buff->size);
}
