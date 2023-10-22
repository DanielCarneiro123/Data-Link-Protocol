// Link layer header.
// NOTE: This file must not be changed.

#ifndef _LINK_LAYER_H_
#define _LINK_LAYER_H_

typedef enum
{
    LlTx,
    LlRx,
} LinkLayerRole;

typedef struct
{
    char serialPort[50];
    LinkLayerRole role;
    int baudRate;
    int nRetransmissions;
    int timeout;
} LinkLayer;

typedef enum{
    INIT,
    ASTATE,
    CSTATE,
    BCCSTATE,
    FINAL,
    ERROR,
    DONE,
    DATA,
    FLAGRCV,
    ARCV,
    CRCV,
    BCCOK,
    DESTUFF,
    STOP
} state_t;

// SIZE of maximum acceptable payload.
// Maximum number of bytes that application layer should send to link layer
#define MAX_PAYLOAD_SIZE 1000

// MISC
#define FALSE 0
#define TRUE 1
#define ESC 0x7D
#define C_I(Ns) (Ns << 6)
#define C_RR(N) (N << 7 | 0x05)
#define C_RJ(N) (N << 7 | 0x01)

// Open a connection using the "port" parameters defined in struct linkLayer.
// Return "1" on success or "-1" on error.
int llopen(LinkLayer connectionParameters);

// Send data in buf with size bufSize.
// Return number of chars written, or "-1" on error.
int llwrite(int fd, const unsigned char *buf, int bufSize);

// Receive data in packet.
// Return number of chars read, or "-1" on error.
int llread(int fd, unsigned char *packet);

// Close previously opened connection.
// if showStatistics == TRUE, link layer should print statistics in the console on close.
// Return "1" on success or "-1" on error.
int llclose(int showStatistics);

int connecting(const char *serialPortName);

int destuffing(unsigned char *payload, int size);

unsigned char *stuffing(const unsigned char *payload, int size);

void alarmHandler(int signal);

#endif // _LINK_LAYER_H_
