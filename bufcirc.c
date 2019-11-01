#include "bufcirc.h"

static void init_buffer(uint8_t size, buffer *buff){
	
	buff->size = size;
	buff->idIn = 0;
	buff->idOut = 0;
	buff->nbElement = 0;
	buff->buffer= kmalloc(sizeof(uint8_t)*size, GFP_KERNEL);
	spin_lock_init(&(buff->buffer_lock));
}

static void read_buffer(uint8_t* tempo, buffer *buff){
	
	*tempo = buff->buffer[buff->idOut];
	buff->idOut = (buff->idOut + 1)%buff->size;
	buff->nbElement--;	
		
}

static void write_buffer(uint8_t tempo, buffer *buff){
	
	buff->buffer[buff->idIn] = tempo;
	buff->idIn = (buff->idIn + 1)%buff->size;
	buff->nbElement++;	
		
}

static int resize_buffer(buffer *buffrx, buffer *bufftx, size_t size){

	if(buffrx->nbElement > size || bufftx->nbElement > size){
		return -EAGAIN;	
	}
	
	krealloc(bufftx->buffer,size, GFP_KERNEL);
	krealloc(buffrx->buffer,size, GFP_KERNEL);
	
	
}

static int get_buffer_size(buffer *buff){

	return (int)(buff->size);
}

