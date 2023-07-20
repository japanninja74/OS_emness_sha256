/**
 * File              : ex5_passw_attempt.c
 *                     
 * Authors           : Fabio Scatozza      <s315216@studenti.polito.it>
 *                     Isacco Delpero      <s314713@studenti.polito.it>
 *                     Leonardo Cerruti    <s317664@studenti.polito.it>
 *                     Claudio Capobianchi <s319187@studenti.polito.it>
 *                     Saman Alipour       <s307980@studenti.polito.it>
 *                     Abdul Rehman        <s315198@studenti.polito.it>
 *                     Stephano Perera     <s313080@studenti.polito.it>
 *
 * Date              : 10.07.2023
 * Last Modified Date: 11.07.2023
 *
 * Copyright (c) 2023
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


/*
This programs reads as inputs a file with a list of attempt of passwords (one
each line) and try to see if one of those returns the hash written inside a 
second input file.
*/


#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>

#define SHA256_IOC_MAGIC  'S'
#define SHA256_IOC_WCAT   _IOW(SHA256_IOC_MAGIC, 1, int)



    typedef struct {
	char *line;
    char *result;
    int valid;
    } strings;

char general_hash[65];
sem_t read_sem;
sem_t hash_sem;
	
void *passwcheck(void *exchange_data){
        long count;
        strings *exchg_data= (strings *) exchange_data;
	//Allocating the correct space for password
	char *password = (char*) malloc(strlen(exchg_data->line)+1);
	
	/*Copying the password attempt in a different memory location, because
	the original memory location will be overwritten*/
	memcpy((char*)password, (char*)exchg_data->line, strlen(exchg_data->line)+1);
	
	//After copying the value here, the main process can continue to generate next passwords
	sem_post(&read_sem);

	int fd,i;
	
	unsigned read_hash [8];
	char hash[65];
	char strtmp[9];
	
	//Opening the file descriptor of the driver
	if((fd = open("/dev/sha256", O_RDWR)) < 0) {
		perror("open()");
		exit(EXIT_FAILURE);
	}
	
	
	//Waiting to take control of the driver
	sem_wait(&hash_sem);

	//Resetting the device concatenation
	if(ioctl(fd, SHA256_IOC_WCAT, 0) == -1) {
		perror("ioctl error");
		exit(-1);
	}
	
	printf("Thread %lu took control and it is trying with password %s\n", pthread_self(), password);
	//Writing the password to the core
	if ((count = write(fd,  password, strlen(password))) < 0)
		perror("write()");
	else
		printf("write(): written %ld byte(s)\n", count);
	
	
	
	//Reading the hash from the core
	if ((count = read(fd, read_hash, 32))<0)
		perror("read()\n");
	else if(count != 32)
		perror("read(): failed reading 32 bytes\n");
	
	//Convert the hash to string format
	
	//Try this:
	hash[0] = '\0';
	for(i=0; i<8; i++){
		sprintf(strtmp, "%08x", read_hash[i]);
		strcat(hash, strtmp);
	}
	
	
	
	printf("Reading %s from the core\n", hash);
	
	
	if(strcmp(hash, general_hash)!= 0){
		//If the hash generated is not correct return 0
		printf("Tried with %s and it is not the password!\n",password);
		sem_post(&hash_sem);

		printf("Thread %lu terminates\n",pthread_self());
		
		
	}
	else{
		//else return the password
		sem_post(&hash_sem);
		printf("Password Matched\n");
		//Write in the result if a correct password is found
		exchg_data->result = (char*) malloc(strlen(password)+1);
        memcpy((char*)exchg_data->result, (char*)password, strlen(password)+1);
		printf("Thread %lu returns: %s\n",pthread_self(), exchg_data->result);
        exchg_data->valid=1;
		
	}
	if (password!=NULL)
		free(password);
	
    pthread_exit(NULL);
}



















int main(int argc , char* argv[]) {
	FILE *fp;
	FILE *hfp;
    

    
    strings exchange_data;
    exchange_data.valid=0;
	char * foundpassword = NULL;
	size_t len = 0;
	ssize_t nread;
	int i, flag;
	
	pthread_t *tid = malloc(1); /*Starting with 1 thread*/
	
        sem_init(&hash_sem, 0, 1);
        sem_init(&read_sem, 0, 0);
        
        
        if (argc !=3){
		printf("%d Error File Name: Needs to have the password file and the hash file as arguments\n", argc);
		exit(EXIT_FAILURE);
	}
	
	fp = fopen(argv[1], "r");
	if (fp == NULL) {
		printf("fopen of file %s failed\n",argv[1]);
		exit(EXIT_FAILURE);
	}
	hfp = fopen(argv[2], "r");
	if (hfp == NULL) {
		printf("fopen of file %s failed\n",argv[2]);
		exit(EXIT_FAILURE);
	}
	//Saving the hash generated by the unknown password (in ASCII)
	if (fgets(general_hash, 65, hfp) == NULL) {
		printf("Error reading the hash to crack\n");
		exit(-1);
	}
	printf("The hash of the password is %s\n",general_hash);

	
	//Each line of the given file is a password attempt
	i=1;
	while ((nread = getline(&exchange_data.line, &len, fp)) != -1) {
		//Removing the "endline" character at the end of each password
		exchange_data.line[nread-1] = '\0';

		printf("Found a password of length %zd: %s\n", nread-1, exchange_data.line);
		
		/*For each new found line a new thread must be created and consequently
		the tid vector is elongated*/
		if((tid = realloc(tid,(sizeof(pthread_t)*i)))== NULL){
			printf("Error in realloc()\n");	
			exit(-1);
		}
		
		//Creating the thread and saving its tid in the new allocated space
		pthread_create(&tid[i-1], NULL, passwcheck,(void *)&exchange_data);
		
		if(tid[i-1] < 0){
			printf("Error creating the thread number %d",i);
			exit(EXIT_FAILURE);
		}

		//Waiting for the newborn thread to read the password
		sem_wait(&read_sem);
		
		i++;
	}
	
	//Reading the threads' return values to see if a valid password was found
	
	flag=0;
	for(int j=0; j<i-1; j++){
		
		//char * tmp = NULL;
		//Reading the returned string from each thread
		pthread_join(tid[j], NULL);
		//printf("Value returned : %s\n",result);

		//If a valid password was found, the return string would be different than 0
		if(exchange_data.valid==1){
			
			//Saving the found password
			foundpassword = (char*)exchange_data.result;
			printf("Password found %s\n",foundpassword );
			
			//This indicate that a password was found
			flag=1;
			
			//Terminate all threads still running and then finish
			while(j<i-1){
				pthread_join(tid[j], NULL);
				//Their value is not important since the password was already found
				j++;
			}
		}			
	}
	
	if(flag==0){
		//Entering here if a return value different than 0 wasn't found
		printf("All password guesses were wrong\n");
	}
	else {
		//Result is used only if a password was found
       		if(exchange_data.result!=NULL)
        		free(exchange_data.result);
    	}
	
	if(exchange_data.line!=NULL)
        	free(exchange_data.line);

        sem_destroy(&hash_sem);
        sem_destroy(&read_sem);
	fclose(fp);
	fclose(hfp);
	exit(EXIT_SUCCESS);
}
