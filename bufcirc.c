#include "bufcirc.h"

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

