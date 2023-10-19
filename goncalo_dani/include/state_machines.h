#ifndef STATE_MACHINES_H
#define STATE_MACHINES_H

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

// Protótipo da função da máquina de estado
void state_machine(int type_machine, int fd);

#endif
