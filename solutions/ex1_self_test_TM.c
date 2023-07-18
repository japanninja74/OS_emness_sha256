/**
 * File              : ex1_self_test_TM.c
 *                     
 * Authors           : Fabio Scatozza      <s315216@studenti.polito.it>
 *                     Isacco Delpero      <s314713@studenti.polito.it>
 *                     Leonardo Cerruti    <s317664@studenti.polito.it>
 *                     Claudio Capobianchi <s319187@studenti.polito.it>
 *                     Saman Alipour       <s307980@studenti.polito.it>
 *                     Abdul Rehman        <s315198@studenti.polito.it>
 *                     Stephano Perera     <s313080@studenti.polito.it>
 *
 * Date              : 14.07.2023
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
 
 /*Description about this Self_test programs*/
 
 /*
 *Main Components and Functionality:

 *Test Data: The code defines four sets of test data, each consisting of an input string and the corresponding expected SHA256 hash.

 *Main Function: The main() function serves as the entry point of the program. It initializes a mutex lock, opens the SHA256 device driver file, 
 *and creates three threads for three different tests. Each thread is assigned a specific test function to execute.

 *Test Functions:
 *a. concatenation(): This test function focuses on testing concatenations. It writes and reads data to/from the device driver, simulating concatenation of 
 *different messages and verifying the resulting SHA256 hashes.
 *b. With_Seek(): This function tests writing without concatenation and involves forward seek operations. It writes zero-bytes and "abc" separately, performs 
 *seek operations, and verifies the resulting hashes.
 *c. Without_Seek(): This function tests writing without concatenation and without seek. It simply writes the string "abc" to the device driver and verifies the hash.

 *Synchronization: Mutex locks (pthread_mutex_t) are used to ensure proper synchronization between threads. Each test function acquires the mutex lock before 
 *execution and releases it upon completion to prevent race conditions.

 *SHA256 Device Driver: The code interacts with the SHA256 driver file through file operations (open(), write(), and read()), as well as an ioctl call (ioctl()) 
 *to enable concatenation.

 *Overall, the program performs self-tests on the SHA256 hashing device driver, demonstrating the ability to handle concatenation and different write scenarios. It prints 
 *the test results, including hash values and matching status, to verify the correctness of the device driver implementation.
 
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <pthread.h>

#define SHA256_IOC_MAGIC  'S'
#define SHA256_IOC_WCAT   _IOW(SHA256_IOC_MAGIC, 1, int)

/* test data */

static const char str[] = "abc";

//Precomuted hash value of "abc"
static const unsigned hash_str[] = {
  0xba7816bf,
  0x8f01cfea,
  0x414140de,
  0x5dae2223,
  0xb00361a3,
  0x96177a9c,
  0xb410ff61,
  0xf20015ad };
  
//Precomuted hash value of 1000000 zeros
static const unsigned hash_1000000zero[] = {
  0xd29751f2,
  0x649b32ff,
  0x572b5e0a,
  0x9f541ea6,
  0x60a50f94,
  0xff0beedf,
  0xb0b692b9,
  0x24cc8025};
//Precomuted hash value of 10 zeros + abc
static const unsigned hash_10zero_abc[] = {
  0x613f318b,
  0xec31b432,
  0x76e3355f,
  0x0e91cd28,
  0xe0547b03,
  0x0a3daf0d,
  0xf9637f54,
  0xe5ab6f01};
//Precomuted hash value of "bc"
static const unsigned hash_abc_10zero_abc_10zero[] = {
  0xbaafff5f,
  0xd15d4354,
  0x3346ee97,
  0x5748fb44,
  0x8f721729,
  0x2cc83464,
  0x8ee986be,
  0xb851adce};

  // Test function to print the generated hash value
void printHash(const unsigned hash[8], int n);

// Function to validate the generated hash against the expected hash value
bool validateHash(const unsigned hash[8], const unsigned hash_expected[8]);

// Function to read data from the file descriptor and compare the generated hash
void readFancy(int fd, const unsigned hash_expected[8], int n);

// Test function to perform concatenation and calculate SHA256 hash
void concatenation(); 

// Test function to write data with forward seek and calculate SHA256 hash
void With_Seek();

// Test function to write data without seek and calculate SHA256 hash
void Without_Seek();

// Test function executed by each thread
void *test(void* arg);

// File descriptor for the device file
int fd;

// Position and count variables for file operations
long pos, count;

// Mutex lock to synchronize access to shared resources
pthread_mutex_t mutex;

int error;


void *test(void* arg) {
        
	pthread_mutex_lock(&mutex);

	// Identify the type of test and execute the corresponding function
	if (strcmp(arg, "C") == 0) {
		concatenation();
	}
	else if (strcmp(arg, "B") == 0) {
		With_Seek();
	}
	else if (strcmp(arg, "A") == 0) {
		Without_Seek();
	}
	
	pthread_mutex_unlock(&mutex);
	pthread_exit(NULL);
	
	
}

int main(int argc, char *argv[]) {

	pthread_t tid; //creating Pthreads variable
	char* arg[3] = {"A", "B", "C"};
	printf("################################\n");
  	printf("# Beginning self test with MultiThreads and Mutexlock...\n");
  	printf("################################\n");
	
	/* 
   	* testing open 
   	*/
  	if((fd = open("dev/sha256", O_RDWR)) < 0) {
    		perror("open()");
    		exit(EXIT_FAILURE);
 	 }
 	 
  	printf("open() done: blocking, read-write access\n");

	
	pthread_mutex_init(&mutex,NULL); // initialization of Mutex_lock
	
	printf("Thread Creation\n");
	
	// Create threads to perform the different tests
	for (int j = 0; j < 3; j++) {
		error = pthread_create(&tid, NULL, test, arg[j]);
		if (error != 0) {
			perror("Error creating the thread\n");
		}
	}
	
	// Wait for all threads to finish
	for (int i = 1; i < 4; i++) {
		pthread_join(tid, NULL);
	}
	
	printf("All threads are done their work!\n");
	pthread_mutex_destroy(&mutex);
	close(fd);
	exit(EXIT_SUCCESS);
}
	
	
// Test function which is used to check with contenation and with forward seek
void concatenation() {
    /* testing concatenations
   */
  	printf("\n################################\n");
  	printf("Testing concatenations...\n");
  
  	printf("Reset by writing 0 without concatenations\n");
  	if ((count = write(fd, NULL, 0)) < 0)
   		 perror("write()");
  	else
    		printf("write(): written %ld byte(s)\n", count);

  	printf("Enabling concatenations...\n");
  	if(ioctl(fd, SHA256_IOC_WCAT, 1) < 0) { // Enabling Concatination  by sending one to ioctl
    		perror("ioctl()");
    		close(fd);
    		exit(EXIT_FAILURE);
  	}

  	puts("");
 	printf("enqueue: %s\n", str);
  	if ((count = write(fd, str, 3)) < 0) // Write the message "str" to the source file 
   		 perror("write()");
 	else if (count != 3)
   		fprintf(stderr, "write(): failed writing 3 bytes\n"); 

  	if ((pos = lseek(fd, 0, SEEK_CUR)) < 0)  // returns the current position of the file
    		perror("lseek()");
  	else
    		printf("lseek(): cur_fpos = %ld\n", pos);
 
  	puts("");
  	printf("Reset by rewind\n");
  	if ((pos = lseek(fd, 0, SEEK_SET)) < 0) // set the current position of the file to zero
    		perror("lseek()");
  	else
    		printf("lseek(): cur_fpos = %ld\n", pos);

  /* actual concatenation: abc, 10 zero-bytes, abc, 10 zero-bytes*/
  	puts("");
  	printf("enqueue: \"abc\"\n");
  	if ((count = write(fd, str, 3)) < 0) // Write the message "str" to the source file
    		perror("write()");
  	else
    		printf("write(): written %ld byte(s)\n", count);


  	puts("");
  	printf("enqueue: 10 zero-bytes, \"abc\"\n");
  	if ((pos = lseek(fd, 0, SEEK_CUR)) < 0) // returns the current position of the file
    		perror("lseek()");
  	else
    		printf("lseek(): cur_fpos = %ld\n", pos);

  	if ((pos = lseek(fd, 10, SEEK_END)) < 0) // returns the new position of the file after adding 10 zeros
    		perror("lseek()");
  	else
    		printf("lseek(): new_fpos = %ld (zero-bytes)\n", pos);

  	if ((count = write(fd, str, 3)) < 0) // Write the message "str" to the source file
    		perror("write()");
  	else
    		printf("write(): written %ld byte(s) (zero-bytes + \"abc\")\n", count);


  	puts("");
  	printf("enqueue: 10 zero-bytes\n");
  	if ((pos = lseek(fd, 0, SEEK_CUR)) < 0) // returns the current position of the file
    		perror("lseek()");
  	else
    		printf("lseek(): cur_fpos = %ld\n", pos);

  	if ((pos = lseek(fd, 10, SEEK_END)) < 0)  // returns the new position of the file after adding 10 zeros
    		perror("lseek()");
  	else
    		printf("lseek(): new_fpos = %ld (zero-bytes)\n", pos);
  
  	if ((count = write(fd, NULL, 0)) < 0) // Write zero to the source file
    		perror("write()");
  	else
    		printf("write(): written %ld byte(s) (zero-bytes)\n", count);

  
  	puts("");
  	printf("terminate concatenation...\n");
  	if ((count = write(fd, NULL, 0)) < 0)
    		perror("write()");
  	else
    		printf("write(): written %ld byte(s) (issue last)\n", count);// Write zero again to source file to indiate it's the last bit 

  	readFancy(fd, hash_abc_10zero_abc_10zero,1);// go to readFancy fuction
  
  	printf("\n################################\n");
  	
  	if(ioctl(fd, SHA256_IOC_WCAT, 0) < 0) { // Disabling Concatination  by sending zero to ioctl
    		perror("ioctl()");
    		close(fd);
    		exit(EXIT_FAILURE);
  	}
  	
  	pthread_mutex_unlock(&mutex);// unclock the mutexlock

  	pthread_exit(NULL);
	

}

// Test function which is used to check without contenation and with forward seek
void With_Seek() {
	
	
    /* 
   * testing zero-message 
   */
  	printf("\n################################\n");
  	printf("Testing write without concatenation + forward seek...\n");
  	printf("1) msg: 1_000_000 zero-bytes\n");

  	if ((pos = lseek(fd, 0, SEEK_CUR)) < 0) // returns the current position of the file
    		perror("lseek()");
  	else
    		printf("lseek(): cur_fpos = %ld\n", pos);

  	if ((pos = lseek(fd, 1000000, SEEK_END)) < 0)// returns the new position of the file after adding 1000000 zeros
    		perror("lseek()");
  	else
    		printf("lseek(): new_fpos = %ld\n", pos);

  	if ((count = write(fd, NULL, 0)) < 0)// Write zero to the source file
    		perror("write()");
  	else
    		printf("write(): written %ld byte(s)\n", count);

  	readFancy(fd, hash_1000000zero,2);// go to readFancy fuction

 /* testing 10 zero_bytes and abc
 */
  	puts("");
  	printf("2) msg: 10 zero-bytes, \"abc\"\n");

  	if ((pos = lseek(fd, 0, SEEK_CUR)) < 0)// returns the current position of the file
    		perror("lseek()");
  	else
    		printf("lseek(2): cur_fpos = %ld\n", pos);

  	if ((pos = lseek(fd, 10, SEEK_END)) < 0)// returns the new position of the file after adding 10 zeros
    		perror("lseek()");
  	else
    		printf("lseek(): new_fpos = %ld (zero-bytes)\n", pos);

  	if ((count = write(fd, str, 3)) < 0)// Write the message "str" to the source file
    		perror("write()");
  	else
    		printf("write(): written %ld byte(s) (zero-bytes + \"abc\")\n", count);

  	readFancy(fd, hash_10zero_abc,2); // go to readFancy fuction

	pthread_mutex_unlock(&mutex);// unclock the mutexlock
	pthread_exit(NULL);


}

// Test function which is used to check without contenation and without seek
void Without_Seek() {
	
	
	   /* 
  	 * testing write without concatenation 
   	*/
  	printf("\n################################\n");
  	printf("Testing write without concatenation and seek...\n");
 	 printf("msg_3: %s\n", str);

  	if ((count = write(fd, str, 3)) < 0)// Write the message "str" to the source file
   		 perror("write(3)");
  	else if (count != 3)
   		fprintf(stderr, "write(3): failed writing 3 bytes\n"); 

  	readFancy(fd, hash_str,3);// go to readFancy fuction
		
	pthread_mutex_unlock(&mutex);// unclock the mutexlock
	pthread_exit(NULL);

}

// Printing the generated hash 
void printHash(const unsigned hash[8],int n) {

  
  printf("hash(%d): %08X",n,hash[0]);

  for (int i = 1; i < 8; i++)
   printf("_%08X", hash[i]); 
  
  puts("");
  
}

// Compare the hash values with predefined hash value
bool validateHash(const unsigned hash[8], const unsigned hash_xpected[8]) {
  bool result = true;

  for (int i = 0; i<8; i++)
   result &= (hash[i] == hash_xpected[i]);

  return result;
}
//read fuction
void readFancy(int fd, const unsigned hash_xpected[8],int n) {
  long count;
  unsigned hash_read[8];
  
  
  
  if ((count = read(fd, hash_read, 32)) < 0) // Read hash values from the device
    perror("read()");
  else if (count != 32) // Check if the hash successfully generated
    fprintf(stderr, "read(%d): failed reading 32 bytes\n",n); 
  else {

    printHash(hash_read,n);//go to printhash function to print hash which is generated by device
    
    if (!validateHash(hash_read, hash_xpected)) // check the hash values with predefined hash value
      fprintf(stderr, "hash mismatch!(%d)\n",n); 
    else
      printf("match!(%d)\n",n);

  }
 
}





