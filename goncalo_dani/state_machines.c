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


#define FLAG 0x7E
#define AC_SND 0x03
#define A_RCV 0x01
#define UA 0x07
#define SET 0x03

state_t state;
extern int alarmEnabled;


void state_machine(int type_machine, int fd) {

state = INIT;
switch (type_machine)
{
case 0:
        while(alarmEnabled==1 && state != ERROR){
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
    break;

case 1:
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