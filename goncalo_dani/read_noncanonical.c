// Read from serial port in non-canonical mode
//
// Modified by: Eduardo Nuno Almeida [enalmeida@fe.up.pt]

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

// Baudrate settings are defined in <asm/termbits.h>, which is
// included by <termios.h>
#define BAUDRATE B38400
#define _POSIX_SOURCE 1 // POSIX compliant source

#define FALSE 0
#define TRUE 1

#define BUF_SIZE 5
#define FLAG 0x7E
#define AC_SND 0x03
#define A_RCV 0x01
#define UA 0x07
#define SET 0x03



typedef enum{
    START,
    FLAGRCV,
    ARCV,
    CRCV,
    BCCOK,
    ERROR,
    STOP
} state_t;
state_t state = START;

int main(int argc, char *argv[])
{
    // Program usage: Uses either COM1 or COM2
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

    // Open serial port device for reading and writing and not as controlling tty
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



    
   
    while(state != STOP){
            unsigned char byte[1] = {0};
            int nBytes = read(fd,byte,1);
            
            if (nBytes>0){
                switch(state){
                    case START:
                        printf("byte nr 1: %x\n",(unsigned int) byte[0] & 0xFF);
                        if (byte[0]==FLAG) state = FLAGRCV;
                        else state = START;
                        break;
                    case FLAGRCV:
                        printf("byte nr 2: %x\n",(unsigned int) byte[0] & 0xFF);
                        if (byte[0]==AC_SND) state = ARCV;
                        else if (byte[0]==FLAG) state = FLAGRCV;
                        else state = START;
                        break;
                    case ARCV:
                        printf("byte nr 3: %x\n",(unsigned int) byte[0] & 0xFF);
                        if(byte[0]==SET) state = CRCV;
                        else if (byte[0]==FLAG) state = FLAGRCV;
                        else state = START;
                        break;
                    case CRCV:
                        printf("byte nr 4: %x\n",(unsigned int) byte[0] & 0xFF);
                        if(byte[0]== (AC_SND ^ SET) & 0xFF) state = BCCOK;
                        else if(byte[0]== FLAG) state = FLAGRCV;
                        else state = START;
                        break;
                    case BCCOK:
                        printf("byte nr 5: %x\n",(unsigned int) byte[0] & 0xFF);
                        if(byte[0]==FLAG) {
                            state = STOP;
                            printf("All done\n");
                        }
                        else state = START;                      
                        break;
                }
            }

        }
        
    //enviar o UA 
    unsigned char buf2[BUF_SIZE] = {0};
    
    buf2[0] = FLAG;
    buf2[1] = A_RCV;
    buf2[2] = UA;
    buf2[3] = (A_RCV ^ UA) & 0xFF;
    buf2[4] = FLAG;
    
    int bytes = write(fd, buf2, BUF_SIZE);
    printf("%d bytes written \n", bytes);
    


    // The while() cycle should be changed in order to respect the specifications
    // of the protocol indicated in the Lab guide

    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);

    return 0;
}
