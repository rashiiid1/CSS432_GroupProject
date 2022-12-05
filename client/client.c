

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
int totalTimeOut = 0;

#define MAXDATASIZE 2000
const static int DATA_OFFSET = 4;
static int Block = 0;
/*
 * This function handles the timeout by incrementing
 * the corresponding global variables.
 *
 * Input:  signal number.
 * Return: None.
 *
 */
void handle_timeout(int signum)
{
	printf("timeout!\n");

	totalTimeOut++;
	printf("Alarm Fired %d times\n", totalTimeOut);

}

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

// ssize_t sendto_with_alarm(int socket, void *message, 
//                           size_t length, int flags, 
// 						  const struct sockaddr *dest_addr,
//            				  socklen_t dest_len,
// 						  int alarm_secs)
// {
// 	ssize_t bytes_sent;

// 	while (totalTimeOut < 10)
// 	{
// 	 	bytes_sent = sendto(socket, message, length, flags, dest_addr, dest_len);

// 		alarm(alarm_secs);

// 		/* Wait for ack. */
// 		if (recvfrom(socket, message, sizeof(Message), 0, NULL, NULL) < 0)
// 		{
// 			printf("%s: recvfrom error\n", progname);
// 			exit(4);
// 		}
// 		else
// 		{
// 			alarm(0);
// 			totalTimeOut = 0;
// 			break;
// 		}
// 	}

// 	return bytes_sent;
// }						  

 



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
		if (sendto_with_alarm(sockfd, (void *)&message, messageLength, 0, pserv_addr, servlen, 3) != messageLength)
		{
			printf("%s: sendto error on RRQ_or_WRQ packet \n", progname);
			exit(3);
		}

		/* Check if ACK was received. */
		if (message.opCode == ACK && message.block == Block)
		{
			printf("ack: %hu\n", message.block);
		}
		else if (message.opCode == ERROR)
		{
			printf("%s: Error Code: %d\n", progname, message.block);
			printf("%s: Error Message: %s\n", progname, message.data);
			exit(5);
		}
		else
		{
			printf("%s: wrong ack error\n", progname);
			exit(5);
		}
		Block++;

		/* Open the file. */
		FILE *flptr = fopen(filename, "r");
		fseek(flptr, 0, SEEK_SET);
		
		/* Copy the contents of the file into sendLine. */

		while (!feof(flptr) && !ferror(flptr))
		{
			/* Get length of file data (contents) */
			/* Getting the file contents. */

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
			
			/* Send the WRQ packet. */
			if (sendto_with_alarm(sockfd, (void *)&message, packetlen, 0, pserv_addr, servlen, 3) != packetlen)
			{
				printf("%s: ssendto error on socket \n", progname);
				exit(3);
			}

			/* Last packet sent */
			if (packetlen < 516)
			{
				printf("%s: Last packet is sent\n", progname);
				
			}
			
			if (message.opCode == ACK && message.block == Block)
			{
				printf("ack: %hu\n", message.block);
			}
			Block++;
		}
		fclose(flptr);
		//}
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
		
		
		/* Received first data block */
		int bytes_received;
		
		bytes_received = sendto_with_alarm(sockfd, (void *)&message, messageLength, 0, pserv_addr, servlen, 3);

		if (bytes_received != messageLength)
		{
			printf("%s: recvfrom error 1\n", progname);
			exit(3);
		}


		/* Open the file to write. */
		FILE *flptr = fopen(filename, "w+");
		while (1)
		{
			Block++;
			/* Check if data was received. */
			if (message.opCode == DATA && message.block == Block)
			{
				printf("ack: %hu\n", message.block);
			}
			else if (message.opCode == ERROR)
			{
				printf("%s: Error Code: %d\n", progname, message.block);
				printf("%s: Error Message: %s\n", progname, message.data);
				exit(5);
			}
			else
			{
				printf("%s: wrong data error\n", progname);
				exit(5);
			}

			printf("%d: received data of block \n", message.block);
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
					break;
				}
				else
				{
					fwrite(message.data, 1, MAXLINE, flptr);
				}
			}
			message.opCode = ACK;
			
			bytes_received = sendto_with_alarm(sockfd, (void *)&message, 4, 0, pserv_addr, servlen, 3);

			if (bytes_received <= 0)
			{
				printf("%s: Client is unable to send acknowledgement. sendto error \n", progname);
				exit(4);
			}
			else
			{
				printf("block: %d bytes_received: %d\n", Block, bytes_received);
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

	char *filename = (char *)malloc(sizeof(char) * 50);
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
			if (argv[i][1] == 'r')
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
	
	printf("register timeout handler\n");
	
	if (register_handler() != 0)
	{
		printf("failed to register timeout\n");
	}
	else
	{
		printf("register timeout success \n");
	}
	
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
