
/**
 * File              : ex6_writer.c
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
 
 /*Description of this Program/*
 /*
 *In the provided C code, the "reader" is waiting for the hash value of the message "Alarm" to send a SIGALRM signal to the parent process(of the reader).
 *The program uses a named pipe "aPipe" to communicate between the parent process (writer) and the child process (reader).

 *When the parent process (writer) sends the message "Alarm" along with its SHA-256 hash to the named pipe, the reader component reads the data from the named pipe.
 *It checks whether the received message matches "Alarm" and its corresponding hash value matches the precomputed SHA-256 hash for "Alarm." If both conditions are
 *met, the reader sends a SIGALRM signal to the parent process.

 *This mechanism could be part of a larger program where the parent process is waiting for an "Alarm" event to occur, and the reader component is responsible for
 *detecting this event and signaling the parent process accordingly using the SIGALRM signal.
 */
 
 /*Writer*/
 
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/ioctl.h>

#include <sys/types.h> 
#include <sys/stat.h> 

#define SHA256_IOC_MAGIC  'S'
#define SHA256_IOC_WCAT   _IOW(SHA256_IOC_MAGIC, 1, int)

unsigned* read_hash(int fd);
void printHash(const unsigned hash[8]);

int main() {
    int fd, fd1;
    unsigned* hash;
    char s[1024];
    int i;
    
    // Open the pipe for writing (non-blocking and synchronous)
    fd1 = open("aPipe", O_WRONLY | O_SYNC | O_NONBLOCK);
    
    // Open the source file for reading and writing
    if ((fd = open("/dev/sha256", O_RDWR)) < 0) {
        perror("open()");
        exit(EXIT_FAILURE);
    }
    //Resetting the device's concatenation
    if(ioctl(fd, SHA256_IOC_WCAT, 0) == -1) {
		      perror("ioctl error");
		      exit(-1);
    }

    // Loop to read 100 messages from the user, calculate the hash, and write it to the  pipe
    for (i = 0; i < 100; i++) {
        printf("Write a message: ");
        fgets(s, sizeof(s), stdin);
        
        // Write the message to the source file
        write(fd, s, strlen(s) + 1);
        
        // Read the hash values calculated by the read_hash() function
        hash = read_hash(fd);
        
        // Write the hash values to the named pipe "aPipe"
        write(fd1, hash, sizeof(unsigned) * 8);
    }

    close(fd); // Close the source file
    exit(EXIT_SUCCESS); // Exit the program successfully
}

// Function to read the hash values from the file descriptor "fd"
unsigned* read_hash(int fd) {
    long count;
    static unsigned hash_read[8]; // Use static to retain data after the function returns.
    
    // Read 32 bytes (8 unsigned integers) from the file descriptor
    if ((count = read(fd, hash_read, 32)) < 0)
        perror("read()");
    else if (count != 32)
        fprintf(stderr, "read(): failed reading 32 bytes\n");
    else {
        // Return the pointer to the array of hash values
        return hash_read;
    }
    
    // Return NULL in case of failure or when count != 32
    return NULL;
}
