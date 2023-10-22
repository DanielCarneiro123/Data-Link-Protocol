// Link layer protocol implementation

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>
#include "../include/state_machines.h"
#include "../include/link_layer.h"

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source
#define BAUDRATE 38400
#define BUF_SIZE 5

#define ESC 0x7D // Define ESC
#define FLAG 0x7E
#define AC_SND 0x03
#define A_RCV 0x01
#define UA 0x07
#define SET 0x03

int alarmEnabled = FALSE;
int alarmCount = 0;
int nRetransmissions = 0;
int timeout = 3;
unsigned char info_frameTx = 0;
unsigned char info_frameRx = 1;
unsigned char BCC2 = 0;

state_t state;

/*void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    alarmCount++;

    printf("Alarm #%d\n", alarmCount);
}*/

unsigned char *stuffing(const unsigned char *payload, int size)
{
    printf("stuffing\n");
    unsigned char *send_payload = (unsigned char *)malloc(size); // Make it big enough for worst-case scenario
    printf("Size Stuffing: %x\n", size);

    if (send_payload == NULL)
    {
        return NULL;
    }

    int send_index = 0;
    for (int i = 0; i < size; i++)
    {
        
        if (payload[i] == FLAG)
        {
            send_payload[send_index++] = ESC;
            send_payload[send_index++] = 0x5E;
        }
        else if (payload[i] == ESC)
        {
            send_payload[send_index++] = ESC;
            send_payload[send_index++] = 0x5D;
        }
        else
        {
            send_payload[send_index++] = payload[i];
        }
    }
    
    BCC2 = send_payload[0];
    for (int i = 1; i < size; i++)
    {
        BCC2 ^= send_payload[i];
    }

    // so para debug
    for (int i = 0; i < size; i++){
        printf("stuffing:%x\n", send_payload[i]);
    }

    return send_payload;
}


int destuffing(unsigned char *payload, int size)
{
    printf("destuffing\n");

    int res = payload[0];
    //printf("nao entrou no ciclo %02X\n", final_payload[0]);
    printf("%02X\n", payload[0]);
    
    for (int i = 1; i < size; i++)
    {
        res ^= payload[i];
        printf("destuffing: %02X\n", payload[i]);
    }

    unsigned char *final_payload = (unsigned char *)malloc(size); // Make it big enough for worst-case scenario

    if (final_payload == NULL)
    {
        return -1;
    }

    int final_index = 0;
    for (int i = 0; i < size; i++)
    {
        if (payload[i] == ESC)
        {
            if (payload[i + 1] == 0x5E)
            {
                final_payload[final_index++] = FLAG;
                i++;
            }
            else if (payload[i + 1] == 0x5D)
            {
                final_payload[final_index++] = ESC;
                i++;
            }
        }
        else
        {
            final_payload[final_index++] = payload[i];
        }
    }
    
    for (int i = 1; i < final_index; i++)
    {
        printf("destuffing: %02X\n", final_payload[i]);
    }

    free(final_payload); // Free memory allocated for final_payload

    return res;
}

int llwrite(int fd, const unsigned char *buf, int bufSize) {
    printf("in llwrite\n");
    int frameSize = bufSize + 6;
    unsigned char *frame = (unsigned char *)malloc(frameSize);

    if (frame == NULL) {
        return -1;
    }

    frame[0] = FLAG;
    frame[1] = AC_SND;
    frame[2] = C_I(info_frameTx);
    frame[3] = frame[1] ^ frame[2];
    unsigned char Ccontrol;
    unsigned char *stuffedPayload = stuffing(buf, bufSize);

    int frame_index = 4;

    for (int i = 0; i < bufSize; i++) {
        frame[frame_index++] = stuffedPayload[i];
    }

    frame[frame_index++] = BCC2;
    frame[frame_index++] = FLAG;

    bool all_done = false;
    bool rejected = false;

    while (nRetransmissions < 3) {
        alarmEnabled = FALSE;
        alarm(timeout);

        while (alarmEnabled == FALSE && !rejected && !all_done) {
            write(fd, frame, frame_index);

            unsigned char Ccontrol = 0;
            state_t state = INIT;
            unsigned char byte = 0;

            while (state != DONE && alarmEnabled == FALSE) {
                if (read(fd, &byte, 1) > 0) {
                    printf("BYTE: %02X\n", byte);

                    switch (state) {
                        case INIT:
                            if (byte == FLAG)
                                state = FLAGRCV;
                            break;
                        case FLAGRCV:
                            if (byte == 0x01)
                                state = A_RCV;
                            else if (byte != FLAG)
                                state = INIT;
                            break;
                        case A_RCV:
                            if (byte == C_RR(0) || byte == C_RR(1) || byte == C_RJ(0) || byte == C_RJ(1) || byte == 0x0B) {
                                state = CRCV;
                                Ccontrol = byte;
                            } else if (byte == FLAG)
                                state = FLAGRCV;
                            else
                                state = INIT;
                            break;
                        case CRCV:
                            if (byte == (0x01 ^ Ccontrol))
                                state = BCCOK;
                            else if (byte == FLAG)
                                state = FLAGRCV;
                            else
                                state = INIT;
                            break;
                        case BCCOK:
                            if (byte == FLAG) {
                                state = DONE;
                            } else
                                state = INIT;
                            break;
                        default:
                            break;
                    }
                }
            }

            if (!Ccontrol) {
                continue;
            } else if (Ccontrol == C_RJ(0) || Ccontrol == C_RJ(1)) {
                rejected = true;
            } else if (Ccontrol == C_RR(0) || Ccontrol == C_RR(1)) {
                all_done = true;
                info_frameTx = (info_frameTx + 1) % 2;
            } else {
                continue;
            }
        }
        free(frame);

        if (all_done) {
            break;
        }
        nRetransmissions++;
    }

    if (all_done) {
        return bufSize;
    } else {
        llclose(fd);
        return -1;
    }
}


int llread(int fd, unsigned char *packet, int payload_size) {
    printf("in llread\n");
    // Aqui, você pode adicionar a lógica para enviar um UA se receber um SET, como mencionou
    unsigned char byte, Ccontrol;
    state_t state = INIT;
    unsigned char *received_payload = (unsigned char *)malloc(payload_size); // Make it big enough for worst-case scenario
    printf("payload_size: %x\n", payload_size);

    if (received_payload == NULL) {
        return -1;
    }

    int size = 0;

    while (state != DONE) {
        if (read(fd, &byte, 1) > 0) {
            // printf("%02X\n", byte);
            switch (state) {
                case INIT:
                    if (byte == FLAG)
                        state = FLAGRCV;
                    break;
                case FLAGRCV:
                    if (byte == 0x03)
                        state = A_RCV;
                    else if (byte != FLAG)
                        state = INIT;
                    break;
                case A_RCV:
                    if (byte == C_I(0) || byte == C_I(1)) {
                        state = CRCV;
                        Ccontrol = byte;
                    } else if (byte == FLAG)
                        state = FLAGRCV;
                    else if (byte == 0x0B) {
                        unsigned char FRAME1[5] = {FLAG, 0x01, 0x0B, 0x01 ^ 0x0B, FLAG};
                        free(received_payload); // Free memory allocated for received_payload
                        return write(fd, FRAME1, 5);
                    } else
                        state = INIT;
                    break;
                case CRCV:
                    if (byte == (0x03 ^ Ccontrol))
                        state = DESTUFF;
                    else if (byte == FLAG)
                        state = FLAGRCV;
                    else
                        state = INIT;
                    break;
                case DESTUFF:
                    if (byte == FLAG) {
                        received_payload[size - 1] = 0;
                        size--;
                        printf("Size: %x:\n", size);

                        int bcc2_res = destuffing(received_payload, size);
                        printf("bcc2_res = %02X\n", bcc2_res);
                        printf("bcc2 = %02X\n", BCC2);
                        if (bcc2_res == BCC2) {
                            state = DONE;
                            unsigned char FRAME1[5] = {FLAG, 0x01, C_RR(info_frameRx), 0x01 ^ C_RR(info_frameRx), FLAG};
                            info_frameRx = (info_frameRx + 1) % 2;
                            free(received_payload); // Free memory allocated for received_payload
                            return write(fd, FRAME1, 5);
                        } else {
                            printf("Error: retransmission\n");
                            unsigned char FRAME1[5] = {FLAG, 0x01, C_RJ(info_frameRx), 0x01 ^ C_RJ(info_frameRx), FLAG};
                            free(received_payload); // Free memory allocated for received_payload
                            return write(fd, FRAME1, 5);
                        }
                    } else {
                        received_payload[size] = byte;
                        printf("Byte: %x\n", byte);
                        BCC2 = byte;
                        size++;
                    }
                    break;
            }
        }
    }
    free(received_payload); // Liberar a memória no final da função
    return -1;
}


int llclose(int showStatistics)
{
    // TODO

    return 1;
}

/*int connecting(const char *serialPortName)
{
    int fd = open(serialPort, O_RDWR | O_NOCTTY);
    if (fd < 0) {
        perror(serialPort);
        return -1; 
    }

    struct termios oldtio;
    struct termios newtio;

    if (tcgetattr(fd, &oldtio) == -1)
    {
        perror("tcgetattr");
        exit(-1);
    }

    memset(&newtio, 0, sizeof(newtio));

    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 0;
    newtio.c_cc[VMIN] = 0;

    tcflush(fd, TCIOFLUSH);

    if (tcsetattr(fd, TCSANOW, &newtio) == -1) {
        perror("tcsetattr");
        return -1;
    }

    return fd;
}
*/