/**
* File              : ex7_client.c
* 					 client simlation program
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
* Last Modified Date: 06.07.2023
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
* Execute this program in another window after launching the server, and use one of the possible id thread (shown in output from server) as argument
*/

#include <stdio.h>                  //              input/output
#include <string.h>                 //              string manipulation
#include <stdlib.h>                 //              exit function   
#include <unistd.h>                 //              file (driver) operations
#include <errno.h>                  //              error output
#include <sys/ipc.h>                //              interprocess comunication (semaphores and shared memory)
#include <sys/sem.h>                //              semaphores
#include <sys/shm.h>                //              shared memory
#include <pthread.h>                //              threads
#include "ex7_sem_shm_string.h"     //              header for semaphore calls and common #define between server and



//argument of client is the thread id, which symbolizes the port server
int main (int argc, char *argv[]){
    
    typedef struct {
        char email[EMAIL_LENGTH];
        char password[HASH_LENGTH];
        char server_message[MSG_SIZE];
        } user;
    user *to_send;
    char *passptr;
    //shared memory and semaphore variables
    int semid, shmid;
    union semun arg, dummy;
    struct sembuf  sop;
    //keys for shared memory and semaphore and creation of them
    key_t sem_key, shm_key;
    //key of semaphore and memory created with thread_id
    sem_key= ftok (sem_path,  atoi(argv[1]));
    shm_key= ftok (shm_path,  atoi(argv[1]));  
    shmid = shmget ( shm_key, sizeof(user), IPC_CREAT | 0666);
    if (shmid ==-1) {
        perror("Error in memory attachment");
        exit(errno);
    }
    //set of semaphores made of one single semaphore
    semid = semget ( sem_key, 2 , IPC_CREAT | 0666);
    if (semid == -1) {
        perror("Error in semaphores attachment");
        exit(errno);
    }
    //attach shared memory region
    to_send = (user *) shmat (shmid, NULL, 0);
    //insert user and password and send them to server
    printf("enter your email address\n");
    scanf("%s", to_send->email);    
    passptr=getpass("enter your passowrd and press enter: ");
    strcpy(to_send->password,passptr);
    //decrement semaphore 0 to 0
    sop.sem_num = 0;
    sop.sem_op = -1;
    sop.sem_flg = 0;
    if (semop(semid, &sop, 1) == -1){
        perror("Error in semaphore 0 op");
        exit(errno);
    }
    
    
    arg.val = 1;                                                    
    if (semctl(semid, 1, SETVAL, arg) == -1){
        perror("Error in semaphore #1 ctl");
        exit(errno);
    }
    printf("waiting for response from server...\n");
    //wait for semaphore to become 0
    sop.sem_num = 1;
    sop.sem_op = 0;
    sop.sem_flg = 0;         
    if (semop(semid, &sop, 1) == -1){
        perror("Error in semaphore 1 operation");
        exit(errno);
    }
        
    printf("%s\n", to_send->server_message);
    shmdt(to_send); //detach shared memory region, disconect my process to shared memory region

    return 0;
  
} 
