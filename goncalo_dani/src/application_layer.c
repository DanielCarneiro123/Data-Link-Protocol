// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"
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

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{
    LinkLayer linkLayer;
    strcpy(linkLayer.serialPort,serialPort);
    linkLayer.role = strcmp(role, "tx") ? CustomLlReceiver : CustomLlSender;
    linkLayer.baudRate = baudRate;
    linkLayer.nRetransmissions = nTries;
    linkLayer.timeout = timeout;

    int fd = llopen(linkLayer);
    printf("O FD no ap layer é: %i\n", fd);
    if (fd < 0) {
        perror("Connection error\n");
        exit(-1);
    }

    switch (linkLayer.role) {

        case CustomLlSender: {
            
            FILE* file = fopen(filename, "rb");
            if (file == NULL) {
                perror("File not found\n");
                exit(-1);
            }

            int prev = ftell(file);
            fseek(file,0L,SEEK_END);
            long int fileSize = ftell(file)-prev;
            fseek(file,prev,SEEK_SET);

            unsigned int cpSize;
            unsigned char *controlPacketStart = controlPacket(2, filename, fileSize, &cpSize);
            if(llwrite(fd, controlPacketStart, cpSize) == -1){ 
                printf("Exit: error in start packet\n");
                exit(-1);
            }

            unsigned char sequence = 0;
            unsigned char *content = data(file, fileSize);
            long int bytesLeft = fileSize;

            while (bytesLeft > 0) {

                int dataSize;

                if (bytesLeft > (long int)MAX_PAYLOAD_SIZE) {
                    dataSize = MAX_PAYLOAD_SIZE;
                } else {
                    dataSize = bytesLeft;
                }

                unsigned char *data = (unsigned char *)malloc(dataSize);
                memcpy(data, content, dataSize);
                int packetSize;
                unsigned char *packet = dataPacket(sequence, data, dataSize, &packetSize);

                if (llwrite(fd, packet, packetSize) == -1) {
                    printf("Exit: error in data packet\n");
                    exit(-1);
                }

                bytesLeft -= dataSize;
                content += dataSize;
                sequence = (sequence + 1) % 255;
            }


            unsigned char *controlPacketEnd = controlPacket(3, filename, fileSize, &cpSize);
            if(llwrite(fd, controlPacketEnd, cpSize) == -1) { 
                printf("Exit: error in end packet\n");
                exit(-1);
            }
            llclose(fd);
            break;
        }

        case CustomLlReceiver: {

            unsigned char *packet = (unsigned char *)malloc(MAX_PAYLOAD_SIZE);
            int packetSize = -1;
            while ((packetSize = llread(fd, packet)) < 0);

            unsigned long int rxFileSize = 0;
            unsigned char* name = readControlPacket(packet, packetSize, &rxFileSize); 

            FILE* newFile = fopen((char *) name, "wb+");

            while (1) {    

                while ((packetSize = llread(fd, packet)) < 0);

                if(packetSize == 0) break;

                else if(packet[0] != 3){
                    unsigned char *buffer = (unsigned char*)malloc(packetSize);
                    readDataPacket(packet, packetSize, buffer);
                    fwrite(buffer, sizeof(unsigned char), packetSize-4, newFile);
                    free(buffer);
                }

                else continue;
            }

            fclose(newFile);
            break;

        default:
            exit(-1);
            break;
    }}
}

unsigned char *readControlPacket(unsigned char* packet, int size, unsigned long int *fileSize) {

    
    unsigned char fileSizeNBytes = packet[2];
    unsigned char fileSizeAux[fileSizeNBytes];
    memcpy(fileSizeAux, packet+3, fileSizeNBytes);
    for(unsigned int i = 0; i < fileSizeNBytes; i++)
        *fileSize |= (fileSizeAux[fileSizeNBytes-i-1] << (8*i));

    
    unsigned char fileNameNBytes = packet[3+fileSizeNBytes+1];
    unsigned char *name = (unsigned char*)malloc(fileNameNBytes);
    memcpy(name, packet+3+fileSizeNBytes+2, fileNameNBytes);
    return name;
}

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


void readDataPacket(const unsigned char* packet, const unsigned int packetSize, unsigned char* buffer) {
    memcpy(buffer,packet+4,packetSize-4);
    buffer += packetSize+4;
}