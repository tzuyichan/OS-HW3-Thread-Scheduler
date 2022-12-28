#ifndef FEEDBACK_QUEUE_H
#define FEEDBACK_QUEUE_H

#define _XOPEN_SOURCE 600
#define N_QUEUES 29
#define N_PRIOR_LVL 3
#define N_EVENT_Q 8
#define HIGH_TQ 100
#define MEDIUM_TQ 200
#define LOW_TQ 300
#define MAX_STR_LEN 128
#define MAX_THREAD_NUM 64

#include <ucontext.h>
#include <stdbool.h>

typedef enum
{
    HIGH = 0,   // high
    MEDIUM = 1, // medium
    LOW = 2     // low
} Prior;

typedef enum
{
    RUNNING = -1,
    READY = 0,      // ready
    WAITING = 1,    // waiting
    WAIT_TIME = 27, // waiting for timer to expire
    TERMINATED = 28 // terminated
} State;

typedef struct thread_t
{
    int tid;
    State state;
    char name[MAX_STR_LEN];
    char p_func[MAX_STR_LEN];
    ucontext_t ctx;
    Prior b_priority; // base priority
    Prior c_priority; // current priority
    int cancel_mode;
    bool am_cancelled;
    int event_id;
    int queue_time;
    int wait_time;
    int elapsed;
    int timer; // timer for thread wait time
    struct thread_t *next;
} Thread;

typedef struct queue_t
{
    Thread **q;
} Queue;

Queue *create_queue(void);
Thread *init_thread(int tid,
                    char *name,
                    char *p_func,
                    Prior b_priority,
                    int cancel_mode);
int get_queue_idx(State Q_type, Prior c_priority, int event_id);
int fill_thread_id_list(Queue *Q, Thread *Running, Thread **list);
int enqueue(Queue *Q, Thread *T, State Q_type, int event_id);
Thread *dequeue(Queue *Q, State Q_type, Prior c_priority, int event_id);
Thread *dequeue_set_event(Queue *Q, int event_id);
Thread *dequeue_wait_time(Queue *Q, int tid);
int next_wait_timeout_thread(Queue *Q);

#endif
