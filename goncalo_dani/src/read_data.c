#include "read_data.h"
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

unsigned char *readControlPacket(unsigned char* packet, int size, unsigned long int *fileSize) {

    
    unsigned char fileBytes = packet[2];
    unsigned char fileSizeAux[fileBytes];
    memcpy(fileSizeAux, packet+3, fileBytes);
    for(unsigned int i = 0; i < fileBytes; i++)
        *fileSize |= (fileSizeAux[fileBytes-i-1] << (8*i));

    
    unsigned char fileNameNBytes = packet[3+fileBytes+1];
    unsigned char *name = (unsigned char*)malloc(fileNameNBytes);
    memcpy(name, packet+3+fileBytes+2, fileNameNBytes);
    return name;
}

void readDataPacket(const unsigned char* packet, const unsigned int packetSize, unsigned char* buffer) {
    memcpy(buffer,packet+4,packetSize-4);
    buffer += packetSize+4;
}
