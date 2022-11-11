

// Sample program at client side for echo transmit-receive - CSS 432 - Autumn 2022

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

// client

#define SERV_UDP_PORT 51527		   // REPLACE WITH YOUR PORT NUMBER
#define SERV_HOST_ADDR "127.0.0.1" // REPLACE WITH SERVER IP ADDRESS

/* A pointer to the name of this program for error reporting.      */

char *progname;

#define MAXDATASIZE 2000
const static int DATA_OFFSET = 4;
static int Block = 0;

void request_WRQ(int sockfd, struct sockaddr *pserv_addr, int servlen, unsigned short int opCode, char *filename, char *file_mode)
{
	int fileNameLength = strlen(filename);
	int fileModeLength = strlen(file_mode);
	Message message;
	memset(&message, 0, sizeof(Message));
	// write request
	if (opCode == WRQ)
	{
		message.opCode = opCode;
		memcpy(&message.data, filename, fileNameLength);
		message.data[fileNameLength] = 0;
		memcpy(&message.data[fileNameLength + 1], file_mode, fileModeLength);
		message.data[fileNameLength + 1 + fileModeLength] = 0;
		message.block = Block;
		/* Send the WRQ packet. */
		if (sendto(sockfd, (void *)&message, sizeof(Message), 0, pserv_addr, servlen) != sizeof(Message))
		{
			printf("%s: sendto error on RRQ_or_WRQ packet \n", progname);
			exit(3);
		}
		/* Wait for ack. */
		if (recvfrom(sockfd, (void *)&message, sizeof(Message), 0, NULL, NULL) < 0)
		{
			printf("%s: recvfrom error\n", progname);
			exit(4);
		}
		/* Check if ACK was received. */
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

		/* Open the file. */
		FILE *flptr = fopen(filename, "r");

		if (flptr == NULL)
		{
			printf("Error opening file.\n");
			exit(4);
		}

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

			message.block = Block;

			int c;
			while ((datalen < MAXLINE) && (!feof(flptr)))
			{
				c = (char)fgetc(flptr);
				if (c == EOF)
				{
					break;
				}
				message.data[datalen] = c;
				datalen++;
			}

			if (datalen <= 0)
			{
				printf("Error reading file.\n");
				fclose(flptr);
				exit(4);
			}
			else {
				// packetlen is sum of datalen, 2 bytes of opcode and 2 bytes of block number. 
				packetlen = datalen + 4;
			}
				  	/* Send the data packet. */
	  	if (sendto(sockfd, (void *)&message, packetlen, 0, pserv_addr, servlen) != packetlen)
		{
			printf("%s: sendto error on socket\n",progname);
			fclose(flptr);
			exit(3);
		}

			/* Wait for ack. */
		if (recvfrom(sockfd, (void *)&message, sizeof(Message), 0, NULL, NULL) < 0)
		{
			printf("%s: recvfrom error\n",progname);
			exit(4);
		}
		if(message.opCode ==  ACK && message.block == Block) {
			printf("ack: %hu\n", message.block);
		} else {
			printf("%s: wrong ack error\n",progname);
			exit(5);
		}
		
		Block++;
		}
		fclose(flptr);
	}
}

	// }
	/* The main program sets up the local socket for communication     */
	/* and the server's address and port (well-known numbers) before   */
	/* calling the dg_cli main loop.                                   */

	int main(int argc, char *argv[])
	{
		int sockfd;

		/* We need to set up two addresses, one for the client and one for */
		/* the server.                                                     */

		struct sockaddr_in cli_addr, serv_addr;
		progname = argv[0];

		/* Initialize first the server's data with the well-known numbers. */

		bzero((char *)&serv_addr, sizeof(serv_addr));
		serv_addr.sin_family = AF_INET;

		/* The system needs a 32 bit integer as an Internet address, so we */
		/* use inet_addr to convert the dotted decimal notation to it.     */

		serv_addr.sin_addr.s_addr = inet_addr(SERV_HOST_ADDR);
		serv_addr.sin_port = htons(SERV_UDP_PORT);

		/* Create the socket for the client side.                          */

		if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		{
			printf("%s: can't open datagram socket\n", progname);
			exit(1);
		}

		printf("Client got socket %d\n", sockfd);

		/* Initialize the structure holding the local address data to      */
		/* bind to the socket.                                             */

		bzero((char *)&cli_addr, sizeof(cli_addr));
		cli_addr.sin_family = AF_INET;

		/* Let the system choose one of its addresses for you. You can     */
		/* use a fixed address as for the server.                          */

		cli_addr.sin_addr.s_addr = htonl(INADDR_ANY);

		/* The client can also choose any port for itself (it is not       */
		/* well-known). Using 0 here lets the system allocate any free     */
		/* port to our program.                                            */

		cli_addr.sin_port = htons(0);

		/* The initialized address structure can be now associated with    */
		/* the socket we created. Note that we use a different parameter   */
		/* to exit() for each type of error, so that shell scripts calling */
		/* this program can find out how it was terminated. The number is  */
		/* up to you, the only convention is that 0 means success.         */

		if (bind(sockfd, (struct sockaddr *)&cli_addr, sizeof(cli_addr)) < 0)
		{
			printf("%s: can't bind local address\n", progname);
			exit(2);
		}

		printf("Bind successful\n");

		/* Call the main client loop. We need to pass the socket to use    */
		/* on the local endpoint, and the server's data that we already    */
		/* set up, so that communication can start from the client.        */

		// printf("If requesting read enter 1. If requesting write enter 2");
		// unsigned short opcode;
		//  scanf("%hu", &opcode);
		//  //printf("You requested %d \n",opcode);
		//  printf("opcode: %hu\n", opcode);
		//  if(opcode == 1) {
		// 		send_RRQ();
		//  }

		//  if(opcode == 2) {
		// 	//send out a WRQ
		// 	send_WRQ();
		// 	// receive ack #1

		// 	//send out data block#1
		//  }
		unsigned short int opcode_WRQ = 2;
		unsigned short int opcode_RRQ = 1;
		request_WRQ(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr), opcode_WRQ, "file_from_client.txt", "mode");
		// request_RRQ(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr), opcode_RRQ, "file_to_send.txt", "mode");
		// send_RRQ_or_WRQ_Packet(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr), opcode, "file_to_send.txt", "mode");
		//  send_Data_Packet(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr), opcode, "file_to_send.txt");
		/* We return here after the client sees the EOF and terminates.    */
		/* We can now release the socket and exit normally.                */

		close(sockfd);
		exit(0);
	}
