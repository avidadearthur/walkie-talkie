#ifndef TRANSPORT_H
#define TRANSPORT_H
#endif

#include "config.h"

typedef enum {
    RX_STATE,
    TX_STATE

} fsm_state_t;

void init_simple_transport(void);

// TODO: Remove this or update it somehow in the main task
static fsm_state_t peer_state;

void signal_RXTX_toggle(void);

void sender_task(void* arg);