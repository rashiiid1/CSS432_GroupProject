
// from client

//-w read from the client and write on the server
//-r read a file from the server and write on the client

// for system calls, please refer to the MAN pages help in Linux
// sample echo tensmit receive program over udp - CSS432 - Autumn 2022
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <errno.h>	// for retrieving the error number.
#include <string.h> // for strerror function.
#include <signal.h> // for the signal handler registration.

#include <unistd.h>

#include "tftp.h"

#define SERV_UDP_PORT 51527 // REPLACE WITH YOUR PORT NUMBER

int totalTimeOut = 0;

char *progname;
void handle_timeout(int signum)
{
	printf("timeout!\n");
	totalTimeOut++;
	printf("Alarm Fired %d times\n", totalTimeOut);
}

/*
 * This function handles the timeout by incrementing
 * the corresponding global variables.
 *
 * Input:  signal number.
 * Return: None.
 *
 */
int register_handler()
{
	int rt_value = 0;
	/* register the handler function. */
	rt_value = (int)signal(SIGALRM, handle_timeout);
	if (rt_value == (int)SIG_ERR)
	{
		printf("can't register function handle_timeout.\n");
		printf("signal() error: %s.\n", strerror(errno));
		return -1;
	}
	/* disable the restart of system call on signal. otherwise the OS will stuck in
	 * the system call
	 */
	rt_value = siginterrupt(SIGALRM, 1);
	if (rt_value == -1)
	{
		printf("invalid sig number.\n");
		return -1;
	}
	return 0;
}


// void recv_file(int sockfd)
void *myThreadFun(void *vargp)
{
	int *pData = (int*)vargp;
	int sockfd = *pData;
	char fileName[20];
	struct sockaddr pcli_addr;
	int i;

	printf("Thread started with socket %d\n", sockfd);

	int bytes_received, clilen, bytes_sent;
	Message message;

	clilen = sizeof(struct sockaddr);

	while (1)
	{
		
    	bytes_received = recvfrom(sockfd, (void *)&message, sizeof(Message), 0, &pcli_addr, &clilen);

		int client_id = -1;

		if (bytes_received < 0)
		{
			printf("%s: recvfrom error\n", progname);
			exit(3);
		}

		// check if message received is from an existing client.
		for (i = 0; i < MAX_CLIENTS; i++)
		{
			if ((sockaddr_cmp(&pcli_addr, &Client_Addr_List[i]) == 0) && (Client_Active[i] == 1))
			{
				printf("Server: Existing client with assigned ID: %d\n", i);
				client_id = i;
				break;
			}
		}

		if (client_id == -1)
		{ // this is a new client
			
			for (i = 0; i < MAX_CLIENTS; i++)
			{ // find empty slot in client_active_list
				if(Client_Active[i] == 0)
				{
					printf("Server: New client. Assigned ID is %d\n", i);
					client_id = i;
					break;
				}
			}

			if (i >= MAX_CLIENTS)
			{
				printf("Server: Max clients reached. Ignoring new client request. \n");
				continue;
			}
			
			Client_Active[client_id] = 1;
			memcpy(&Client_Addr_List[client_id], &pcli_addr, clilen);
		}
		else
		{

		}

		if (message.opCode == WRQ)
		{
			printf("%s: received first WRQ \n", progname);
			int startIndex = 0;
			int c;
			int index = 0;
			while (1)
			{
				c = message.data[startIndex];
				if (c == 0)
				{
					break;
				}
				startIndex++;
				fileName[index] = c;
				index++;
			}
			fileName[index] = '\0';

			File_List[client_id] = fopen(fileName, "r");
			
			if (File_List[client_id] != NULL) // file already exists
			{
				printf("File already exists: %s\n", fileName);
				memset(message.data, 0, sizeof(message.data));
				message.opCode = ERROR;
				message.block = 6;
				char err_msg[] = "File already exists.";
				strcpy(message.data, err_msg);
				/* Send the ERROR packet. */
				//if (sendto(sockfd, (void *)&message, sizeof(message), 0, &pcli_addr, clilen) <= 0)
				if (sendto(sockfd, (void *)&message, sizeof(message), 0, &Client_Addr_List[client_id], clilen) <= 0)
				{
					printf("%s: sendto error 001\n", progname);
					exit(4);
				}
			}
			else
			{
				printf("Creating file with name: %s\n", fileName);

				File_List[client_id] = fopen(fileName, "w+");

				if (File_List[client_id] == NULL)
				{
					printf("Couldn't create file: %s\n", fileName);
				}

				message.opCode = ACK;
				
				//if (sendto(sockfd, (void *)&message, 4, 0, &pcli_addr, clilen) <= 0)
				if (sendto(sockfd, (void *)&message, 4, 0, &Client_Addr_List[client_id], clilen) <= 0)
				{
					printf("%s: sendto error 002\n", progname);
					exit(4);
				}
			}
		}
		else if (message.opCode == DATA)
		{
			printf("Server: Received data of block: %d\n", message.block);

			int startIndex = (message.block - 1) * 512;

			if (File_List[client_id] != NULL)
			{
				fseek(File_List[client_id], startIndex, SEEK_SET);
				char possible_eof = message.data[bytes_received - 5];
				if (possible_eof == EOF)
				{
					fwrite(message.data, 1, bytes_received - 5, File_List[client_id]);
					printf("Last data packet received. Closing file 1. \n");
					fclose(File_List[client_id]);
					Client_Active[client_id] = 0;
					printf("Server: Freeing active client ID: %d \n", client_id);
				}
				else
				{
					fwrite(message.data, 1, MAXLINE, File_List[client_id]);
				}
			}
			message.opCode = ACK;

			//if (sendto(sockfd, (void *)&message, 4, 0, &pcli_addr, clilen) <= 0)
			if (sendto(sockfd, (void *)&message, 4, 0, &Client_Addr_List[client_id], clilen) <= 0)
			{
				printf("%s: sendto error\n", progname);
				exit(4);
			}
		}
		else if (message.opCode == RRQ)
		{
			Block_Count[client_id] = 1;
			printf("%s: received first RRQ \n", progname);
			int startIndex = 0;
			int c;
			int index = 0;
			while (1)
			{
				c = message.data[startIndex];
				if (c == 0)
				{
					break;
				}
				startIndex++;
				fileName[index] = c;
				index++;
			}
			fileName[index] = '\0';

			printf("Read data from the file with name: %s\n", fileName);
			File_List[client_id] = fopen(fileName, "r");
			if (File_List[client_id] == NULL)
			{
				printf("File doesn't exist: %s\n", fileName);
				printf("Sending error packet.\n");
				memset(message.data, 0, sizeof(message.data));
				message.opCode = ERROR;
				message.block = 1;
				char err_msg[] = "File not found.";
				strcpy(message.data, err_msg);
				Client_Active[client_id] = 0;
				printf("Server: Freeing active client ID: %d\n", client_id);
				if (sendto(sockfd, (void *)&message, sizeof(message), 0, &Client_Addr_List[client_id], clilen) <= 0)
				{
					printf("%s: sendto error\n", progname);
					exit(4);
				}

			}
			else
			{
				Message message;
				if (!feof(File_List[client_id]) && !ferror(File_List[client_id]))
				{
					/* Get length of file data (contents) */
					/* Getting the file contents. */

					// int datalen = fread(&message.data, sizeof(Message), 1, flptr);
					int datalen = 0;
					int packetlen = 0;

					memset(&message, 0, sizeof(Message));
					message.opCode = DATA;
					//	message.fileSize = size;
					message.block = Block_Count[client_id];
					int c;

					/* Read a max of 512 characters. */
					while ((datalen < MAXLINE) && (!feof(File_List[client_id])))
					{
						c = (char)fgetc(File_List[client_id]);
						message.data[datalen] = c;
						datalen++;
						if (c == EOF)
						{
							printf("File end encountered...This message will contain EOF\n");
							break;
						}
					}
					if (datalen <= 0)
					{
						printf("Error reading file.\n");
						fclose(File_List[client_id]);
						exit(4);
					}
					else
					{
						// packetlen is sum of datalen, 2 bytes of opcode and 2 bytes of block number.
						packetlen = datalen + 4;
					}

					//bytes_received = sendto_with_alarm(sockfd, (void *)&message, packetlen, 0, &Client_Addr_List[client_id], clilen, 3, &bytes_sent);
					bytes_sent = sendto(sockfd, (void *)&message, packetlen, 0, &Client_Addr_List[client_id], clilen);

					if (bytes_sent != packetlen)
					{
						printf("%s: recvfrom error 2 \n", progname);
						exit(3);
					}

					Block_Count[client_id]++;

				}
	
			}
		}
		else if (message.opCode == ERROR)
		{
			printf("%s: Error Code: %d\n", progname, message.block);
			printf("%s: Error Message: %s\n", progname, message.data);
			exit(5);
		}
		else if (message.opCode == ACK)
		{					
			int c;
			if (message.block == (Block_Count[client_id]-1))
			{
				printf("ack: %hu\n", message.block);
				Message message;
				if (!feof(File_List[client_id]) && !ferror(File_List[client_id]))
				{
					/* Get length of file data (contents) */
					/* Getting the file contents. */

					// int datalen = fread(&message.data, sizeof(Message), 1, flptr);
					int datalen = 0;
					int packetlen = 0;

					memset(&message, 0, sizeof(Message));
					message.opCode = DATA;
					//	message.fileSize = size;
					message.block = Block_Count[client_id];


					/* Read a max of 512 characters. */
					while ((datalen < MAXLINE) && (!feof(File_List[client_id])))
					{
						c = (char)fgetc(File_List[client_id]);
						message.data[datalen] = c;
						datalen++;
						if (c == EOF)
						{
							printf("File end encountered...This message will contain EOF\n");
							break;
						}
					}
					if (datalen <= 0)
					{
						printf("Error reading file.\n");
						fclose(File_List[client_id]);
						exit(4);
					}
					else
					{
						// packetlen is sum of datalen, 2 bytes of opcode and 2 bytes of block number.
						packetlen = datalen + 4;
					}

					//bytes_received = sendto_with_alarm(sockfd, (void *)&message, packetlen, 0, &Client_Addr_List[client_id], clilen, 3, &bytes_sent);
					bytes_sent = sendto(sockfd, (void *)&message, packetlen, 0, &Client_Addr_List[client_id], clilen);

					if (bytes_sent != packetlen)
					{
						printf("%s: recvfrom error 2 \n", progname);
						exit(3);
					}
				}
				else
				{
					fclose(File_List[client_id]);
					Client_Active[client_id] = 0;
					printf("Server: Freeing active client ID: %d\n", client_id);
				}
				
				Block_Count[client_id]++;
				
			}
			else
			{
				printf("%s: wrong ack error 001\n", progname);
				exit(5);
			}
		}
	}
}

/* Main driver program. Initializes server's socket and calls the  */
/* dg_echo function that never terminates.                         */

int main(int argc, char *argv[])
{


	/* argv[0] holds the program's name. We use this to label error    */
	/* reports.                                                        */
	int serverPort = SERV_UDP_PORT;
	
	progname = argv[0];
	// char *filename = (char *)malloc(20);
	// strcpy(filename, "file_from_client.txt");
	/* Overwrite the defaults if they are provided by the command line. */
	int i;
	for (i = 1; i < argc; i++)
	{
		//  printf("argv[%u] = %s\n", i, argv[i]);
		if (argv[i][0] == '-')
		{
			if (argv[i][1] == 'p')
			{
				serverPort = atoi(argv[i + 1]);
				i++;
			}
		}
	}
	/* General purpose socket structures are accessed using an         */
	/* integer handle.                                                 */

	// recv_file(sockfd);

	int sockfd;
	
	int optval;

	/* The Internet specific address structure. We must cast this into */
	/* a general purpose address structure when setting up the socket. */

	struct sockaddr_in serv_addr;
	
	/* Create a UDP socket (an Internet datagram socket). AF_INET      */
	/* means Internet protocols and SOCK_DGRAM means UDP. 0 is an      */
	/* unused flag byte. A negative value is returned on error.        */

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		printf("%s: can't open datagram socket\n", progname);
		exit(1);
	}

	optval = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int));

	printf("Opened socket: %d\n", sockfd);

	/* Abnormal termination using the exit call may return a specific  */
	/* integer error code to distinguish among different errors.       */

	/* To use the socket created, we must assign to it a local IP      */
	/* address and a UDP port number, so that the client can send data */
	/* to it. To do this, we fisrt prepare a sockaddr structure.       */

	/* The bzero function initializes the whole structure to zeroes.   */

	bzero((char *)&serv_addr, sizeof(serv_addr));

	/* As sockaddr is a general purpose structure, we must declare     */
	/* what type of address it holds.                                  */

	serv_addr.sin_family = AF_INET;

	/* If the server has multiple interfaces, it can accept calls from */
	/* any of them. Instead of using one of the server's addresses,    */
	/* we use INADDR_ANY to say that we will accept calls on any of    */
	/* the server's addresses. Note that we have to convert the host   */
	/* data representation to the network data representation.         */

	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	/* We must use a specific port for our server for the client to    */
	/* send data to (a well-known port).                               */

	serv_addr.sin_port = htons(serverPort);

	/* We initialize the socket pointed to by sockfd by binding to it  */
	/* the address and port information from serv_addr. Note that we   */
	/* must pass a general purpose structure rather than an Internet   */
	/* specific one to the bind call and also pass its size. A         */
	/* negative return value means an error occured.                   */

	if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
		printf("%s: can't bind local address\n", progname);
		exit(2);
	}

	/* We can now start the echo server's main loop. We only pass the  */
	/* local socket to dg_echo, as the client's data are included in   */
	/* all received datagrams.                                         */

	// dg_echo(sockfd);

	// receive_message(sockfd);
	printf("register timeout handler\n");
	
	if (register_handler() != 0)
	{
		printf("failed to register timeout\n");
	}

	memset(Client_Addr_List, 0, MAX_CLIENTS * sizeof(struct sockaddr));
	memset(File_List, 0, MAX_CLIENTS * sizeof(FILE*));
	memset(Client_Active, 0, MAX_CLIENTS * sizeof(int));
	memset(Block_Count, 0, MAX_CLIENTS * sizeof(int));
	
	pthread_t thread_id1;
	//pthread_t thread_id2;

	pthread_create(&thread_id1, NULL, myThreadFun, (void *)&sockfd);
	//pthread_create(&thread_id2, NULL, myThreadFun, (void *)&sockfd);
	
	pthread_join(thread_id1, NULL);
	//pthread_join(thread_id2, NULL);
	
	printf("Closing Server because threads have terminated. \n");

	/* The echo function in this example never terminates, so this     */
	/* code should be unreachable.                                     */
	return 0;
}
