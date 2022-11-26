
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

char *progname;

void recv_file(int sockfd)
{
	struct sockaddr pcli_addr;

	int bytes_received, clilen;
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

			// flptr = fopen(fileName, "r");
			// if (flptr != NULL)  //file already exists
			// {
			// 	message.opCode = ERROR;
			// 	message.data[0] = 6;
			// 	char ErrMsg[50] = "File already exists.";
			// 	int ErrMsgLength = strlen(ErrMsg);
			// 	memcpy(&message.data[1], ErrMsg, ErrMsgLength);
			// 	message.data[ErrMsgLength + 1] = 0;
			// 	int messageLength = ErrMsgLength + 1 + 5;
			// 	/* Send the ERROR packet. */
			// 	if (sendto(sockfd, (void *)&message, messageLength, 0, &pcli_addr, clilen != messageLength))
			// 	{
			// 		printf("%s: sendto error on ERROR packet \n", progname);
			// 		exit(3);
			// 	}
			// 	exit(3);
			// }
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
		else if (message.opCode == DATA)
		{
			printf("%d: received data of block \n", message.block);
			int startIndex = (message.block - 1) * 512;
			if (flptr != NULL)
			{
				fseek(flptr, startIndex, SEEK_SET);
				// if(message.fileSize == startIndex + bytes_received - 4)
				// {
				// 	fwrite(message.data, 1, bytes_received - 5, flptr);
				// 	printf("Last data packet received. Closing file 1. \n");
				// 	fclose(flptr);
				// } else {
				// 	fwrite(message.data, 1, MAXLINE, flptr);
				// }
				if (message.data[bytes_received - 4] == EOF)
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
			// if (bytes_received < 516)
			// {
			// 	printf("Last data packet received. Closing file 2.  \n");
			// 	fclose(flptr);
			// }
		}
		else if (message.opCode == RRQ)
		{
			int Block = 0;
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
			flptr = fopen(fileName, "r");
			if (flptr == NULL)
			{
				printf("Couldn't read data. File doesn't exist: %s\n", fileName);
			}
			// else
			// {
			// 	printf("File succesfully created: %s\n", fileName);
			// }
			/* Copy the contents of the file into sendLine. */
			while (!feof(flptr) && !ferror(flptr))
			{
				/* Get length of file data (contents) */
				/* Getting the file contents. */

				// int datalen = fread(&message.data, sizeof(Message), 1, flptr);
				int datalen = 0;
				int packetlen = 0;
				Message message;
				memset(&message, 0, sizeof(Message));
				message.opCode = DATA;
				//	message.fileSize = size;
				message.block = Block;
				int c;

				/* Read a max of 512 characters. */
				while ((datalen < MAXLINE) && (!feof(flptr)))
				{
					c = (char)fgetc(flptr);
					if (c == EOF)
					{
						break;
					}
					message.data[datalen] = c;
					datalen++;

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
					if (sendto(sockfd, (void *)&message, packetlen, 0, &pcli_addr, clilen) != packetlen)
					{
						printf("%s: sendto error on socket\n", progname);
						fclose(flptr);
						exit(3);
					}

					/* Wait for ack. */
					if (recvfrom(sockfd, (void *)&message, sizeof(Message), 0, &pcli_addr, &clilen) < 0)
					{
						printf("%s: recvfrom error\n", progname);
						exit(4);
					}
					if (message.opCode == ACK && message.block == Block)
					{
						printf("ack: %hu\n", message.block);
					}
				}
				// else
				// {
				// 	printf("%s: wrong ack error\n", progname);
				// 	exit(5);
				// }

				Block++;
			}

			fclose(flptr);
		}
		else if (message.opCode == ERROR)
		{
			// Do we terminate when an error is received.
		}
	}
}

/* Main driver program. Initializes server's socket and calls the  */
/* dg_echo function that never terminates.                         */

int main(int argc, char *argv[])
{

	/* General purpose socket structures are accessed using an         */
	/* integer handle.                                                 */

	int sockfd;
	int serverPort = SERV_UDP_PORT;

	/* The Internet specific address structure. We must cast this into */
	/* a general purpose address structure when setting up the socket. */

	struct sockaddr_in serv_addr;

	/* argv[0] holds the program's name. We use this to label error    */
	/* reports.                                                        */

	progname = argv[0];
	if (argc == 3)
	{
		if (strcmp(argv[1], "-p") == 0)
		{
			serverPort = atoi(argv[2]);
		}
	}
	printf("%s: using port %d\n", progname, serverPort);
	/* Create a UDP socket (an Internet datagram socket). AF_INET      */
	/* means Internet protocols and SOCK_DGRAM means UDP. 0 is an      */
	/* unused flag byte. A negative value is returned on error.        */

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		printf("%s: can't open datagram socket\n", progname);
		exit(1);
	}

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

	recv_file(sockfd);

	/* The echo function in this example never terminates, so this     */
	/* code should be unreachable.                                     */
	return 0;
}
