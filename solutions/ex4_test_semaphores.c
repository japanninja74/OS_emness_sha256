/**
 * File              : ex4_test_semaphores.c
 *                     
 * Authors           : Fabio Scatozza      <s315216@studenti.polito.it>
 *                     Isacco Delpero      <s314713@studenti.polito.it>
 *                     Leonardo Cerruti    <s317664@studenti.polito.it>
 *                     Claudio Capobianchi <s319187@studenti.polito.it>
 *                     Saman Alipour       <s307980@studenti.polito.it>
 *                     Abdul Rehman        <s315198@studenti.polito.it>
 *                     Stephano Perera     <s313080@studenti.polito.it>
 *
 * Date              : 16.07.2023
 * Last Modified Date: 18.07.2023
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


/* This file requests a number of files as input (specify the number
of files first and then put all the names) and via hash calculation
checks if those files have equal content.*/

#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<pthread.h>
#include<semaphore.h>
#include<string.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#define SHA256_IOC_MAGIC  'S'
#define SHA256_IOC_WCAT   _IOW(SHA256_IOC_MAGIC, 1, int)


#define L 100

sem_t sem;

void print_hash(unsigned * hash);
int compare_hash(unsigned * hash1, unsigned * hash2);

void * hash_thread (void * arg){
	char * f_name;
	f_name = (char*)arg;
	long count;
	char str[L];
	unsigned * read_hash = malloc(sizeof(unsigned)*8);
	unsigned temp[8];
	int fd;
	//Opening the file descriptor of the driver
	if((fd = open("/dev/sha256", O_RDWR)) < 0) {
		perror("open()");
		exit(EXIT_FAILURE);
	}
	
	FILE* tfp;
	//Opening the file to calculate its hash
	tfp = fopen(f_name, "r");
	if(tfp==NULL){
		printf("Error opening the file\n");
		exit(-1);
	}
	

	//Now the critical section starts
	sem_wait(&sem);

	if(ioctl(fd, SHA256_IOC_WCAT, 0) == -1) {
		perror("ioctl error");
		exit(-1);
	}
	
	printf("Thread %lu took control and reads file %s\n", pthread_self(), f_name);
	
	printf("Reset the driver by writing 0 without concatenations\n");
	if ((count = write(fd, NULL, 0)) < 0)
		perror("write()");
	else
		printf("write(): written %ld byte(s)\n", count);
	
	/*Since the lenght of the file is unknown, the method used is to write 100 bytes
        each time until the file is finished*/
	printf("Enabling concatenations...\n");
	if(ioctl(fd, SHA256_IOC_WCAT, 1) < 0) {
		perror("ioctl()");
		close(fd);
		exit(EXIT_FAILURE);
	}

	while(fgets(str, L, tfp)!=NULL){
		printf("Writing %s to the core\n",str);

		//Actually writing to the driver
		if ((count = write(fd, str, strlen(str)) < 0))
			perror("write()");
		else if (count != strlen(str))
			fprintf(stderr, "write(): failed writing %d bytes\n", strlen(str));
	}

	//This action of terminating the concatenation is needed to send the "last" signal
	printf("terminate concatenation...\n");
	if ((count = write(fd, NULL, 0)) < 0)
		perror("write()");
	else
		printf("write(): written %ld byte(s) (issue last)\n", count);
	
	
	
	//Reading the hash of the file
	if ((count = read(fd, read_hash, 32)) < 0)
		perror("read()");
	else if (count != 32)
		fprintf(stderr, "read(): failed reading 32 bytes\n");
	

	sem_post(&sem);
	
	close(fd);
	fclose(tfp);

	//Returning the read hash
	return (void*) read_hash;
}






int main(int argc, char * argv[]){
	int N = argc-1; //Number of input files
	pthread_t tid[N];
	int i;
	int flag =0;
	unsigned * hash = NULL;
	unsigned * cmp_hash = NULL;

	if(argc <2){
		printf("You need to specify the names of the files to compare as input arguments");
		return -1;
	}
	if(argc == 2){
		printf("\nMust check 2 or more files");
		return -1;
	}
	
	sem_init (&sem, 0, 1);
		
	for(i=0; i<N ; i++){
		pthread_create (&tid[i], NULL, hash_thread, argv[i+1]);
		//Create one hash thread for each file
	}
	
	//Taking the first returned hash as a reference for the comparison
	pthread_join(tid[0], (void**)&cmp_hash);
	printf("Hash of the reference file %s is: ",argv[1]);
	print_hash(cmp_hash);
	
	for(int i=1; i<N; i++){
		pthread_join(tid[i], (void**)&hash);
		printf("Hash of the file %s is: ",argv[i+1]);
		print_hash(hash);
		//Waiting and comparing all hashes
		if(compare_hash(cmp_hash,hash)){
			printf("Found a difference in the file %s\n", argv[i+1]);
			//Saving that a file is different
			flag =1;
			/*Here the for loop is not  terminating for 2 reason:
				1)Need to terminate all threads
				2)In this way in the output all different files are
				printed (if there are more than one)*/
		}
	}
	
	if(flag == 1){
		printf("The files are not equal\n");
	}else{
		printf("Found the same hash: the files are equal\n");
	}
	
	if(hash != NULL){
		free(hash);
	}
	if(cmp_hash != NULL){
		free(cmp_hash);
	}
	
	sem_destroy(&sem);
}

void print_hash(unsigned * hash){
	//reading each character in hexadecimal
	for (int i = 0; i < 8; i++)
		printf("%08X", (unsigned) hash[i]); 
	
	printf("\n");
}


int compare_hash(unsigned * hash1, unsigned * hash2){
	//Comparing all characters one by one
	for(int i = 0;  i<8; i++){
		if((unsigned) hash1[i] != (unsigned) hash2[i]){
			//If 2 characters with the same index are different return immediately
			return 1;
		}
	}
	return 0;
}
	
