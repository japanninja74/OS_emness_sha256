/**
* File              : ex7_server.c
*                     server simulation program
*
* Authors           : Fabio Scatozza      <s315216@studenti.polito.it>
*                     Isacco Delpero      <s314713@studenti.polito.it>
*                     Leonardo Cerruti    <s317664@studenti.polito.it>
*                     Claudio Capobianchi <s319187@studenti.polito.it>
*                     Saman Alipour       <s307980@studenti.polito.it>
*                     Abdul Rehman        <s315198@studenti.polito.it>
*                     Stephano Perera     <s313080@studenti.polito.it>
*
* Date              : 04.07.2023
* Last Modified Date: 20.07.2023
*
* Copyright (c) 2023
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 2 of the License, or
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

/**
 * 
 * The application I developed reproduces the scenario of logging from a client to a server.
 * The latter has a database of users containing per each of them its email, hashed password and a message from last login attempt. After having checked whether the email is present in the database, the password is hashed by the crypto core and the result is compared with all hashes resulting in a successfull access or not.
 * Interprocess communication is implemented in System V through a shared memory region, necessary for email and password, and a couple of semaphores: the first one pauses the server until email and password are transferred, and the second one pauses the client, waiting for a response from the server.
 * I have enriched server capabilities by including in it an arbitrary number of threads representing connection client-server: after setting up IPC each of them prints its thread ID which is used, just like a real server port, as a command line argument by the client to connect to that specific thread. This allows to have as connections as threads, using only one client executable. For the hash computation, the unique crypto core is assigned to a single thread thanks to mutex, locking the device for the time strictly necessary for computing the hash.
 * 
 */

#define _GNU_SOURCE             //required for  gettid of thread
#include <stdio.h>              //              input/output
#include <string.h>             //              string manipulation
#include <stdlib.h>             //              exit function   
#include <unistd.h>             //              file (driver) operations
#include <errno.h>              //              error output
#include <sys/ipc.h>            //              interprocess comunication (semaphores and shared memory)
#include <sys/sem.h>            //              semaphores
#include <sys/shm.h>            //              shared memory
#include <pthread.h>            //              threads
#include "ex7_sem_shm_string.h" //              header for semaphore calls and common #define between server and client

                                //              driver
#include <stdbool.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#define SHA256_IOC_MAGIC  'S'
#define SHA256_IOC_WCAT   _IOW(SHA256_IOC_MAGIC, 1, int)

#define DB_SIZE 3

#define NUM_THREADS 4

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
//data structure for users database


typedef struct {
    char email[EMAIL_LENGTH];
    char password[HASH_LENGTH];
    char server_message[MSG_SIZE];
} user;

void generate_hash(int fd, char *hashed);
void *th_routine (void *database);

//thread routine in loop
void *th_routine (void *database){
    while(1){
        int i, err, user_match=0;
        //pointer to databse for matching
        user *db= (user *) database;
        //pointer to shared memory region
        user *receivedPtr;
        //thread id as a simulation of port of a web server
        pid_t thread_id=gettid();
        char hashed_password[HASH_LENGTH];
        //driver variables
        int fd;
        long pos, count;
        if((fd = open("/dev/sha256", O_RDWR)) < 0) {
            perror("open()");
            exit(EXIT_FAILURE);
        }
        //shared memory and semaphore variables
        int semid, shmid;
        union semun arg, dummy;
        struct sembuf  sop;
        //keys for shared memory and semaphore and creation of them with thread_id
        key_t sem_key, shm_key;
        sem_key= ftok (sem_path, thread_id);
        shm_key= ftok (shm_path, thread_id);  
        shmid = shmget ( shm_key, sizeof(user) , IPC_CREAT | 0666);
        if (shmid ==-1) {
            perror("Error in memory attachment");
            exit(errno);
        }
        //set of semaphores made of two semaphores, one client-> server and one server->client
        semid = semget ( sem_key, 2 , IPC_CREAT | 0666);
        if (semid == -1) {
        perror("Error in semaphores attachment");
        exit(errno);
        }
        //semaphore 0 initialized to 1
        arg.val = 1;                                                    
        if (semctl(semid, 0, SETVAL, arg) == -1){
            perror("Error in semaphore 0 ctl");
            exit(errno);
        }
        printf("server waiting and listening to port %d\n",thread_id);
        //wait for semaphore 0 to become 0
        sop.sem_num = 0;
        sop.sem_op = 0;
        sop.sem_flg = 0;         
        if (semop(semid, &sop, 1) == -1){
            perror("Error in operation for semaphore 0");
            exit(errno);
        } 
        //instruction from now on are executed once the semaphore is decremented by the client
        //attach shared memory region
        receivedPtr=(user *) shmat (shmid, NULL, 0);
        //assign the unique crypto core to the first thread who got "green light"
        pthread_mutex_lock(&mutex);                                     
        
        //writing password from client to the crypto core to be hashed
                                                                        
        if ((count = write(fd, receivedPtr->password, strlen(receivedPtr->password))) < 0)
            perror("write()");
        else if (count != strlen(receivedPtr->password))
            fprintf(stderr, "write(): failed writing 32 bytes\n");
        //create hash string from 8 unisgned ints
        generate_hash(fd,hashed_password );                             
        //check wheter there is a matching username and is so the given password
        for (i=0; i<3 && user_match==0; i++){                           
            if (strcmp(db[i].email,receivedPtr->email)==0) {
                user_match=1;
                //strcmp(database[i].hashed_password,receivedPtr->hashed_password
                if (strcmp(db[i].password,hashed_password)==0) {
                    strcpy(receivedPtr->server_message,"access granted");
                    printf("[%d] access granted for user %s\n", thread_id, receivedPtr->email);
                }
                else {
                    strcpy(receivedPtr->server_message,"wrong password");
                    printf("[%d] wrong password for user %s\n", thread_id, receivedPtr->email);
                }
            //release of the crypto core, now it is available to all threads again
            pthread_mutex_unlock(&mutex);                              
            }
        }
        if (user_match==0) {
            strcpy(receivedPtr->server_message,"user not found");
            printf("[%d] user not found\n", thread_id);
            //release of the crypto core, now it is available to all threads again
            pthread_mutex_unlock(&mutex);                             
            
        }
        
        //decrement semaphore #1 from 1 to 0
        sop.sem_num = 1;
        sop.sem_op = -1;
        sop.sem_flg = 0;
        if (semop(semid, &sop, 1) == -1){
            perror("Error in semaphore 1 op");
            exit(errno);
        }
        
        //give time to client process to pass from ready to run state and save content of shared memory
        //this avoids the semaphore to be deleted too early
        usleep(1000);
        if (semctl(semid, 0, IPC_RMID, dummy) == -1) {
            perror("Error in removing semaphores");
            exit(errno);
        }
        
        //detach shared memory region, disconect my process to shared memory region
        shmdt(receivedPtr);
        //release share memory region
        if(shmctl(shmid, IPC_RMID, 0) ==-1){
            perror("Error in removing memory");
            exit(errno);
        }
        //close connection to hash driver
        close(fd);                                                       
        }
}
int main () {
    int i,err;
    //example of server database with user and hashed passwords
    user database[DB_SIZE];    
    strcpy(database[0].email,"user1@polito.it");
    strcpy(database[0].password,"e6c3da5b206634d7f3f3586d747ffdb36b5c675757b380c6a5fe5c570c714349");    //passowrd=pass1
    strcpy(database[1].email,"user2@polito.it");
    strcpy(database[1].password,"1ba3d16e9881959f8c9a9762854f72c6e6321cdd44358a10a4e939033117eab9");    //passowrd=pass2
    strcpy(database[2].email,"user3@polito.it");
    strcpy(database[2].password,"3acb59306ef6e660cf832d1d34c4fba3d88d616f0bb5c2a9e0f82d18ef6fc167");    //password=pass3
    
    //creation of arbitrary number of threads with access to the database
    pthread_t tid[NUM_THREADS];
    for (i=0; i<NUM_THREADS; i++){
        err= pthread_create (&tid[i], NULL,&th_routine, (void *) &database);   
        if (err!=0) {
            printf ("Unable to create thread [%s]",strerror(err));
            exit(1);
        }  
    }
    for (i=0; i<NUM_THREADS; i++) {
        pthread_join (tid[i],NULL);
    }
    
    return 0;
}

void generate_hash(int fd, char *hashed) {
    long count;
    //array of 8 unsigned ints fot the 8 output words from hash
    unsigned hash_read[8];
    //read from the hash 32 byte (the complete hash)
    if ((count = read(fd, hash_read, 32)) < 0)
        perror("read()");
    else if (count != 32)
        fprintf(stderr, "read(): failed reading 32 bytes\n");
    else {
        //print unsigned int in string format and relative concatenation
        char temp_string[9];
        sprintf(hashed, "%08x", hash_read[0]);
        for (int i = 1; i < 8; i++) {
            sprintf(temp_string, "%08x", hash_read[i]);
            strcat(hashed, temp_string);
        }
    }
}
