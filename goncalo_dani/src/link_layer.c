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

    switch (connectionParameters.role)
    {
    case LlTx:
        (void)signal(SIGALRM, alarmHandler);

        while(((alarmCount < 4 && alarmEnabled == 0) || state == ERROR) && state != DONE){
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
        
        alarm(3);
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
        }
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
            };
            break;
    default:
        break;
    }   
}


////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize)
{
    // TODO

    return 0;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    // TODO

    return 0;
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