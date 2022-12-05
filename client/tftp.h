#ifndef __TFTP_H__
#define __TFTP_H__

#define MAXMESG 2048
#define MAXLINE 512

//take from RFC 1350 - section 5 
          // opcode  operation
          //   1     Read request (RRQ)
          //   2     Write request (WRQ)
          //   3     Data (DATA)
          //   4     Acknowledgment (ACK)
          //   5     Error (ERROR)

typedef enum 
{
    RRQ = 1,
    WRQ,
    DATA,
    ACK, 
    ERROR
} Message_Type;

typedef struct 
{
  short int opCode;
  short int block;
  unsigned char data[512];
  int fileSize;


}Message;

extern int totalTimeOut;

ssize_t sendto_with_alarm(int socket, void *message, 
                          size_t length, int flags, 
						  const struct sockaddr *dest_addr,
           				  socklen_t dest_len,
						  int alarm_secs)
{
	ssize_t bytes_sent;

	while (totalTimeOut < 10)
	{
	 	bytes_sent = sendto(socket, message, length, flags, dest_addr, dest_len);

		alarm(alarm_secs);

		/* Wait for ack. */
		if (recvfrom(socket, message, sizeof(Message), 0, NULL, NULL) < 0)
		{
			printf("client recvfrom error\n");
			exit(4);
		}
		else
		{
			alarm(0);
			totalTimeOut = 0;
			break;
		}
	}

	return bytes_sent;
}						  


#endif
