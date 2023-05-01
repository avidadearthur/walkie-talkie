#ifndef TRANSPORT_H
#define TRANSPORT_H
#endif

#include "config.h"

typedef enum {
    RX_STATE,
    TX_STATE

} fsm_state_t;

// task handle for sender_task
TaskHandle_t sender_task_handle;

// TODO: Remove this or update it somehow in the main task
static fsm_state_t peer_state;

void init_simple_transport(StreamBufferHandle_t mic_stream_buf,
                           StreamBufferHandle_t network_stream_buf);

void signal_RXTX_toggle(void);

void sender_task(void* arg);