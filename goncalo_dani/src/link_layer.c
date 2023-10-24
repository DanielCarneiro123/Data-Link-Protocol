// Link layer protocol implementation


#include "../include/link_layer.h"

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

#define BUF_SIZE 5

#define ESC 0x7D // Define ESC
#define FLAG 0x7E
#define AC_SND 0x03
#define A_RCV 0x01
#define UA 0x07
#define SET 0x03

int alarmEnabled = FALSE;
int alarmCount = 0;
int max_transmitions = 0;
int timeout = 0;
unsigned char info_frameTx = 0;
unsigned char info_frameRx = 1;
unsigned char BCC2 = 0;
int stuffedPayload_size = 0;


int llopen(LinkLayer LinkLayerInfo)
{
    int fd = connecting(LinkLayerInfo.serialPort);
    if (fd < 0) return -1;
    printf("O FD é: %i\n", fd);
    
    state_t state = INIT;
    timeout = LinkLayerInfo.timeout;
    max_transmitions = LinkLayerInfo.nRetransmissions;
    unsigned char byte;

    switch (LinkLayerInfo.role)
    {
    case CustomLlSender: {

        (void)signal(SIGALRM, alarmHandler);

        while((LinkLayerInfo.nRetransmissions != 0) && state != DONE){  
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
        
        alarm(LinkLayerInfo.timeout);
        alarmEnabled = 0;

        while (alarmEnabled==0 && state != DONE){
            printf("vai ler\n");
            int nBytes = read(fd,&byte,1);
            printf("leu\n");
            
            if (nBytes > 0){
                switch(state){
                    case INIT:
                        printf("byte nr 1: %x\n",(unsigned int) byte & 0xFF);
                        if (byte==FLAG) state = ASTATE;
                        else state = ERROR;
                        break;
                    case ASTATE:
                        printf("byte nr 2: %x\n",(unsigned int) byte & 0xFF);
                        if (byte==A_RCV) state = CSTATE;
                        else state = ERROR;
                        break;
                    case CSTATE:
                        printf("byte nr 3: %x\n",(unsigned int) byte & 0xFF);
                        if(byte==UA) state = BCCSTATE;
                        else state = ERROR;
                        break;
                    case BCCSTATE:
                        printf("byte nr 4: %x\n",(unsigned int) byte & 0xFF);
                        if(byte == (A_RCV ^ UA)) { printf("%c entrou\n", A_RCV ^ UA); state = FINAL;}
                        else state = ERROR;
                        break;
                    case FINAL:
                        printf("byte nr 5: %x\n",(unsigned int) byte & 0xFF);
                        if(byte==FLAG) {
                            alarm(0);
                            alarmEnabled=0;
                            state = DONE;
                            printf("All done\n");
                        }
                        else state = ERROR;                      
                        break;
                    case ERROR:
                        printf("byte nr error: %x\n",(unsigned int) byte & 0xFF);
                        break;
                        //res = res ^ byte;
                        //else state = ERROR;
                    default:
                        break;
                    }
                }
            }
            LinkLayerInfo.nRetransmissions--;
        }
        if (state != DONE) return -1;
            break;
    }

    case CustomLlReceiver:
        while(state != STOP){
            int nBytes = read(fd,&byte,1);
                
            if (nBytes > 0){
                switch(state){
                    case INIT:
                        printf("Byte nr 1: %x\n",(unsigned int) byte & 0xFF);
                        if (byte==FLAG) state = FLAGRCV;
                        else state = INIT;
                        break;
                    case FLAGRCV:
                        printf("Byte nr 2: %x\n",(unsigned int) byte & 0xFF);
                        if (byte==AC_SND) state = ARCV;
                        else if (byte==FLAG) state = FLAGRCV;
                        else state = INIT;
                        break;
                    case ARCV:
                        printf("Byte nr 3: %x\n",(unsigned int) byte & 0xFF);
                        if(byte==SET) state = CRCV;
                        else if (byte==FLAG) state = FLAGRCV;
                        else state = INIT;
                        break;
                    case CRCV:
                        printf("Byte nr 4: %x\n",(unsigned int) byte & 0xFF);
                        if(byte == (AC_SND ^ SET)) state = BCCOK;
                        else if(byte== FLAG) state = FLAGRCV;
                        else state = INIT;
                        break;
                    case BCCOK:
                        printf("Byte nr 5: %x\n",(unsigned int) byte & 0xFF);
                        if(byte==FLAG) {
                            state = STOP;
                            printf("All done\n");
                        }
                        else state = INIT;                      
                        break;
                    }
                }
            }
            unsigned char buf2[BUF_SIZE] = {0};
            buf2[0] = FLAG;
            buf2[1] = A_RCV;
            buf2[2] = UA;
            buf2[3] = (A_RCV ^ UA) & 0xFF;
            buf2[4] = FLAG;
            for (int i = 0; i < 5; i++) {
                printf("0x%02X", buf2[i]);
                if (i < 4) {
                    printf(", ");
                }
            }
            int bytes = write(fd, buf2, BUF_SIZE);
            printf("%d bytes written \n", bytes);
            break;
    default:
        return -1;
        break;
    } 

    printf("O FD (2) é: %i\n", fd);
    return fd; 
}

void alarmHandler(int signal)
{
    alarmEnabled = TRUE;
    alarmCount++;

    printf("Alarm #%d\n", alarmCount);
}


unsigned char *stuffing(const unsigned char *payload, int size)
{
    printf("stuffing\n");
    unsigned char *send_payload = (unsigned char *)malloc(size); // Make it big enough for worst-case scenario
    printf("Size Stuffing: %d\n", size);

    printf("Before stuffing:\n");
    for (int i = 0; i < size; i++){
        printf("stuffing:%x\n", payload[i]);
    }

    if (send_payload == NULL)
    {
        return NULL;
    }

    BCC2 = payload[0];
    for (int i = 1; i < size; i++)
    {
        BCC2 ^= payload[i];
    }

    int send_index = 0;
    stuffedPayload_size = size;
    for (int i = 0; i < size; i++)
    {
        
        if (payload[i] == FLAG)
        {
            send_payload = realloc(send_payload,++stuffedPayload_size);
            send_payload[send_index++] = ESC;
            send_payload[send_index++] = 0x5E;
            

        }
        else if (payload[i] == ESC)
        {
            send_payload = realloc(send_payload,++stuffedPayload_size);
            send_payload[send_index++] = ESC;
            send_payload[send_index++] = 0x5D;
        }
        else
        {
            send_payload[send_index++] = payload[i];
        }
    }

    // so para debug
    printf("After stuffing:\n");
    for (int i = 0; i < stuffedPayload_size; i++){
        printf("stuffing:%x\n", send_payload[i]);
    }
    return send_payload;
}


int destuffing(unsigned char *payload, int size)
{
    int bcc2_result = 0;
    printf("destuffing\n");

    //printf("nao entrou no ciclo %02X\n", final_payload[0]);
    printf("%02X\n", payload[0]);

    bcc2_result = payload[0];

    for (int i = 1; i < size; i++)
    {
        bcc2_result ^= payload[i];
        printf("destuffing: %02X\n", payload[i]);
    }

    return bcc2_result;
}

int llwrite(int fd, const unsigned char *buf, int bufSize) {
    printf("in llwrite\n");
    int frameSize = bufSize + 6;
    unsigned char *frame = (unsigned char *)malloc(frameSize);

    frame[0] = FLAG;
    frame[1] = AC_SND;
    frame[2] = C_I(info_frameTx);
    frame[3] = frame[1] ^ frame[2];
    unsigned char Ccontrol;
    unsigned char *stuffedPayload = stuffing(buf, bufSize);
    frame = realloc(frame,stuffedPayload_size+6);
    

    int frame_index = 4;

    for (int i = 0; i < stuffedPayload_size ; i++) {
        frame[frame_index++] = stuffedPayload[i];
    }

    frame[frame_index++] = BCC2;
    frame[frame_index++] = FLAG;

    bool all_done = false;
    bool rejected = false;

    int nRetransmissions = 0;

    while (nRetransmissions < max_transmitions){
        alarmEnabled = FALSE;
        alarm(timeout);
        all_done = false;
        rejected = false;
        while (alarmEnabled == FALSE && !rejected && !all_done) {
            write(fd, frame, frame_index);
            Ccontrol = 0;
            state_t state = INIT;
            unsigned char byte = 0;

            while (state != DONE && alarmEnabled == FALSE) {
                    if (read(fd, &byte, 1) > 0 || 1) {
                    printf("BYTE: %02X\n", byte);
                    switch (state) {
                        case INIT:
                            if (byte == FLAG) state = FLAGRCV;
                            break;
                        case FLAGRCV:
                            if (byte == 0x01) state = A_RCV;
                            else if (byte != FLAG) state = INIT;
                            break;
                        case A_RCV:
                            if (byte == C_RR(0) || byte == C_RR(1) || byte == C_RJ(0) || byte == C_RJ(1) || byte == 0x0B) {
                                state = CRCV;
                                Ccontrol = byte;
                            } 
                            else if (byte == FLAG) state = FLAGRCV;
                            else state = INIT;
                            break;
                        case CRCV:
                            if (byte == (0x01 ^ Ccontrol)) state = BCCOK;
                            else if (byte == FLAG) state = FLAGRCV;
                            else state = INIT;
                            break;
                        case BCCOK:
                            if (byte == FLAG) {
                                state = DONE;
                            } else state = INIT;
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
                printf("ola\n");
                all_done = true;
                info_frameTx = (info_frameTx + 1) % 2;
            } 
            else {
                continue;
            }  
        }

        if (all_done) {
            printf("adeus");
            break;
        }
        nRetransmissions++;
    }
    
    free(frame);

    if (all_done) {
        return frameSize;
    } else {
        llclose(fd);
        return -1;
    }
}


int llread(int fd, unsigned char *received_payload) {
    printf("in llread\n");
    // Aqui, você pode adicionar a lógica para enviar um UA se receber um SET, como mencionou
    unsigned char byte, Ccontrol;
    state_t state = INIT;
    //printf("payload_size: %x\n", payload_size);

    if (received_payload == NULL) {
        return -1;
    }
    int size = 0;
    int bcc2_res = 0;
    while (state != DONE) {
        if (read(fd, &byte, 1) > 0) {
            printf("beggining of the read cycle: %02X\n", byte);
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
                        write(fd, FRAME1, 5);
                        return 0;
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
                    if (byte == ESC) state = ESCAPE;
                    else if (byte == FLAG) {
                        bcc2_res = 0;
                        received_payload[size - 1] = '\0';
                        size--;
                        printf("Size: %x:\n", size);
                        bcc2_res = destuffing(received_payload, size);
                        printf("bcc2_res = %02X\n", bcc2_res);
                        printf("bcc2 = %02X\n", BCC2);
                        if (bcc2_res == BCC2) {
                            state = DONE;
                            unsigned char FRAME1[5] = {FLAG, 0x01, C_RR(info_frameRx), 0x01 ^ C_RR(info_frameRx), FLAG};
                            write(fd, FRAME1, 5);
                            info_frameRx = (info_frameRx + 1) % 2;
                            return size;
                        } else {
                            printf("Error: retransmission\n");
                            unsigned char FRAME1[5] = {FLAG, 0x01, C_RJ(info_frameRx), 0x01 ^ C_RJ(info_frameRx), FLAG};
                            write(fd, FRAME1, 5);
                            return -1;
                        };
                    } else {
                        received_payload[size] = byte;
                        printf("Byte: %x\n", byte);
                        BCC2 = byte;
                        size++;
                    }
                    break;
                case ESCAPE:
                    state = DESTUFF;    
                    if (byte == 0x5e){
                        received_payload[size] = FLAG;
                        BCC2 = byte;
                        size++;
                    }
                    else if (byte == 0x5d){
                        received_payload[size] = ESC;
                        BCC2 = byte;
                        size++;
                    }
                    else if (byte == FLAG){
                        received_payload[size - 1] = ESC;
                        printf("Size: %x:\n", size);
                        bcc2_res = destuffing(received_payload, size);
                        printf("bcc2_res = %02X\n", bcc2_res);
                        printf("bcc2 = %02X\n", BCC2);
                        if (bcc2_res == BCC2) {
                            state = DONE;
                            unsigned char FRAME1[5] = {FLAG, 0x01, C_RR(info_frameRx), 0x01 ^ C_RR(info_frameRx), FLAG};
                            write(fd, FRAME1, 5);
                            info_frameRx = (info_frameRx + 1) % 2;
                            return size;
                        } else {
                            printf("Error: retransmission\n");
                            unsigned char FRAME1[5] = {FLAG, 0x01, C_RJ(info_frameRx), 0x01 ^ C_RJ(info_frameRx), FLAG};
                            write(fd, FRAME1, 5);
                            return -1;
                        };
                    }
                    
                    else {
                        received_payload[size] = ESC;
                        size++;
                        received_payload[size] = byte; 
                        BCC2 = byte;
                        size++;
                    }
                    break;
                default: 
                    break; 
            }
        }
    }
    return -1;
}


int llclose(int fd){

    state_t state = INIT;
    unsigned char byte;
    (void) signal(SIGALRM, alarmHandler);
    
    while (max_transmitions != 0 && state != DONE) {
                
        unsigned char FRAME1[5] = {FLAG, 0x03, 0x0B, 0x03 ^ 0x0B, FLAG};
        write(fd, FRAME1, 5);
        alarm(timeout);
        alarmEnabled = FALSE;
                
        while (alarmEnabled == FALSE && state != DONE) {
            if (read(fd, &byte, 1) > 0)  {
                switch (state) {
                    case INIT:
                        if (byte == FLAG) state = FLAGRCV;
                        break;
                    case FLAGRCV:
                        if (byte == 0x01) state = ARCV;
                        else if (byte != FLAG) state = INIT;
                        break;
                    case ARCV:
                        if (byte == 0x0B) state = CRCV;
                        else if (byte == FLAG) state = FLAGRCV;
                        else state = INIT;
                        break;
                    case CRCV:
                        if (byte == (0x01 ^ 0x0B)) state = BCCOK;
                        else if (byte == FLAG) state = FLAGRCV;
                        else state = INIT;
                        break;
                    case BCCOK:
                        if (byte == FLAG) state = DONE;
                        else state = INIT;
                        break;
                    default: 
                        break;
                }
            }
        } 
        max_transmitions--;
    }

    if (state != DONE) return -1;
    unsigned char FRAME2[5] = {FLAG, 0x03, 0x07, 0x03 ^ 0x07, FLAG};
    write(fd, FRAME2, 5);
    return close(fd);
}

int connecting(const char *serialPortName)
{
    int fd = open(serialPortName, O_RDWR | O_NOCTTY);
    if (fd < 0) {
        perror(serialPortName);
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
