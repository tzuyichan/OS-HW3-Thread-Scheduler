#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "feedback_queue.h"

#define FAIL_IF(EXP, MSG)                        \
    {                                            \
        if (EXP)                                 \
        {                                        \
            fprintf(stderr, "Error! " MSG "\n"); \
            exit(EXIT_FAILURE);                  \
        }                                        \
    }

Queue *create_queue()
{
    Queue *Q;
    FAIL_IF(!(Q = malloc(sizeof(Queue))), "Queue head malloc failure!");
    FAIL_IF(!(Q->q = malloc(sizeof(Thread *) * N_QUEUES)), "Queue array malloc failure!");

    for (int i = 0; i < N_QUEUES; ++i)
        Q->q[i] = NULL;

    return Q;
}

Thread *init_thread(int tid,
                    char *name,
                    char *p_func,
                    Prior b_priority,
                    int cancel_mode)
{
    Thread *T;

    FAIL_IF(!(T = malloc(sizeof(Thread))), "Thread malloc failure!");

    T->tid = tid;
    T->state = READY;
    strncpy(T->name, name, MAX_STR_LEN);
    strncpy(T->p_func, p_func, MAX_STR_LEN);
    T->b_priority = b_priority; // base priority
    T->c_priority = b_priority; // current priority
    T->cancel_mode = cancel_mode;
    T->am_cancelled = false;
    T->event_id = 0;
    T->queue_time = 0;
    T->wait_time = 0;
    T->elapsed = 0;
    T->timer = 0;
    T->next = NULL;

    return T;
}

int get_queue_idx(State Q_type, Prior c_priority, int event_id)
{
    if (Q_type == TERMINATED)
    {
        return TERMINATED;
    }
    else if (event_id == 8)
    {
        return WAIT_TIME;
    }
    else if (Q_type == READY)
    {
        return c_priority;
    }
    else if (Q_type == WAITING)
    {
        return WAITING * N_PRIOR_LVL + c_priority * N_EVENT_Q + event_id;
    }
    else
    {
        return -1; // this should never execute
    }
}

int fill_thread_id_list(Queue *Q, Thread *Running, Thread **list)
{
    Thread *p;
    int count = 1;

    memset(list, 0, MAX_THREAD_NUM);

    list[0] = Running;
    for (int i = 1; i < N_QUEUES; ++i)
    {
        for (p = Q->q[i - 1]; p != NULL; p = p->next)
        {
            list[count++] = p;
        }
    }

    return count;
}

int enqueue(Queue *Q, Thread *T, State Q_type, int event_id)
{
    Thread *p;

    int index = get_queue_idx(Q_type, T->c_priority, event_id);

    for (p = Q->q[index]; p != NULL; p = p->next)
    {
        if (p->tid == T->tid)
            return -1;
        if (!p->next)
            break;
    } // p = last node in the linked list

    if (!p)
        Q->q[index] = T;
    else
        p->next = T;

    // printf("enqueue idx: %d\n", index);
    // printf("tid: %d\n", T->tid);
    // printf("state: %d\n", T->state);
    // printf("name: %s\n", T->name);
    // printf("entry function: %s\n", T->p_func);
    // printf("priority: %d\n", T->b_priority);
    // printf("priority: %d\n", T->c_priority);
    // printf("cancel mode: %d\n", T->cancel_mode);
    // printf("Event id: %d\n", T->event_id);
    // printf("next: %p\n\n", T->next);
    return 0;
}

Thread *dequeue(Queue *Q, State Q_type, Prior c_priority, int event_id)
{
    Thread *p;
    int index = get_queue_idx(Q_type, c_priority, event_id);

    if ((p = Q->q[index]))
    {
        Q->q[index] = p->next;
        p->next = NULL;
        // printf("dequeue idx: %d\n", index);
        // printf("tid: %d\n", p->tid);
        // printf("state: %d\n", p->state);
        // printf("name: %s\n", p->name);
        // printf("entry function: %s\n", p->p_func);
        // printf("priority: %d\n", p->b_priority);
        // printf("priority: %d\n", p->c_priority);
        // printf("cancel mode: %d\n", p->cancel_mode);
        // printf("Event id: %d\n", p->event_id);
        // printf("next: %p\n\n", p->next);
        return p;
    }
    return NULL;
}

Thread *dequeue_set_event(Queue *Q, int event_id)
{
    Thread *p;

    for (int priority = 0; priority < 3; ++priority)
    {
        p = dequeue(Q, WAITING, priority, event_id);
        if (p)
            return p;
    }
    return NULL;
}

Thread *dequeue_wait_time(Queue *Q, int tid)
{
    Thread *p, *prev;

    int index = WAIT_TIME;

    if ((p = Q->q[index]))
    {
        if (p->tid == tid)
        {
            Q->q[index] = p->next;
            p->next = NULL;
            // printf("dequeue idx: %d\n", index);
            // printf("tid: %d\n", p->tid);
            // printf("state: %d\n", p->state);
            // printf("name: %s\n", p->name);
            // printf("entry function: %s\n", p->p_func);
            // printf("priority: %d\n", p->b_priority);
            // printf("priority: %d\n", p->c_priority);
            // printf("cancel mode: %d\n", p->cancel_mode);
            // printf("Event id: %d\n", p->event_id);
            // printf("next: %p\n\n", p->next);
            return p;
        }
    }
    else
        return NULL;

    prev = p;
    p = p->next;

    for (; p != NULL; p = p->next, prev = prev->next)
    {
        if (p->tid == tid)
        {
            prev->next = p->next;
            p->next = NULL;
            // printf("dequeue idx: %d\n", index);
            // printf("tid: %d\n", p->tid);
            // printf("state: %d\n", p->state);
            // printf("name: %s\n", p->name);
            // printf("entry function: %s\n", p->p_func);
            // printf("priority: %d\n", p->b_priority);
            // printf("priority: %d\n", p->c_priority);
            // printf("cancel mode: %d\n", p->cancel_mode);
            // printf("Event id: %d\n", p->event_id);
            // printf("next: %p\n\n", p->next);
            return p;
        }
    }
    return NULL;
}

int next_wait_timeout_thread(Queue *Q)
{
    Thread *p;

    for (p = Q->q[WAIT_TIME]; p != NULL; p = p->next)
    {
        if (p->timer <= 0)
        {
            return p->tid;
        }
    }
    return -1;
}