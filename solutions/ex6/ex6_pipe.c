/**
 * File              : ex6_pipe.c
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
 *met, the reader sends a *SIGALRM signal to the parent process.

 *This mechanism could be part of a larger program where the parent process is waiting for an "Alarm" event to occur, and the reader component is responsible for
 *detecting this event and signaling the parent process accordingly using the SIGALRM signal.
 */
 
 /*Pipe*/
 
#include <stdio.h>
#include <sys/types.h> 
#include <sys/stat.h> 
#include <fcntl.h>

int main() {
    // Create a named pipe (FIFO) named "aPipe" with read and write permissions for the owner (user).
    // The named pipe will be created in the current directory where this program is executed.
    // The permissions are set to 0600, meaning read and write permissions for the owner and no permissions for group and others.
    mkfifo("aPipe", 0600);

    
    return 0;
}
