#ifndef TX_API_H
#define TX_API_H
#include <stdint.h>
typedef unsigned int UINT;
typedef uint32_t ULONG;
typedef struct { uint32_t dummy; } TX_THREAD;
typedef struct { uint32_t dummy; } TX_MUTEX;
typedef struct { uint32_t dummy; } TX_QUEUE;
#define TX_SUCCESS 0U
#define TX_INHERIT 1U
#define TX_WAIT_FOREVER 0xFFFFFFFFU
#define TX_NO_WAIT 0U
#define TX_NO_TIME_SLICE 0U
#define TX_AUTO_START 1U
#define TX_TIMER_TICKS_PER_SECOND 1000U
UINT tx_mutex_create(TX_MUTEX*, const char*, UINT);
UINT tx_mutex_delete(TX_MUTEX*);
UINT tx_mutex_get(TX_MUTEX*, ULONG);
UINT tx_mutex_put(TX_MUTEX*);
UINT tx_queue_create(TX_QUEUE*, const char*, UINT, void*, ULONG);
UINT tx_queue_delete(TX_QUEUE*);
UINT tx_queue_send(TX_QUEUE*, const void*, ULONG);
UINT tx_queue_receive(TX_QUEUE*, void*, ULONG);
UINT tx_thread_create(TX_THREAD*, const char*, void (*)(ULONG), ULONG, void*, ULONG, UINT, UINT, ULONG, UINT);
UINT tx_thread_sleep(ULONG);
UINT tx_thread_relinquish(void);
#endif
