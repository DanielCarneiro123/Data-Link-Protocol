#ifndef _READ_DATA_H_
#define _READ_DATA_H_

#include <stdio.h>

unsigned char* readControlPacket(unsigned char* packet, int size, unsigned long int *fileSize);

void readDataPacket(const unsigned char* packet, const unsigned int packetSize, unsigned char* buffer);

#endif