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

}Message;

#endif
