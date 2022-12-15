#ifndef __TFTP_H__
#define __TFTP_H__

#define MAXMESG 2048
#define MAXLINE 512

#include <pthread.h>

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

// typedef struct thread_data
// {
// 	int id;
// 	int socket;
// 	struct sockaddr *client_addr;
// } THREAD_DATA;

#define MAX_CLIENTS 10
struct sockaddr Client_Addr_List[MAX_CLIENTS];
FILE* File_List[MAX_CLIENTS];
int Client_Active[MAX_CLIENTS];
int Block_Count[MAX_CLIENTS];


extern int totalTimeOut;

#define ENABLE_ALARM

ssize_t sendto_with_alarm(int socket, void *message, 
                          size_t length, int flags, 
						  const struct sockaddr *dest_addr,
           				  socklen_t dest_len,
						  int alarm_secs,
						  ssize_t *bytes__sent)
{
	ssize_t bytes_sent;
	ssize_t bytes_received;

	while (totalTimeOut < 10)
	{
	 	bytes_sent = sendto(socket, message, length, flags, dest_addr, dest_len);

		if (bytes_sent < 0)
		{
			printf("Server: sendto error in sendto_with_alarm\n");
			exit(4);
		}

		*bytes__sent = bytes_sent;

#ifdef ENABLE_ALARM
		alarm(alarm_secs);
#endif
		/* Wait for ack. */
		bytes_received = recvfrom(socket, message, sizeof(Message), 0, NULL, NULL);

		if (bytes_received < 0)
		{
			printf("Server: recvfrom error in sendto_with_alarm\n");
			//exit(4);
		}
		else
		{
#ifdef ENABLE_ALARM
			alarm(0);
#endif
			totalTimeOut = 0;
			break;
		}
	}

	if (totalTimeOut >= 10)
	{
		printf("Connection with peer has stopped. \n");
		exit(6);
	}

	return bytes_received;
}						  

int sockaddr_cmp(struct sockaddr *x, struct sockaddr *y)
{
#define CMP(a, b) if (a != b) return a < b ? -1 : 1

    CMP(x->sa_family, y->sa_family);

    if (x->sa_family == AF_INET) {
        struct sockaddr_in *xin = (void*)x, *yin = (void*)y;
        CMP(ntohl(xin->sin_addr.s_addr), ntohl(yin->sin_addr.s_addr));
        CMP(ntohs(xin->sin_port), ntohs(yin->sin_port));
    } else if (x->sa_family == AF_INET6) {
        struct sockaddr_in6 *xin6 = (void*)x, *yin6 = (void*)y;
        int r = memcmp(xin6->sin6_addr.s6_addr, yin6->sin6_addr.s6_addr, sizeof(xin6->sin6_addr.s6_addr));
        if (r != 0)
            return r;
        CMP(ntohs(xin6->sin6_port), ntohs(yin6->sin6_port));
        CMP(xin6->sin6_flowinfo, yin6->sin6_flowinfo);
        CMP(xin6->sin6_scope_id, yin6->sin6_scope_id);
	}
#undef CMP
    return 0;
}

#endif
