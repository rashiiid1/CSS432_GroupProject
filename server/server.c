
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
	THREAD_DATA *pData = (THREAD_DATA*)vargp;

	int serverPort = pData->port;

	int sockfd;
	
	int optval;

	/* The Internet specific address structure. We must cast this into */
	/* a general purpose address structure when setting up the socket. */

	struct sockaddr_in serv_addr;
	
	printf("%s: Thread %d using port %d\n", progname, pData->thread_number, serverPort);
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

	printf("[%d] Opened socket: %d\n", pData->thread_number, sockfd);

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

	printf("Thread started with socket %d\n", sockfd);

	struct sockaddr pcli_addr;

	int bytes_received, clilen, bytes_sent;
	Message message;
	FILE *flptr = NULL;

	while (1)
	{
		clilen = sizeof(struct sockaddr);

		bytes_received = recvfrom(sockfd, (void *)&message, sizeof(Message), 0, &pcli_addr, &clilen);

		if (bytes_received < 0)
		{
			printf("%s: recvfrom error\n", progname);
			exit(3);
		}

		char fileName[20];
		if (message.opCode == WRQ)
		{
			printf("%s: [%d] received first WRQ \n", progname, pData->thread_number);
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

			flptr = fopen(fileName, "r");
			if (flptr != NULL) // file already exists
			{
				printf("File already exists: %s\n", fileName);
				memset(message.data, 0, sizeof(message.data));
				message.opCode = ERROR;
				message.block = 6;
				char err_msg[] = "File already exists.";
				strcpy(message.data, err_msg);
				/* Send the ERROR packet. */
				if (sendto(sockfd, (void *)&message, sizeof(message), 0, &pcli_addr, clilen) <= 0)
				{
					printf("%s: sendto error\n", progname);
					exit(4);
				}
			}
			else
			{
				printf("Creating file with name: %s\n", fileName);
				flptr = fopen(fileName, "w+");
				if (flptr == NULL)
				{
					printf("Couldn't create file: %s\n", fileName);
				}
				message.opCode = ACK;
				if (sendto(sockfd, (void *)&message, 4, 0, &pcli_addr, clilen) <= 0)
				{
					printf("%s: sendto error\n", progname);
					exit(4);
				}
			}
		}
		else if (message.opCode == DATA)
		{
			printf("%d: [%d] received data of block \n", message.block, pData->thread_number);
			int startIndex = (message.block - 1) * 512;
			if (flptr != NULL)
			{
				fseek(flptr, startIndex, SEEK_SET);
				char possible_eof = message.data[bytes_received - 5];
				if (possible_eof == EOF)
				{
					fwrite(message.data, 1, bytes_received - 5, flptr);
					printf("Last data packet received. Closing file 1. \n");
					fclose(flptr);
				}
				else
				{
					fwrite(message.data, 1, MAXLINE, flptr);
				}
			}
			message.opCode = ACK;
			if (sendto(sockfd, (void *)&message, 4, 0, &pcli_addr, clilen) <= 0)
			{
				printf("%s: sendto error\n", progname);
				exit(4);
			}
		}
		else if (message.opCode == RRQ)
		{
			int Block = 1;
			printf("%s: [%d] received first RRQ \n", progname, pData->thread_number);
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
			flptr = fopen(fileName, "r");
			if (flptr == NULL)
			{
				printf("File doesn't exist: %s\n", fileName);
				printf("Sending error packet.\n");
				memset(message.data, 0, sizeof(message.data));
				message.opCode = ERROR;
				message.block = 1;
				char err_msg[] = "File not found.";
				strcpy(message.data, err_msg);
				if (sendto(sockfd, (void *)&message, sizeof(message), 0, &pcli_addr, clilen) <= 0)
				{
					printf("%s: sendto error\n", progname);
					exit(4);
				}
			}
			else
			{
				Message message;
				while (!feof(flptr) && !ferror(flptr))
				{
					/* Get length of file data (contents) */
					/* Getting the file contents. */

					// int datalen = fread(&message.data, sizeof(Message), 1, flptr);
					int datalen = 0;
					int packetlen = 0;

					memset(&message, 0, sizeof(Message));
					message.opCode = DATA;
					//	message.fileSize = size;
					message.block = Block;
					int c;

					/* Read a max of 512 characters. */
					while ((datalen < MAXLINE) && (!feof(flptr)))
					{
						c = (char)fgetc(flptr);
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
						fclose(flptr);
						exit(4);
					}
					else
					{
						// packetlen is sum of datalen, 2 bytes of opcode and 2 bytes of block number.
						packetlen = datalen + 4;
					}

					/* Send the data packet. */
					// if (sendto(sockfd, (void *)&message, packetlen, 0, &pcli_addr, clilen) != packetlen)
					// {
					// 	printf("%s: sendto error on socket\n", progname);
					// 	fclose(flptr);
					// 	exit(3);
					// }
					// /* Wait for ack. */
					// if (recvfrom(sockfd, (void *)&message, sizeof(Message), 0, &pcli_addr, &clilen) < 0)
					// {
					// 	printf("%s: recvfrom error\n", progname);
					// 	exit(4);
					// }
					bytes_received = sendto_with_alarm(sockfd, (void *)&message, packetlen, 0, &pcli_addr, clilen, 3, &bytes_sent);

					if (bytes_sent != packetlen)
					{
						printf("%s: recvfrom error 2 \n", progname);
						exit(3);
					}

					if (message.opCode == ACK && message.block == Block)
					{
						printf("ack: %hu\n", message.block);
					}
					else
					{
						printf("%s: wrong ack error\n", progname);
						exit(5);
					}
					Block++;
				}
				fclose(flptr);
			} // if (flptr == NULL)
		}
		else if (message.opCode == ERROR)
		{
			printf("%s: Error Code: %d\n", progname, message.block);
			printf("%s: Error Message: %s\n", progname, message.data);
			exit(5);
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

	THREAD_DATA data[2];

	data[0].port = serverPort;
	data[1].port = serverPort;

	data[0].thread_number = 0;
	data[1].thread_number = 1;
	

	pthread_t thread_id1, thread_id2;
	pthread_create(&thread_id1, NULL, myThreadFun, (void *)&data[0]);
	pthread_create(&thread_id2, NULL, myThreadFun, (void *)&data[1]);
	pthread_join(thread_id1, NULL);
	pthread_join(thread_id2, NULL);
	printf("Closing Server because threads have terminated. \n");

	/* The echo function in this example never terminates, so this     */
	/* code should be unreachable.                                     */
	return 0;
}
