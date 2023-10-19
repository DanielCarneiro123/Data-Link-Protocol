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


#define FLAG 0x7E
#define AC_SND 0x03
#define A_RCV 0x01
#define UA 0x07
#define SET 0x03

int res = 0x00;

state_t state;
extern int alarmEnabled;
extern int alarmCount;
extern char info_frameTx;
unsigned char info_frameRx = 1;
int nRetransmitions = 0;
unsigned char byte;
unsigned char Ccontrol;



void state_machine(int type_machine, int fd) {

state = INIT;
unsigned char payload[7] = {0};
int index = 0;
unsigned char all_done = 0;
unsigned char rejected = 0;
unsigned char byte, Ccontrol, bcc2;
unsigned char *received_payload = (unsigned char *) malloc(7);
int size = 0;

switch (type_machine)
{
case 0:
        /*while(alarmEnabled==1 && case == ERROR){
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
                        printf("byte nr 4: %x\n",(unsigned int) byte[0] & 0xFF);
                        payload[index] = byte[0]; index++;
                        res = res ^ byte[0];
                        else state = ERROR;
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
        }*/
        
        unsigned char *frame = (unsigned char *) malloc(7);
        while (alarmCount < 4){
        alarmEnabled = FALSE;
        alarm(3);

        while (alarmEnabled == FALSE && !rejected && !all_done) {
            write(fd, frame, 7);

            Ccontrol = 0;
            state_t state = INIT;
            byte = 0;

            while (state != DONE && alarmEnabled == FALSE) {  
                if (read(fd, &byte, 1) > 0) {
                    switch (state) {
                        case INIT:
                            if (byte == FLAG) state = FLAGRCV;
                            break;
                        case FLAGRCV:
                            if (byte == 0x03) state = A_RCV;
                            else if (byte != FLAG) state = INIT;
                            break;
                        case A_RCV:
                            if (byte == C_RR(0) || byte == C_RR(1) || byte == C_RJ(0) || byte == C_RJ(1) || byte == 0x0B){
                                state = CRCV;
                                Ccontrol = byte;   
                            }
                            else if (byte == FLAG) state = FLAGRCV;
                            else state = INIT;
                            break;
                        case CRCV:
                            if (byte == (0x03 ^ Ccontrol)) state = BCCOK;
                            else if (byte == FLAG) state = FLAGRCV;
                            else state = INIT;
                            break;
                        case BCCOK:
                            if (byte == FLAG){
                                state = DONE;
                            }
                            else state = INIT;
                            break;
                        default: 
                            break;
                    }
                } 
            }

            if (!Ccontrol) {
                continue;
            } 
            else if (Ccontrol == C_RJ(0) || Ccontrol == C_RJ(1)) {
                rejected = true;
            } 
            else if (Ccontrol == C_RR(0) || Ccontrol == C_RR(1)) {
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
    break;

case 1:
    /*while(state != STOP){
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
            };*/
            while (state != DONE){
        if (read(fd, &byte, 1) > 0) {
            switch (state) {
                case INIT:
                    if (byte == FLAG) state = FLAGRCV;
                    break;

                case FLAGRCV:
                    if (byte == 0x03) state = A_RCV;
                    else if (byte != FLAG) state = INIT;
                    break;

                case A_RCV:
                    if (byte == C_I(0) || byte == C_I(1)){
                        state = CRCV;
                        Ccontrol = byte;   
                    }
                    else if (byte == FLAG) state = FLAGRCV;
                    else if (byte == 0x0B) {
                            unsigned char FRAME[5] = {FLAG, 0x01, 0x0B, 0x01 ^ 0x0B, FLAG};
                            write(fd, FRAME, 5);
                            //return 0;
                    }
                    else state = INIT;
                    break;

                case CRCV:
                    if (byte == (0x03 ^ Ccontrol)) state = DESTUFF;
                    else if (byte == FLAG) state = FLAGRCV;
                    else state = INIT;
                    break;

                case DESTUFF:
                    if (byte == FLAG){ 
                        received_payload[size-1] = 0;
                        size--;
                        int bcc2_res = destuffing(received_payload, &size);
                        if(bcc2_res == bcc2){
                            state = DONE;
                            unsigned char FRAME[5] = {FLAG, 0x01, C_RR(info_frameRx), 0x01 ^ C_RR(info_frameRx), FLAG};
                            write(fd, FRAME, 5);
                            info_frameRx = (info_frameRx + 1)%2;
                            //return size;
                        }
                        else{
                            printf("Error: retransmition\n");
                            unsigned char FRAME[5] = {FLAG, 0x01, C_RJ(info_frameRx), 0x01 ^ C_RJ(info_frameRx), FLAG};
                            write(fd, FRAME, 5);
                            //return -1;
                        }
                    }
                    else{
                        received_payload[size] = byte;
                        bcc2 = byte;
                        size++;
                    }
                    break;

            }
        }
    }
            break;
default:
    break;
}
}

/*int destuffing(unsigned char *payload){
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
}*/

   

/*void stuffing(unsigned char *payload){
    unsigned char send_payload[MAX_PAYLOAD_SIZE] = {0};
    int second_index = 0;
    for (int i = 0; i < MAX_PAYLOAD_SIZE - 1; i++){
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
}*/
