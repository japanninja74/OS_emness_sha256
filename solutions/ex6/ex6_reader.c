/**
 * File              : ex6_reader.c
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
 
/* Description of this Program/*
 /*
 *In the provided C code, the "reader" is waiting for the hash value of the message "Alarm" to send a SIGALRM signal to the parent process(of the reader).
 *The program uses a named pipe "aPipe" to communicate between the parent process (writer) and the child process (reader).

 *When the parent process (writer) sends the message "Alarm" along with its SHA-256 hash to the named pipe, the reader component reads the data from the named pipe.
 *It checks whether the received message matches "Alarm" and its corresponding hash value matches the precomputed SHA-256 hash for "Alarm." If both conditions are
 *met, the reader sends a SIGALRM signal to the parent process.

 *This mechanism could be part of a larger program where the parent process is waiting for an "Alarm" event to occur, and the reader component is responsible for
 *detecting this event and signaling the parent process accordingly using the SIGALRM signal.
 */
 
 /*Reader*/
 
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h> 
#include <sys/types.h> 
#include <sys/stat.h> 
#include <stdbool.h>
#include <stdlib.h>

//Precomputed Hash value of "Alarm"
static const unsigned hash_s[] = {
   0xb165bcd8,
   0xd20bdfda,
   0xcdf05f21,
   0x9205afd4,
   0x986403db,
   0x471c60ff,
   0x3ff198a8,
   0x2020c0d9};

// Function to be called when SIGALRM signal is received
void release() {

  printf("Alarm message is received\n");
  
}

int main() {
    int fd1 = open("aPipe", O_RDONLY); // Open the pipe only for reading
    unsigned hash[8]; // Array to store the hash values read from the pipe
    bool result = false; // Flag to indicate if the hash values match
   
    pid_t pid;

    // Check if the named pipe was successfully opened
    if (fd1 == -1) {
        perror("open");
        return 1;
    }

    pid = fork(); // Fork a child process from the parent process

    // Check for fork errors
    if (pid < 0) {
        perror("fork");
        return 1;
    } else if (pid == 0) {
        // Child process

        close(fd1); // Close the read end of the pipe
        
        // Open the pipe again for reading
        fd1 = open("aPipe", O_RDONLY);

        // Loop to read hash values from the named pipe
        while (!result) {
        
            read(fd1, hash, sizeof(unsigned) * 8); // Read hash values from the pipe
            
            bool cmp = true; // Flag for hash comparison
           
            // Compare the rest of the hash values with predefined hash_str
            for (int j = 0; j < 8; j++) {
                cmp &= (hash[j] == hash_s[j]); // Check if the hash values match
        
            }

            result = cmp; // Update the result flag based on comparison
            if (!result) {
                printf("Hash mismatch!.\n");
            }
        }

        if (result) {
            printf("Hash values matched.\n");
            kill(getppid(), SIGALRM); // Send SIGALRM signal to the parent process
        }

        close(fd1); // Close the named pipe
    } else {
        // Parent process

        close(fd1); // Close the read end of the named pipe
        
        // Set up the signal handler for SIGALRM signal to call release() function
        signal(SIGALRM, release);
        
        pause(); // Wait until the SIGALRM signal is received
        
        exit(0); // Exit the parent process
    }

    return 0; // Return success status
}
