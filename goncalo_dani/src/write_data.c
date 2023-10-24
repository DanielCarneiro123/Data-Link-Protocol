#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <math.h>
#include "write_data.h"

unsigned char *controlPacket(const unsigned int c, const char* file_name, long int length, unsigned int* control_size){

    int L1 = (int) ceil(log2f((float)length)/8.0);
    int L2 = strlen(file_name);
    *control_size = 1+2+2+L1+L2;
    unsigned char *frame = (unsigned char*)malloc(*control_size);
    
    unsigned int i = 0;
    frame[i++]=c;
    frame[i++]=0;
    frame[i++]=L1;

    for (unsigned char i = 0 ; i < L1 ; i++) {
        frame[2+L1-i] = length & 0xFF;
        length >>= 8;
    }

    i+=L1;
    frame[i++]=1;
    frame[i++]=L2;
    memcpy(frame+i, file_name, L2);
    return frame;
}

unsigned char *dataPacket(unsigned char sequence, unsigned char *data, int dataSize, int *frame_size){

    *frame_size = 1 + 1 + 2 + dataSize;
    unsigned char* frame = (unsigned char*)malloc(*frame_size);

    frame[0] = 1;   
    frame[1] = sequence;
    frame[2] = dataSize >> 8 & 0xFF;
    frame[3] = dataSize & 0xFF;
    memcpy(frame+4, data, dataSize);

    return frame;
}

unsigned char *data(FILE *fd, long int fileLength)
{
    if (fd == NULL || fileLength <= 0) {
        return NULL;
    }

    unsigned char *content = (unsigned char *)malloc(fileLength);
    
    if (content == NULL) {
        return NULL;
    }

    size_t bytesRead = fread(content, sizeof(unsigned char), fileLength, fd);

    if (bytesRead != fileLength) {
        free(content); 
        return NULL;
    }

    return content;
}