

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

void request(int sockfd, struct sockaddr *pserv_addr, int servlen, unsigned short int opCode, char *filename, char *file_mode)
{
	int fileNameLength = strlen(filename);
	int fileModeLength = strlen(file_mode);
	int messageLength;
	Message message;
	memset(&message, 0, sizeof(Message));
	// write request
	if (opCode == WRQ)
	{
		Block = 0;
		message.opCode = opCode;
		memcpy(&message.data, filename, fileNameLength);
		message.data[fileNameLength] = 0;
		memcpy(&message.data[fileNameLength + 1], file_mode, fileModeLength);
		message.data[fileNameLength + 1 + fileModeLength] = 0;
		message.block = Block;
		messageLength = fileNameLength + 1 + fileModeLength + 4;
		/* Send the WRQ packet. */
		if (sendto(sockfd, (void *)&message, messageLength, 0, pserv_addr, servlen) != messageLength)
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
		// else
		// {
		// 	printf("%s: wrong ack error\n", progname);
		// 	exit(5);
		// }
		Block++;

		/* Open the file. */
		FILE *flptr = fopen(filename, "r");
		fseek(flptr, 0L, SEEK_END);
		int size = ftell(flptr);
		message.fileSize = size;
		fseek(flptr, 0, SEEK_SET);
		printf("%d: message.fileSize \n", message.fileSize);
		if (flptr == NULL)
		{
			message.opCode = ERROR;
			message.data[0] = 1;
			char ErrMsg[50] = "File Not found";
			int ErrMsgLength = strlen(ErrMsg);
			memcpy(&message.data[1], ErrMsg, ErrMsgLength);
			message.data[ErrMsgLength + 1] = 0;
			messageLength = ErrMsgLength + 1 + 5;
			/* Send the ERROR packet. */
			if (sendto(sockfd, (void *)&message, messageLength, 0, pserv_addr, servlen) != messageLength)
			{
				printf("%s: sendto error on ERROR packet \n", progname);
				exit(3);
			}
			exit(3);
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
			message.fileSize = size;
			message.block = Block;

			int c;
			while ((datalen < MAXLINE) && (!feof(flptr)))
			{
				c = (char)fgetc(flptr);
				message.data[datalen] = c;
				datalen++;
				if (c == EOF)
				{
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
			if (sendto(sockfd, (void *)&message, packetlen, 0, pserv_addr, servlen) <= 0)
			{
				printf("%s: sendto error on socket\n", progname);
				exit(3);
			}
			/* Last packet sent */
			if (packetlen < 516)
			{
				printf("%s: Last packet is sent\n", progname);
				// fclose(flptr);
				// exit(4);
			}

			/* Wait for ack. */
			if (recvfrom(sockfd, (void *)&message, sizeof(Message), 0, NULL, NULL) < 0)
			{
				printf("%s: recvfrom error\n", progname);
				exit(4);
			}
			if (message.opCode == ACK && message.block == Block)
			{
				printf("ack: %hu\n", message.block);
			}
			// else
			// {
			// 	printf("%s: wrong ack error 2 \n", progname);
			// 	exit(5);
			// }

			Block++;
		}
		fclose(flptr);
	}
	// read request
	else if (opCode == RRQ)
	{
		Block = 0;
		message.opCode = opCode;
		memcpy(&message.data, filename, fileNameLength);
		message.data[fileNameLength] = 0;
		memcpy(&message.data[fileNameLength + 1], file_mode, fileModeLength);
		message.data[fileNameLength + 1 + fileModeLength] = 0;
		messageLength = fileNameLength + 1 + fileModeLength + 4;
		message.block = Block;
		/* Send the RRQ packet. */
		if (sendto(sockfd, (void *)&message, messageLength, 0, pserv_addr, servlen) != messageLength)
		{
			printf("%s: sendto error on RRQ_or_WRQ packet \n", progname);
			exit(3);
		}
		/* Open the file to write. */
		FILE *flptr = fopen(filename, "w+");
		while (1)
		{
			/* Received first data block */
			int bytes_received;
			bytes_received = recvfrom(sockfd, (void *)&message, sizeof(Message), 0, NULL, NULL);
			if (bytes_received <= 0)
			{
				printf("%s: recvfrom error\n", progname);
				exit(4);
			}
			Block++;
			/* Check if data was received. */
			if (message.opCode == DATA && message.block == Block)
			{
				printf("ack: %hu\n", message.block);
			}
			// else
			// {
			// 	printf("%s: wrong data error\n", progname);
			// 	exit(5);
			// }

			printf("%d: received data of block \n", message.block);
			int startIndex = (message.block - 1) * 512;
			if (flptr != NULL)
			{
				fseek(flptr, startIndex, SEEK_SET);
				fwrite(message.data, 1, bytes_received - 4, flptr);
			}
			// else
			// {
			// 	printf("%s: can't open file 2\n", progname);
			// 	exit(5);
			// }
			if (bytes_received < 516)
			{
				printf("%s: end of file. closing file 2\n", progname);
				fclose(flptr);
			}
			message.opCode = ACK;
			if (sendto(sockfd, (void *)&message, 4, 0, pserv_addr, servlen) <= 0)
			{
				printf("%s: sendto error 2\n", progname);
				exit(4);
			}
		}
	}
}


// }
/* The main program sets up the local socket for communication     */
/* and the server's address and port (well-known numbers) before   */
/* calling the dg_cli main loop.                                   */

int main(int argc, char *argv[])
{
	int sockfd;

	int serverPort = SERV_UDP_PORT;
	// read = 1
	// write = 0
	int r_or_write = 1;

	char *filename = (char *)malloc(20);
	strcpy(filename, "file_from_client.txt");
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
			if (argv[i][0] == 'r')
			{
				r_or_write = 1;
				strcpy(filename, argv[i + 1]);
				i++;
			}
			if (argv[i][1] == 'w')
			{
				r_or_write = 0;
				strcpy(filename, argv[i + 1]);
				i++;
			}
		}
	}

	/* We need to set up two addresses, one for the client and one for */
	/* the server.                                                     */

	struct sockaddr_in cli_addr, serv_addr;
	progname = argv[0];

	printf("%s: using port %d\n", progname, serverPort);
	/* Initialize first the server's data with the well-known numbers. */

	bzero((char *)&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;

	/* The system needs a 32 bit integer as an Internet address, so we */
	/* use inet_addr to convert the dotted decimal notation to it.     */

	serv_addr.sin_addr.s_addr = inet_addr(SERV_HOST_ADDR);
	serv_addr.sin_port = htons(serverPort);

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
	// if(argc == 2) {

	// }
	// printf("arg %d - %s\n", 0, argv[0]);
	// printf("arg %d - %s\n", 1, argv[1]);
	// printf("arg %d - %s\n", 2, argv[2]);
	// if(argv[1] =="-r") {

	// 	 request(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr), RRQ, argv[2], "mode");
	// }]
	// printf("Client got socket %d\n", sockfd);
	if (r_or_write == 0)
	{
		request(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr), WRQ, filename, "mode");
	}
	else
	{
		request(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr), RRQ, filename, "mode");
	}

	close(sockfd);
	exit(0);
}
