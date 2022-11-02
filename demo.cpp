//
//  main.cpp
//  TFTP
//
//  Created by B Pan on 10/24/22.
//

#include <iostream>
#include <string.h>
using namespace std;

const static int MAX_BUFFER_SIZE = 516;
const static unsigned short OP_CODE_DATA = 3;
const static int DATA_OFFSET = 4;
int main(int argc, const char * argv[]) {
    // insert code here...
    std::cout << "Hello, World!\n";
    
    // sender
    char buffer[MAX_BUFFER_SIZE];
    bzero(buffer, sizeof(buffer));
    
    unsigned short opCode = OP_CODE_DATA;
    
    cout<< "size of unsigned short: " << sizeof(opCode) << " bytes." << endl;
    
    unsigned short *opCodePtr = (unsigned short*) buffer;
    *opCodePtr = htons(OP_CODE_DATA);
    //*opCodePtr = OP_CODE_DATA;
    opCodePtr++; //pointing to 3rd byte.
    
    unsigned short blockNum = 1;
    unsigned short *blockNumPtr = opCodePtr;
    *blockNumPtr = htons(blockNum);
    
    
    char *fileData = buffer + DATA_OFFSET;
    char file[] = "This is a demo";
    strncpy (fileData, file, strlen(file)); // bcopy , memcpy
    
    //print the first 30 bytes of the buffer
    for ( int i = 0; i < 30; i++ ) {
        printf("0x%X,", buffer[i]);
    }
    cout<<endl<< "*****Receiver side*****"<<endl;
    
    // Receiver ;
    unsigned short *opCodePtrRcv = (unsigned short*) buffer;
    unsigned short opCodeRcv = ntohs(*opCodePtrRcv);
    
    cout<< "received opcode is: " << opCodeRcv <<endl;
    
    // parse block num
    // parse file data
    
    
    cout<<endl;
    return 0;
}