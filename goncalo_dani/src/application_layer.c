// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"
#include "read_data.h"
#include "write_data.h"
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
#include <time.h>

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
    if (fd < 0) {
        perror("erro no connecting\n");
        exit(-1);
    }

    switch (linkLayer.role) {

        case CustomLlSender: {
            
            FILE* file = fopen(filename, "rb");
            if (file == NULL) {
                perror("Ficheiro não encontrado\n");
                exit(-1);
            }

            int prev = ftell(file);
            fseek(file,0L,SEEK_END);
            long int fileSize = ftell(file)-prev;
            fseek(file,prev,SEEK_SET);

            unsigned int cpSize;
            unsigned char *controlPacketStart = controlPacket(2, filename, fileSize, &cpSize);
            if(llwrite(fd, controlPacketStart, cpSize) == -1){ 
                printf("Erro no primeiro control Packet\n");
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
                    printf("Erro no data packet\n");
                    exit(-1);
                }
                else{
                    bytesLeft -= dataSize;
                    content += dataSize;
                    sequence = (sequence + 1) % 255;
                }
               
            }


            unsigned char *controlPacketEnd = controlPacket(3, filename, fileSize, &cpSize);
            if(llwrite(fd, controlPacketEnd, cpSize) == -1) { 
                printf("Erro no ultimo control packet\n");
                exit(-1);
            }
            llclose(fd);
            break;
        }

        case CustomLlReceiver: {

            unsigned char *packet = (unsigned char *)malloc(MAX_PAYLOAD_SIZE);
            int packetSize = -1;
            clock_t start_time = clock();
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
            clock_t end_time = clock();
            double full_time = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;
            fclose(newFile);
            break;

        default:
            exit(-1);
            break;
    }}
}





