/**
 * File              : Tcp_server.c
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
 * Description		 : This program constructs a TCP server capable of concurrently handling multiple clients using multithreading. 
 *		           It offers various functions to clients for interacting with a shared folder on the server, such as listing 
 *		           files (Ls), creating directories (mkdir), and downloading file contents while verifying their integrity 
 *		           using the SHA256 checksum.
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <semaphore.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#define PORT 1234					//Port ot the tcp
#define BUFFER_SIZE 1024			//length of message that we want to send to client
#define MAX_CLIENTS 2				//Number of clients 
#define TERMINATION_MESSAGE "bye"	//a message to teminate session 
#define MAX_LINE_LENGTH 100			//Number of characters in a line to be read

typedef struct {
    int socket;
    int active;
    int client_slot;
} ClientInfo;

typedef struct {
    int client_slot;
    int socket;
    char *file_name;
} Cal_data;

Cal_data clients_data_for_cal[MAX_CLIENTS];
char buffer_to_send[MAX_CLIENTS][BUFFER_SIZE]; 
char hex_digest[MAX_CLIENTS][(sizeof(unsigned)*2*8)+1];			//sizeof(unsigned) for each block and  multiply by 2 because each byte represent in two hexadecimal and 8 because we have 8 blocks
sem_t sem[MAX_CLIENTS];											//To synchronize between threads of each client 
pthread_mutex_t mutex;											//To prevent access a file by multiple clients
pthread_mutex_t mutex_core;										//To prevent access to core by multiple clients
/*
**prototype of functions
*/
void respond_client(char *msg, int socket);						
void *Read_file(void *arg);
void *cal_hash(void *arg);
void* handleClient(void* arg);
void Read_calculated_hash(int fd, int socketNumber);

int main() {
    int serverSocket, newSocket, clientLength;
    struct sockaddr_in serverAddress, clientAddress;
    pthread_t threads[MAX_CLIENTS];
    ClientInfo clients[MAX_CLIENTS];

    // Create socket
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);		//AF_INET represent address which is ipv4 in our case, SOCK_STREAM is used for TCP
    if (serverSocket == -1) {
        perror("Failed to create socket");
        exit(EXIT_FAILURE);
    }

    // Set up server address
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;			//INADDR_ANY instructs the operating system to bind the socket to all available IP addresses on the machine
    serverAddress.sin_port = htons(PORT);				//htons convert the port number from host byte order to network byte order.

    // Bind socket to the specified address and port
    if (bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
        perror("Binding failed");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(serverSocket, 3) < 0) {  				//3 etermining the maximum number of pending connections that can wait in the queue before the server starts rejecting new connections
        perror("Listening failed");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", PORT);
	
    pthread_mutex_init(&mutex, NULL);					//init mutex NULL means we want to use default attributes
	pthread_mutex_init(&mutex_core, NULL);
    // Initialize clients
    for (int i = 0; i < MAX_CLIENTS; i++) {				//init all clients variables and their semaphore variable
        clients[i].socket = -1;
        clients[i].active = 0;
	sem_init (&sem[i],0,0);								
    }
	
    while (1) {											//It is a listner server so it should be in a loop
        clientLength = sizeof(clientAddress);			//This is necessary to properly retrieve the client's address information during the accept() function call.
        newSocket = accept(serverSocket, (struct sockaddr*)&clientAddress, (socklen_t*)&clientLength);		//accepts an incoming client connection on the serverSocket and creates a new socket (newSocket) dedicated to this client.
        if (newSocket < 0) {
            perror("Accepting connection failed");
            exit(EXIT_FAILURE);
        }

        // Find an available client slot
        int slot = -1;									//check do we have free slot or not
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (!clients[i].active) {
                slot = i;
                break;
            }
        }

        if (slot == -1) {								//if we dont have free slot then we reject client
            printf("Maximum number of clients reached. Rejecting new connection.\n");
            close(newSocket);
        } else {
            clients[slot].socket = newSocket;			//fill member of structure with proper values to pass it to thread of client 
            clients[slot].active = 1;
			clients[slot].client_slot = slot;
			sem_init (&sem[slot],0,0);					//initalize the semaphor of this client to 0

            
            if (pthread_create(&threads[slot], NULL, handleClient, (void*)&clients[slot]) != 0) {	// Create a new thread to handle the client
                perror("Failed to create thread");
                exit(EXIT_FAILURE);
            }
        }
    }

    // Close the server socket
    close(serverSocket);

    return 0;
}


//This a function to send a message to a client 
void respond_client(char *msg, int socket){
	if (write(socket, msg, strlen(msg)) < 0) {
		perror("Writing to socket failed");
		close(socket);
		pthread_exit(NULL);
	}
}

//This function is responsible to read the data inside a text file and save it inside buffer_to_send variable of its client
void *Read_file(void *arg){
	Cal_data* client = (Cal_data*)arg;
	int clientSlot = client->client_slot;
	const char* filename = client->file_name;
	pthread_mutex_lock(&mutex);									//we luck mutex to prevent multiple access of files with different clients
	FILE* file = fopen(filename, "r");
	if (file == NULL) {
		printf("Failed to open file: %s\n", filename);
	}
	char line[MAX_LINE_LENGTH];
	while (fgets(line, sizeof(line), file) != NULL) {
		strcat(buffer_to_send[clientSlot], line);
	}
	//printf("%s", buffer_to_send[clientSlot]);
	fclose(file);
	pthread_mutex_unlock(&mutex);								//we unlock accesss to files for other threads
	sem_post (&sem[clientSlot]);								//cal_hash function must be executed after this function so we control it with semaphore
	pthread_exit(NULL);
}

void *cal_hash(void *arg){
	Cal_data* client = (Cal_data*)arg;
	int clientSlot = client->client_slot;
	int clientsocket1 = client->socket;
	sem_wait(&sem[clientSlot]);									//wait on semaphore of this client
	char* message = buffer_to_send[clientSlot];					//copy content of file into message variable
	//printf("%s\n", message);
	size_t message_len = strlen(message);						//calculate size of input message
	int fd;
	long pos, count;
	pthread_mutex_lock(&mutex_core);							//lock access to the core
	if((fd = open("/dev/sha256", O_RDWR)) < 0) {				
	perror("open()");
	exit(EXIT_FAILURE);
	}
	printf("msg: %s\n", message);
	if ((count = write(fd, message, strlen(message))) < 0)		//write function to pass message to driver
	perror("write()");
	else if (count != strlen(message))
	fprintf(stderr, "write(): failed writing bytes\n");
	Read_calculated_hash(fd,clientsocket1);												//read output of the driver 
	printf("################################\n");
	close(fd); 
	pthread_mutex_unlock(&mutex_core);							//unlock access to the core
 	strcat(buffer_to_send[clientSlot], hex_digest[clientSlot]);	//copy output of driver into buffer that includes message 
	printf("%s\n", buffer_to_send[clientSlot]);				
	respond_client(buffer_to_send[clientSlot],clientsocket1);
	memset(buffer_to_send[clientSlot], 0, BUFFER_SIZE);			 //reset buffer
	memset(hex_digest[clientSlot], 0, (sizeof(unsigned)*2*8)+1); //reset hex_digest
	pthread_exit(NULL);											 //exit thread
}
//This is the function  which handle each client
void* handleClient(void* arg) {
	ClientInfo* client = (ClientInfo*)arg;
	int clientSocket = client->socket;
	int ClientNumber = client->client_slot;
	char buffer[BUFFER_SIZE] = {0};

	while (1) {														//since this function get command it will listen on client in a loop till client terminate session
		int bytesRead = read(clientSocket, buffer, BUFFER_SIZE);	//read message from client 
		if (bytesRead < 0) {
			perror("Reading from socket failed");
			close(clientSocket);
			client->active = 0;  // Mark client as inactive
			pthread_exit(NULL);
		} else if (bytesRead == 0) {								//This case will terminate session
			printf("Client disconnected\n");
			close(clientSocket);
			client->active = 0;  // Mark client as inactive
			pthread_exit(NULL);
		}

		printf("Received message from client: %s\n", buffer);

		if (strcmp(buffer, TERMINATION_MESSAGE) == 0) {				//if it is a termination message it will terminate session
			printf("Termination message received. Closing connection.\n");
			close(clientSocket);
			client->active = 0;  // Mark client as inactive
			pthread_exit(NULL);
		}

		if (strcmp(buffer, "ls") == 0) {							//it sends list of files and directories in share folder to client		
		//printf("I will create a folder\n");
			struct dirent *dp;
			DIR *dir = opendir("share");
		while ((dp = readdir(dir)) != NULL)
		{
			respond_client(dp->d_name,clientSocket);
			respond_client("\n",clientSocket);
			printf("%s\n", dp->d_name);
		}
		closedir(dir);
		}
		if (strcmp(buffer, "mkdir") == 0) {							//if we receive this command then it ask client to enter a name to create a folder with that name in share folder 
			respond_client("please give me the name of folder that you want\n",clientSocket);	//send this message to client 
			char command[50];
			int sys_value;
			memset(buffer, 0, BUFFER_SIZE);
			read(clientSocket, buffer, BUFFER_SIZE);											//read name from client
			strcpy( command, "mkdir share/");
			strcat( command, buffer);
			printf("%s\n",command);
			sys_value = system(command);														//use system to create folder
		}	
		if (strcmp(buffer, "Download") == 0) {													/*if server receive this command it asks for the name of the file and then it 
																								**creat two threads one to read file content and another one to calculate the hash of file content 
																								*/
			respond_client("please give me the name of file that you need\n",clientSocket);		//Ask client for a name of the file
			memset(buffer, 0, BUFFER_SIZE);
			read(clientSocket, buffer, BUFFER_SIZE);											//read name
			char command[50];
			strcpy( command, "share/");
			strcat( command, buffer);
			clients_data_for_cal[ClientNumber].client_slot = ClientNumber;
			clients_data_for_cal[ClientNumber].file_name = command;
			clients_data_for_cal[ClientNumber].socket = clientSocket;
			pthread_t tid;
			pthread_create(&tid, NULL, Read_file, (void*)&clients_data_for_cal[ClientNumber]);	//create thread of reading file
			pthread_create(&tid, NULL, cal_hash,  (void*)&clients_data_for_cal[ClientNumber]);	//create thread of calculating hash of file 
		}
		memset(buffer, 0, BUFFER_SIZE);
	}
}

void Read_calculated_hash(int fd, int socketNumber) {
  long count;
  unsigned hash_read[8];
  if ((count = read(fd, hash_read, 32)) < 0)						//Reading of calculated hash
    perror("read()");
  else if (count != 32)
    fprintf(stderr, "read(): failed reading 32 bytes\n");
  else {
  for(int i=0;i<8;i++){
	  sprintf(&hex_digest[socketNumber][i*8], "%08X", hash_read[i]);	//save them as hexadecimal value in an array
	}
  }
} 
