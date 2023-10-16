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
#include "state_machines.h"
#include "link_layer.h"

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

#define BUF_SIZE 5

int alarmEnabled = FALSE;
int alarmCount = 0;
int nRetransmitions = 0;
int timeout = 0;
unsigned char info_frameTx = 0;

#define FLAG 0x7E
#define AC_SND 0x03
#define A_RCV 0x01
#define UA 0x07
#define SET 0x03
int res = 0x00;

state_t state;

void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    alarmCount++;

    printf("Alarm #%d\n", alarmCount);
}


////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters)
{

    state = INIT;
    int dev = connecting(connectionParameters.serialPort);
    unsigned char payload[MAX_PAYLOAD_SIZE] = {0};
    int index = 0;
    timeout = connectionParameters.timeout;
    nRetransmitions = connectionParameters.nRetransmissions;

    switch (connectionParameters.role)
    {
    case LlTx:
        (void)signal(SIGALRM, alarmHandler);

        while(((alarmCount < 4 && alarmEnabled == 0)) && state != DONE){
        printf("dentro do while\n");

        // Create string to send
        unsigned char buf[BUF_SIZE] = {0};


        buf[0] = FLAG;
        buf[1] = AC_SND;
        buf[2] = SET;
        buf[3] = (AC_SND ^ SET) & 0xFF;
        buf[4] = FLAG;

        int bytes = write(fd, buf, BUF_SIZE);
        printf("%d bytes written\n", bytes);
        
        alarm(connectionParameters.timeout);
        alarmEnabled = 1;

        while (alarmEnabled==1){
            unsigned char byte[1] = {0};
            int nBytes = read(fd,byte,1);
            
            if (nBytes>0){
                switch(state){
                    case INIT:
                        printf("byte nr 1: %x\n",(unsigned int) byte[0] & 0xFF);
                        if (byte[0]==FLAG) state = ASTATE;
                        else state = ERROR;
                        break;
                    case ASTATE:
                        printf("byte nr 2: %x\n",(unsigned int) byte[0] & 0xFF);
                        if (byte[0]==A_RCV) state = CSTATE;
                        else state = ERROR;
                        break;
                    case CSTATE:
                        printf("byte nr 3: %x\n",(unsigned int) byte[0] & 0xFF);
                        if(byte[0]==UA) state = BCCSTATE;
                        else state = ERROR;
                        break;
                    case BCCSTATE:
                        printf("byte nr 4: %x\n",(unsigned int) byte[0] & 0xFF);
                        if(byte[0]== (A_RCV ^ UA) & 0xFF) state = FINAL;
                        else state = ERROR;
                        break;
                    case ERROR:
                        printf("byte nr error: %x\n",(unsigned int) byte[0] & 0xFF);
                        break;
                        //res = res ^ byte[0];
                        //else state = ERROR;
                    case FINAL:
                        printf("byte nr 5: %x\n",(unsigned int) byte[0] & 0xFF);
                        if(byte[0]==FLAG) {
                            alarm(0);
                            alarmEnabled=0;
                            state = DONE;
                            printf("All done\n");
                        }
                        else state = ERROR;                      
                        break;
                }
            }
            connectionParameters.nRetransmissions--;
        }
        if (state != FINAL) return -1;
            break; 
    }
    break;

    case LlRx:
        while(state != STOP){
            unsigned char byte[1] = {0};
            int nBytes = read(fd,byte,1);
                
            if (nBytes>0){
                switch(state){
                    case INIT:
                        printf("byte nr 1: %x\n",(unsigned int) byte[0] & 0xFF);
                        if (byte[0]==FLAG) state = FLAGRCV;
                        else state = INIT;
                        break;
                    case FLAGRCV:
                        printf("byte nr 2: %x\n",(unsigned int) byte[0] & 0xFF);
                        if (byte[0]==AC_SND) state = ARCV;
                        else if (byte[0]==FLAG) state = FLAGRCV;
                        else state = INIT;
                        break;
                    case ARCV:
                        printf("byte nr 3: %x\n",(unsigned int) byte[0] & 0xFF);
                        if(byte[0]==SET) state = CRCV;
                        else if (byte[0]==FLAG) state = FLAGRCV;
                        else state = INIT;
                        break;
                    case CRCV:
                        printf("byte nr 4: %x\n",(unsigned int) byte[0] & 0xFF);
                        if(byte[0]== (AC_SND ^ SET) & 0xFF) state = BCCOK;
                        else if(byte[0]== FLAG) state = FLAGRCV;
                        else state = INIT;
                        break;
                    case BCCOK:
                        printf("byte nr 5: %x\n",(unsigned int) byte[0] & 0xFF);
                        if(byte[0]==FLAG) {
                            state = STOP;
                            printf("All done\n");
                        }
                        else state = INIT;                      
                        break;
                    }
                }
            }
            buf2[0] = FLAG;
            buf2[1] = A_RCV;
            buf2[2] = UA;
            buf2[3] = (A_RCV ^ UA) & 0xFF;
            buf2[4] = FLAG;
            int bytes = write(fd, buf2, BUF_SIZE);
            printf("%d bytes written \n", bytes);
            break;
    default:
        return -1;
        break;
    }  
    return fd; 
}



unsigned char stuffing(unsigned char *payload){
    unsigned char send_payload[MAX_PAYLOAD_SIZE] = {0};
    int second_index = 0;
    for (int i = 4; i < MAX_PAYLOAD_SIZE /*- 1*/; i++){
        if (payload[i] == FLAG){
            send_payload[i + second_index] = ESC; 
            second_index++;
            send_payload[i + second_index] = 0x5E;
        }
        else if (payload[i] == ESC){
            send_payload[i + second_index] = ESC; 
            second_index++;
            send_payload[i + second_index] = 0x5D;
        }
        else{
            send_payload[i + second_index] = payload[i];
        }
    }
    return send_payload;
}


int llwrite(int fd, const unsigned char *buf, int bufSize) { 
    unsigned char *frame = (unsigned char *) malloc(frameSize);
    frame[0] = FLAG;
    frame[1] = AC_SND;
    frame[2] = C_I(info_frameTx);
    frame[3] = FLAG;

    unsigned char *stuffedPayload = stuffing(buf, bufSize);

    int frame_index = 4;

    for (int i = 0; i < bufSize; i++) {
        frame[frame_index++] = stuffedPayload[i];
    }

    unsigned char BCC2 = stuffedPayload[0];
    for (int i = 1; i < bufSize; i++) {
        BCC2 ^= stuffedPayload[i];
    }
    frame[frame_index++] = BCC2;
    frame[frame_index++] = FLAG;

    int alarmCount = 0;
    bool all_done = false;
    bool rejected = false;

    while (alarmCount < nRetransmitions){
        alarmEnabled = FALSE;
        alarm(timeout);

        while (alarmEnabled == FALSE && !rejected && !all_done) {
            write(fd, frame, frame_index);

            unsigned char result = ler RR (a fazer);

            if (!result) {
                continue;
            } 
            else if (result == C_REJ(0) || result == C_REJ(1)) {
                rejected = true;
            } 
            else if (result == C_RR(0) || result == C_RR(1)) {
                all_done = true;
                info_frameTx = (info_frameTx + 1) % 2;
            } 
            else {
                continue;
            }
        }   
        free(frame);

        if (all_done) {
            break;
        }
        nRetransmitions++;
    }

    if (all_done) {
        return bufSize;
    } else {
        llclose(fd);
        return -1;
    }
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    // TODO

    return 0;
}

int destuffing(unsigned char *payload){
    unsigned char final_payload[MAX_PAYLOAD_SIZE] = {0};
    int res = 0;
    for (int i = 0; i < MAX_PAYLOAD_SIZE; i++){
        if (payload[i] == ESC && payload[i+1] == 0x5E){
            final_payload[i] = FLAG; 
            i++;          
        }
        else if(payload[i] == ESC && payload[i+1] == 0x5D){
            final_payload[i] = ESC; 
            i++; 
        }
        else{
            final_payload[i] = payload[i];
        }
    }
    for (int i = 0; i < MAX_PAYLOAD_SIZE; i++){
        res = final_payload[i] ^ res;
    }
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics)
{
    // TODO

    return 1;
}


int connecting(const char *serialPortName){
    const char *serialPortName = argv[1];

    if (argc < 2)
    {
        printf("Incorrect program usage\n"
               "Usage: %s <SerialPort>\n"
               "Example: %s /dev/ttyS1\n",
               argv[0],
               argv[0]);
        exit(1);
    }

    // Open serial port device for reading and writing, and not as controlling tty
    // because we don't want to get killed if linenoise sends CTRL-C.
    int fd = open(serialPortName, O_RDWR | O_NOCTTY);

    if (fd < 0)
    {
        perror(serialPortName);
        exit(-1);
    }

    struct termios oldtio;
    struct termios newtio;

    // Save current port settings
    if (tcgetattr(fd, &oldtio) == -1)
    {
        perror("tcgetattr");
        exit(-1);
    }

    // Clear struct for new port settings
    memset(&newtio, 0, sizeof(newtio));

    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    // Set input mode (non-canonical, no echo,...)
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 0; // Inter-character timer unused
    newtio.c_cc[VMIN] = 0;  // Blocking read until 5 chars received

    // VTIME e VMIN should be changed in order to protect with a
    // timeout the reception of the following character(s)

    // Now clean the line and activate the settings for the port
    // tcflush() discards data written to the object referred to
    // by fd but not transmitted, or data received but not read,
    // depending on the value of queue_selector:
    //   TCIFLUSH - flushes data received but not read.
    tcflush(fd, TCIOFLUSH);

    // Set new port settings
    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    printf("New termios structure set\n");

}