/*
 ============================================================================
 Name        : Test.c
 Author      : Samuel
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include "cmdioctl.h"




int main(void) {



	int fd =0;
	char option = 'a';
	int etat = 0;
	int read_write = 0;
	int piolte_open = 0;
	char *device;
	int drap = 0;
	char write_texte[100];
	int nb_char = 0;
	char read_texte[100];
	int ret = 0;
	int i;
	int tempo = 0;
	long retval;

	while (option != 'f') {

		if (etat == 0) {

			while (!(option == '0' || option == '1' || option == 'f')) {

				printf(" Choisir le port serie : 0 ou 1 : ");
				scanf(" %c",&option);

			}

			if (option != 'f') {
				if(option == '0'){
					device = "/dev/SerialDev0";
				}
				else
				{
					device = "/dev/SerialDev1";
				}
				option = 'a';
			}

			while (!((option == '1') || (option == '2' || option == '3') || (option == 'f'))) {

				printf(" \n Choisir mode lecture ou ecriture ou les 2 : 1 ou 2 ou 3 : ");
				scanf(" %c", &option);

			}
			if (option != 'f') {
				read_write = (int)option;
				if (option == '1') {
					drap = O_RDONLY;
				}
				else if (option == '2') {
					drap = O_WRONLY;
				}
				else {
					drap = O_RDWR;
				}
				option = 'a';
			}
			while (!(option == '0' || option == '1' || option == 'f')) {

				printf(" \n Choisir bloquant ou non bloquant : 0 ou 1 : ");
				scanf(" %c", &option);
			}
			if (option != 'f') {
				if (option == '1') {
					drap = drap | O_NONBLOCK;
				}
				else {
					drap = drap & ~O_NONBLOCK;
				}
				//printf("\n%s",drap);
				fd = open(device, drap);
				if (fd < 0) {
					printf(" erreur dans l'installation du pilote");
				}
				else {
					printf(" \n le pilote a ete installe !! \n");
					etat = 1;
				}
				option = 'a';
			}

		}

		if (etat == 1) {

			printf("\n Menu de controle choisir une option : \n");
			printf("\n r : lire des donnees ");
			printf("\n w : pour ecrire des donnees ");
			printf("\n i : pour les fonction ioctl ");
			printf("\n c : pour fermer le pilote et recommancer ");
			printf("\n f : pour fermer programme ");
			scanf(" %c", &option);

			switch (option)
			{
			case 'r':
				while (!(nb_char > 0 && nb_char < 100)) {
					printf("\n nombre de characteres a lire entre (1 et 100)  : ");
					scanf("%d", &nb_char);
				}
				ret = read(fd, read_texte, nb_char);
				printf("\n");
				for (i = 0; i < ret; ++i) {
					printf("%c", read_texte[i]);
				}
				nb_char = 0;
				break;
			case 'w':

				while (!(nb_char > 0 && nb_char < 100)) {
					printf("\n nombre de characteres a ecrire entre (1 et 100)  : ");
					scanf(" %d", &nb_char);
				}
				printf("\n entrer le texte a envoyer %d : ", nb_char);
				scanf(" %s",write_texte);
				ret = write(fd, write_texte, nb_char);
				if(ret<= 0){
					printf("\n erreur");
				}

				nb_char = 0;
					break;
				case 'i':
					printf("\n Menu IOCTL choisir une option : \n");
					printf("\n 1 : Set baudrate ");
					printf("\n 2 : Set Data Size ");
					printf("\n 3 : Set parity ");
					printf("\n 4 : Get buf Size ");
					printf("\n 5 : Set Buf Size ");
					scanf(" %c", &option);
					switch(option){
					case '1' :

						printf("\n Le baudRate  : ");
						scanf("%d", &tempo);

						retval = ioctl(fd,SET_BAUD_RATE,tempo);
						if(retval < 0){
							printf("\n erreur");
						}
						break;
					case '2' :

						printf("\n Le data size  : ");
						scanf("%d", &tempo);

						retval = ioctl(fd,SET_DATA_SIZE,tempo);
						if(retval < 0){
							printf("\n erreur");
						}
						break;
					case '3' :

						printf("\n La parity  : ");
						scanf("%d", &tempo);

						retval = ioctl(fd,SET_PARITY,tempo);
						if(retval < 0){
							printf("\n erreur");
						}
						break;
					case '4' :

						retval = ioctl(fd,GET_BUF_SIZE,tempo);
						if(retval < 0){
							printf("\n erreur");
						}
						else
						{
							printf("\n buffer size : %ld \n ",retval);
						}
						break;
					case '5' :
						printf("\n buffer size : ");
						scanf("%d", &tempo);
						printf("%d",tempo);
						retval = ioctl(fd,SET_BUF_SIZE,tempo);
						if(retval < 0){
							printf("\n erreur");
						}
						break;
					}
				    break;

				case 'c':
					close(fd);
					printf("\n pilote fermee");
					etat = 0;
					break;
				case 'f':
					break;
			}
		}

	}
	if (etat == 1) {
		close(fd);
	}



//	int i;
//	int fd = 1;
//	int nb;
//
//	char texte[] = "BonjourBonjourBonjour";
//
//	char receive[20];
//
//	fd = open("/dev/SerialDev0", O_RDWR);
//	nb = write(fd, texte, 10);
//	printf("%d \n",nb);
//
//	close(fd);
//
//	nb = read(fd,receive,26);
//	printf("%d \n",nb);
//	for(i=0;i<26;++i){
//
//		printf("%c",receive[i]);
//	}
//	printf("\n");d




	return EXIT_SUCCESS;
}
