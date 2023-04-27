#ifndef TRANSPORT_H
#define TRANSPORT_H
#endif

#include "config.h"

typedef enum {
    RX_STATE,
    TX_STATE

} fsm_state_t;

void init_simple_transport(void);

static fsm_state_t peer_state;

void signal_RX(void);

void signal_TX(void);

void sender_task(void* arg);