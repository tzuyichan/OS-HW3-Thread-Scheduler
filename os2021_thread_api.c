#include <stdio.h>
#include <stdlib.h>
#include <json-c/json.h>
#include "os2021_thread_api.h"

#define JSON_BUF_SIZE 4096
#define MAX_STR_LEN 128
#define MAX_THREAD_NUM 64
#define IT_INTERVAL_MSEC 10
#define USEC_TO_MSEC 1000

#define FAIL_IF(EXP, MSG)              \
    {                                  \
        if (EXP)                       \
        {                              \
            fprintf(stderr, MSG "\n"); \
            exit(EXIT_FAILURE);        \
        }                              \
    }

int tid_counter = 0;
int thread_count = 0; // number of threads at a given point in time
Queue *Q;
Thread *Running;
Thread *thread_list[MAX_THREAD_NUM];

struct sigaction sa;
struct itimerval Signaltimer;
ucontext_t dispatch_ctx;
ucontext_t timeout_ctx;
void (*func[6])() = {Function1, Function2, Function3, Function4, Function5, ResourceReclaim};

char running[] = "Running";
char ready[] = "Ready";
char waiting[] = "Waiting";

int OS2021_ThreadCreate(char *job_name, char *p_function, char *priority, int cancel_mode)
{
    if (!p_function_is_valid(p_function))
        return -1;

    int p = priority_stoi(priority);
    Thread *T = init_thread(tid_counter, job_name, p_function, p, cancel_mode);
    CreateContext(&T->ctx, NULL, get_function_handle(T->p_func));
    enqueue(Q, T, READY, 0);

    tid_counter++;
    thread_count++;

    return tid_counter;
}

void OS2021_ThreadCancel(char *job_name)
{
    Thread *T = find_thread_by_name(job_name);
    if (!T)
    {
        printf("Cannot find thread %s to cancel\n", job_name);
        fflush(stdout);
        return;
    }

    T->am_cancelled = true;

    if (T->cancel_mode == 0)
    {
        State original_state = T->state;
        T->state = TERMINATED;
        enqueue(Q, T, TERMINATED, 0);
        if (T->tid == Running->tid)
        {
            swapcontext(&Running->ctx, &dispatch_ctx);
        }
        else
        {
            dequeue(Q, original_state, T->c_priority, T->event_id);
        }
    }
}

void OS2021_ThreadWaitEvent(int event_id)
{
    printf("%s wants to wait for event %d\n", Running->name, event_id);
    fflush(stdout);

    int time_quantum = get_time_quantum(Running->c_priority);
    if (Running->elapsed < time_quantum)
    {
        if (Running->c_priority != HIGH)
        {
            printf("The priority of %s changed from %d to %d\n",
                   Running->name, Running->c_priority, Running->c_priority - 1);
            fflush(stdout);
            Running->c_priority--;
        }
    }

    Running->event_id = event_id;
    Running->elapsed = 0;
    Running->state = WAITING;
    enqueue(Q, Running, Running->state, Running->event_id);
    swapcontext(&Running->ctx, &dispatch_ctx);
}

void OS2021_ThreadSetEvent(int event_id)
{
    Thread *T;

    if ((T = dequeue_set_event(Q, event_id)))
    {
        T->state = READY;
        T->event_id = 0;
        enqueue(Q, T, T->state, T->event_id);
        printf("%s changed the state of %s to READY\n", Running->name, T->name);
        fflush(stdout);
    }
}

void OS2021_ThreadWaitTime(int msec)
{
    printf("%s wants to wait for %d ms\n", Running->name, msec * 10);
    fflush(stdout);

    int time_quantum = get_time_quantum(Running->c_priority);
    if (Running->elapsed < time_quantum)
    {
        if (Running->c_priority != HIGH)
        {
            printf("The priority of %s changed from %d to %d\n",
                   Running->name, Running->c_priority, Running->c_priority - 1);
            fflush(stdout);
            Running->c_priority--;
        }
    }

    Running->timer = msec * IT_INTERVAL_MSEC;
    Running->elapsed = 0;
    Running->state = WAITING;
    enqueue(Q, Running, WAITING, 8); // wait time queue is one behind [wait, low, 7]
    swapcontext(&Running->ctx, &dispatch_ctx);
}

void OS2021_DeallocateThreadResource()
{
    Thread *T = dequeue(Q, TERMINATED, 0, 0);
    if (T)
    {
        free(T->ctx.uc_stack.ss_sp);
        free(T);
    }
}

void OS2021_TestCancel()
{
    if (Running->am_cancelled)
    {
        Running->state = TERMINATED;
        enqueue(Q, Running, TERMINATED, 0);
        swapcontext(&Running->ctx, &dispatch_ctx);
    }
}

void CreateContext(ucontext_t *context, ucontext_t *next_context, void *func)
{
    getcontext(context);
    context->uc_stack.ss_sp = malloc(STACK_SIZE);
    context->uc_stack.ss_size = STACK_SIZE;
    context->uc_stack.ss_flags = 0;
    context->uc_link = next_context;
    makecontext(context, (void (*)(void))func, 0);
}

void (*get_function_handle(const char *p_function))(void)
{
    if (strncmp(p_function, "Function1", MAX_STR_LEN) == 0)
        return func[0];
    if (strncmp(p_function, "Function2", MAX_STR_LEN) == 0)
        return func[1];
    if (strncmp(p_function, "Function3", MAX_STR_LEN) == 0)
        return func[2];
    if (strncmp(p_function, "Function4", MAX_STR_LEN) == 0)
        return func[3];
    if (strncmp(p_function, "Function5", MAX_STR_LEN) == 0)
        return func[4];
    if (strncmp(p_function, "ResourceReclaim", MAX_STR_LEN) == 0)
        return func[5];
    return NULL; // should never return null
}

int get_time_quantum(Prior c_priority)
{
    if (c_priority == HIGH)
        return HIGH_TQ;
    else if (c_priority == MEDIUM)
        return MEDIUM_TQ;
    else
        return LOW_TQ;
}

Thread *find_thread_by_name(const char *name)
{
    if (strncmp(name, "reclaimer", MAX_STR_LEN) == 0)
        return NULL;

    int count, i = 0;
    count = fill_thread_id_list(Q, Running, thread_list);

    for (i = 0; i < count; ++i)
    {
        if (strncmp(thread_list[i]->name, name, MAX_STR_LEN) == 0)
            return thread_list[i];
    }

    return NULL;
}

void ResetTimer()
{
    Signaltimer.it_value.tv_usec = IT_INTERVAL_MSEC * USEC_TO_MSEC;
    Signaltimer.it_value.tv_sec = 0;
    if (setitimer(ITIMER_REAL, &Signaltimer, NULL) < 0)
    {
        printf("ERROR SETTING TIME SIGALRM!\n");
        fflush(stdout);
    }
}

void Dispatcher()
{
    //printf("Hello this is the dispatcher!\n");
    //fflush(stdout);

    Thread *T;

    for (int priority = 0; priority < N_PRIOR_LVL; ++priority)
    {
        if ((T = dequeue(Q, READY, priority, 0)))
            break;
    }
    Running = T;
    Running->state = RUNNING;
    //printf("Current running %s\n", Running->name);
    //fflush(stdout);
    setcontext(&Running->ctx);
}

void timeout_handler(void)
{
    /* increment queue_time and wait_time */
    int count = fill_thread_id_list(Q, Running, thread_list);
    for (int i = 0; i < count; ++i)
    {
        if (thread_list[i]->state == READY)
        {
            thread_list[i]->queue_time += IT_INTERVAL_MSEC;
        }
        if (thread_list[i]->state == WAITING)
        {
            thread_list[i]->wait_time += IT_INTERVAL_MSEC;
        }
    }

    /* handle wait timeout threads */
    Thread *p;
    for (p = Q->q[WAIT_TIME]; p != NULL; p = p->next)
    {
        p->timer -= IT_INTERVAL_MSEC;
        //printf("%s has %d ms left\n", p->name, p->timer);
    }

    int tid;
    while ((tid = next_wait_timeout_thread(Q)) >= 0)
    {
        p = dequeue_wait_time(Q, tid);
        p->timer = 0;
        p->event_id = 0;
        p->state = READY;
        enqueue(Q, p, p->state, 0);
    }

    /* handle running thread */
    int time_quantum = get_time_quantum(Running->c_priority);

    if (Running->elapsed >= time_quantum)
    {
        Running->state = READY;
        Running->event_id = 0;
        // lower priority
        if (Running->c_priority != LOW)
        {
            printf("The priority of %s changed from %d to %d\n",
                   Running->name, Running->c_priority, Running->c_priority + 1);
            fflush(stdout);
            Running->c_priority++;
        }
        Running->elapsed = 0;
        // queue to lower priority
        enqueue(Q, Running, READY, 0);
        setcontext(&dispatch_ctx);
    }
    else
    {
        Running->elapsed += IT_INTERVAL_MSEC;
        //printf("has run for %d ms\n", Running->elapsed);
        //fflush(stdout);
        setcontext(&Running->ctx);
    }
}

void signal_handler(int signal)
{
    if (signal == SIGALRM)
    {
        swapcontext(&Running->ctx, &timeout_ctx);
    }

    if (signal == SIGTSTP)
    {
        print_thread_status();
    }
}

void StartSchedulingSimulation()
{
    sa.sa_handler = signal_handler;
    sigaction(SIGTSTP, &sa, NULL);
    sigaction(SIGALRM, &sa, NULL);

    queue_init_threads();

    /*Set Timer*/
    Signaltimer.it_interval.tv_usec = IT_INTERVAL_MSEC * USEC_TO_MSEC;
    Signaltimer.it_interval.tv_sec = 0;

    /*Create Context*/
    CreateContext(&dispatch_ctx, NULL, &Dispatcher);
    CreateContext(&timeout_ctx, NULL, &timeout_handler);

    ResetTimer();
    setcontext(&dispatch_ctx);
}

void queue_init_threads(void)
{
    Q = create_queue();

    FILE *fp;
    char json_buffer[JSON_BUF_SIZE];
    struct json_object *parsed_json;
    struct json_object *threads;
    struct json_object *thread;
    struct json_object *name;
    struct json_object *entry_func;
    struct json_object *priority;
    struct json_object *cancel_mode;
    size_t n_threads;

    fp = fopen("init_threads.json", "r");
    FAIL_IF(!fp, "Failed to open file.");
    fread(json_buffer, sizeof(char), JSON_BUF_SIZE, fp);
    fclose(fp);

    parsed_json = json_tokener_parse(json_buffer);

    json_object_object_get_ex(parsed_json, "Threads", &threads);
    n_threads = json_object_array_length(threads);

    for (int i = 0; i < n_threads; ++i)
    {
        thread = json_object_array_get_idx(threads, i);

        json_object_object_get_ex(thread, "name", &name);
        json_object_object_get_ex(thread, "entry function", &entry_func);
        json_object_object_get_ex(thread, "priority", &priority);
        json_object_object_get_ex(thread, "cancel mode", &cancel_mode);

        OS2021_ThreadCreate((char *)json_object_get_string(name),
                            (char *)json_object_get_string(entry_func),
                            (char *)json_object_get_string(priority),
                            json_object_get_int(cancel_mode));
    }
    OS2021_ThreadCreate("reclaimer", "ResourceReclaim", "L", 1);
}

bool p_function_is_valid(const char *p_function)
{
    if (strncmp(p_function, "Function1", MAX_STR_LEN) == 0 ||
            strncmp(p_function, "Function2", MAX_STR_LEN) == 0 ||
            strncmp(p_function, "Function3", MAX_STR_LEN) == 0 ||
            strncmp(p_function, "Function4", MAX_STR_LEN) == 0 ||
            strncmp(p_function, "Function5", MAX_STR_LEN) == 0 ||
            strncmp(p_function, "ResourceReclaim", MAX_STR_LEN) == 0)
    {
        return true;
    }
    return false;
}

Prior priority_stoi(const char *priority)
{
    switch (*priority)
    {
    case 'H':
        return HIGH;
    case 'M':
        return MEDIUM;
    case 'L':
        return LOW;
    default:
        FAIL_IF(true, "Unknown priority level.");
    }
}

void print_thread_status(void)
{

    int count = fill_thread_id_list(Q, Running, thread_list);

    char tid[] = "TID";
    char name[] = "Name";
    char state[] = "State";
    char b_prior[] = "B_Priority";
    char c_prior[] = "C_Priority";
    char q_time[] = "Q_Time";
    char w_time[] = "W_Time";
    printf("\n---------------------------------------------------------------------------\n");
    printf("%-10s"
           "%-14s"
           "%-10s"
           "%-12s"
           "%-12s"
           "%-10s"
           "%-10s"
           "\n",
           tid, name, state, b_prior, c_prior, q_time, w_time);

    for (int i = 0; i < count; ++i)
    {
        printf("%-10d"
               "%-14s"
               "%-10s"
               "%-12c"
               "%-12c"
               "%-10d"
               "%-10d"
               "\n",
               thread_list[i]->tid,
               thread_list[i]->name,
               state_itos(thread_list[i]->state),
               priority_itos(thread_list[i]->b_priority),
               priority_itos(thread_list[i]->c_priority),
               thread_list[i]->queue_time,
               thread_list[i]->wait_time);
    }
    printf("---------------------------------------------------------------------------\n");
    fflush(stdout);
}
char *state_itos(State state)
{
    if (state == RUNNING)
        return running;
    else if (state == READY)
        return ready;
    else
        return waiting;
}

char priority_itos(Prior prior)
{
    if (prior == HIGH)
        return 'H';
    else if (prior == MEDIUM)
        return 'M';
    else
        return 'L';
}
