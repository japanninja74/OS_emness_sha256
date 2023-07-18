/**
 * File              : ex2_compare.c
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



/*
 * This C program reads a text file and saves its content into a memory buffer ('msg'). Then the hash of the file is computed in two different ways: 
 * accessing the external cryptocore and using 'sha256sum' Unix command. the two results are compared, in order to know if the external device works properly.
 * The file name must be passed as an input parameter to the program.
 * Since we don't know how large the file is, dynamic memory allocation is used. 1024 characters at a time are read from the file.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>

int main (int argc, char* argv[]){

// Check the input parameter
if (argc != 2){
   printf("Wrong input parameters\n");
   printf("Usage: <program_name> <file_name>\n");
   exit(-1);
}

int fd=0;
int bytes_read=0, size=0;
char buf[1024];    //temporary buffer to store last 1024 characters read from file 
char* msg = NULL;  //message to be sent to the cryptocore
char* tmppointer = NULL;  //temporary pointer during 'realloc'
pid_t pid;

// Open text file
fd = open(argv[1], O_RDONLY);
if (fd == -1){
   perror ("open error");
   exit(-1);
}

// Read the first 1024 characters
bytes_read = read (fd, buf, 1024);

if (bytes_read == -1){
   perror("read error");
   exit(-1);
}

// Repeat until all characters have been read from the file
while (bytes_read != 0){   
   
   // Dynamic memory allocation
   tmppointer = (char*) realloc (msg, size + bytes_read*sizeof(char));
   
   if (tmppointer == NULL) {
      perror("realloc error");
      exit(-1);
   }
   
   msg = tmppointer;
   
   // Adding the last 1024 characters read into the message
   if (memcpy(msg + size, buf, bytes_read) == NULL){
      perror("memcpy error");
      exit(-1);
   }
   
   size += bytes_read;
   
   // Read other 1024 characters
   bytes_read = read (fd, buf, 1024);
   
   if (bytes_read == -1){
      perror("read error");
      exit(-1);
   }
}

// Add one byte to store the string terminator
tmppointer = (char*) realloc (msg, size + sizeof(char));

if (tmppointer == NULL) {
   perror("realloc error");
   exit(-1);
}

msg = tmppointer;

msg[size] = '\0';

if (close(fd) == -1) {
   perror("close error");
   exit(-1);
}

// Create a child process to launch 'sha256sum' command with 'execl' system call
pid = fork();
if (pid < 0){

   perror("fork error");
   exit(-1);
   
} else if (pid == 0){ //child process

   char* cmd;
   
   // Create commmand to be send to Linux shell
   cmd = (char*) malloc((strlen(argv[1])+22)*sizeof(char));
   
   if(sprintf(cmd, "sha256sum %s > hash.txt", argv[1]) != strlen(argv[1])+21) {
      perror("sprintf error");
      exit(-1);
   }
   
   execl("/bin/sh", "sh", "-c", cmd, NULL);
   perror("execl error");
   exit(-1);
      
} else { //parent process

   int fd_dev, fd_hash, status;
   char hash_file[65], hash_dev[65], temp_string[9];
   unsigned hash[8];
   
   // Open the device file descriptor
   fd_dev = open("/dev/sha256", O_RDWR);
   
   if (fd_dev == -1){
      perror("device open error");
      exit(-1);
   }
   
   // Write the whole message to the device
   if (write(fd_dev, msg, strlen(msg)) == -1){
      perror("device write error");
      exit(-1);
   }
   
   // Read the hash from the device
   if ((read(fd_dev, hash, 32)) == -1){
      perror("device read error");
      exit(-1);
   }
   
   // Close the device file descriptor
   if (close(fd_dev) == -1){
      perror("device close error");
      exit(-1);
   }
   
   // Convert the read message into a string (hexadecimal format)   
   sprintf(hash_dev, "%08x", hash[0]);
   
   for (int i=1; i<8; i++){
      sprintf(temp_string, "%08x", hash[i]);
      strcat(hash_dev, temp_string);
   }
   
   // Wait child termination
   wait(&status);
   
   if (!WIFEXITED(status)){
      printf("Error in child termination\n");
      exit(-1);
   }
   
   // Open file with hash computed by the OS
   fd_hash = open("hash.txt", O_RDONLY);
   
   if (fd_hash == -1){
      perror("open error");
      exit(-1);
   }
   
   // Read the hash from the file
   if ((read(fd_hash, hash_file, 64)) == -1){
      perror("read error");
      exit(-1);
   }
   
   // Close file 
   if (close(fd_hash) == -1){
      perror("close error");
      exit(-1);
   }
   
   free(msg);
   
   // Add string terminator
   hash_file[64] = '\0';
     
   // Compare the two hashes
   if(strcmp(hash_file, hash_dev) == 0){
      printf("SUCCESS! The two hashes are equal.\n");
   } else {
      printf("FAILURE! The two hashes are different.\n");
   }
   
   // Print the two hashes
   printf("\n");
   printf("hash from device: %s\n", hash_dev);
   printf("hash from OS:     %s\n", hash_file);
   
   // Remove hash file generated by the OS
   system("rm hash.txt");
   
   exit(0);
}
}
