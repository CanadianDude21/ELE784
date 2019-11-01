#include <linux/ioctl.h>

#define MAGIC_NUMBER_PILOTE 'Z'

#define SET_BAUD_RATE _IOW(MAGIC_NUMBER_PILOTE,0,int) //50 a 115200 baudrate

#define SET_DATA_SIZE _IOW(MAGIC_NUMBER_PILOTE,1,int) //5 a 8 bits

#define SET_PARITY _IOW(MAGIC_NUMBER_PILOTE,2,int) //0,1,2 pas parite,impaire,paire

#define GET_BUF_SIZE _IOR(MAGIC_NUMBER_PILOTE,3,int) //taille du tampon

#define SET_BUF_SIZE _IOW(MAGIC_NUMBER_PILOTE,4,int)

#define MAX_NBR_IOCCMD 4