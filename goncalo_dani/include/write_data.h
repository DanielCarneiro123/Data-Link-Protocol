#ifndef _WRITE_DATA_H_
#define _WRITE_DATA_H_

#include <stdio.h>

unsigned char * controlPacket(const unsigned int c, const char* filename, long int length, unsigned int* size);

unsigned char * dataPacket(unsigned char sequence, unsigned char *data, int dataSize, int *packetSize);

unsigned char * data(FILE* fd, long int fileLength);

#endif

