#ifndef OS2021_API_H
#define OS2021_API_H

#define _XOPEN_SOURCE 600
#define STACK_SIZE 40960

#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include <sys/time.h>
#include <signal.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include "function_libary.h"
#include "feedback_queue.h"

int OS2021_ThreadCreate(char *job_name, char *p_function, char *priority, int cancel_mode);
void OS2021_ThreadCancel(char *job_name);
void OS2021_ThreadWaitEvent(int event_id);
void OS2021_ThreadSetEvent(int event_id);
void OS2021_ThreadWaitTime(int msec);
void OS2021_DeallocateThreadResource();
void OS2021_TestCancel();

void CreateContext(ucontext_t *, ucontext_t *, void *);
void ResetTimer();
void Dispatcher();
void timeout_handler(void);
void signal_handler(int signal);
void StartSchedulingSimulation();
void queue_init_threads(void);
void (*get_function_handle(const char *p_function))(void);
int get_time_quantum(Prior c_priority);
Thread *find_thread_by_name(const char *name);
bool p_function_is_valid(const char *);
Prior priority_stoi(const char *);
void print_thread_status(void);
char *state_itos(State state);
char priority_itos(Prior prior);

#endif
