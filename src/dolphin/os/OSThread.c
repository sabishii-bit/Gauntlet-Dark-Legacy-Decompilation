#include "types.h"
#include "dolphin/os/OSThread.h"

int OSDisableInterrupts(void);
int OSEnableInterrupts(void);
int OSRestoreInterrupts(int level);

extern unsigned char _stack_end[];
extern unsigned char _stack_addr[];

OSThread* __gUnkThread1 : 0x800000D8;
OSThreadQueue __OSActiveThreadQueue : 0x800000DC;
OSThread* __gCurrentThread : 0x800000E4;

#define ENQUEUE_THREAD(thread, queue, link)      \
    do {                                         \
        struct OSThread* __prev = (queue)->tail; \
        if (__prev == NULL) {                    \
            (queue)->head = (thread);            \
        } else {                                 \
            __prev->link.next = (thread);        \
        }                                        \
        (thread)->link.prev = __prev;            \
        (thread)->link.next = 0;                 \
        (queue)->tail = (thread);                \
    } while (0);

#define DEQUEUE_THREAD(thread, queue, link)            \
    do {                                               \
        struct OSThread* __next = (thread)->link.next; \
        struct OSThread* __prev = (thread)->link.prev; \
        if (__next == NULL) {                          \
            (queue)->tail = __prev;                    \
        } else {                                       \
            __next->link.prev = __prev;                \
        }                                              \
        if (__prev == NULL) {                          \
            (queue)->head = __next;                    \
        } else {                                       \
            __prev->link.next = __next;                \
        }                                              \
    } while (0);

#define ENQUEUE_THREAD_PRIO(thread, queue, link)                                \
    do {                                                                        \
        struct OSThread* __prev;                                                \
        struct OSThread* __next;                                                \
        for (__next = (queue)->head;                                            \
             __next && (__next->priority <= (thread)->priority);                \
             __next = __next->link.next)                                        \
            ;                                                                   \
                                                                                \
        if (__next == NULL) {                                                   \
            ENQUEUE_THREAD(thread, queue, link);                                \
        } else {                                                                \
            (thread)->link.next = __next;                                       \
            __prev = __next->link.prev;                                         \
            __next->link.prev = (thread);                                       \
            (thread)->link.prev = __prev;                                       \
            if (__prev == NULL) {                                               \
                (queue)->head = (thread);                                       \
            } else {                                                            \
                __prev->link.next = (thread);                                   \
            }                                                                   \
        }                                                                       \
    } while (0);

#define DEQUEUE_HEAD(thread, queue, link)             \
    do {                                              \
        struct OSThread* __next = thread->link.next;  \
        if (__next == NULL) {                         \
            (queue)->tail = 0;                        \
        } else {                                      \
            __next->link.prev = 0;                    \
        }                                             \
        (queue)->head = __next;                       \
    } while (0);

static struct OSThreadQueue RunQueue[32];
static struct OSThread IdleThread;
static struct OSThread DefaultThread;
static struct OSContext IdleContext;
static volatile unsigned long RunQueueBits;
static volatile int RunQueueHint;
static long Reschedule;

static void __OSSwitchThread(struct OSThread* nextThread);
static void SetRun(struct OSThread* thread);
static void UnsetRun(struct OSThread* thread);
static struct OSThread* SetEffectivePriority(struct OSThread* thread, long priority);
static void UpdatePriority(struct OSThread* thread);
static struct OSThread* SelectThread(int yield);
int OSCreateThread(struct OSThread* thread, void* (*func)(void*), void* param, void* stack,
                   unsigned long stackSize, long priority, unsigned short attr);
void OSExitThread(void* val);

void __OSThreadInit() {
    struct OSThread* thread = &DefaultThread;
    int prio;

    thread->state = 2;
    thread->attr = 1;
    thread->priority = thread->base = 0x10;
    thread->suspend = 0;
    thread->val = (void*)-1;
    thread->mutex = 0;

    OSInitThreadQueue(&thread->queueJoin);
    thread->queueMutex.head = thread->queueMutex.tail = 0;

    __gUnkThread1 = thread;
    OSClearContext(&thread->context);
    OSSetCurrentContext(&thread->context);
    thread->stackBase = (unsigned char*)&_stack_addr;
    thread->stackEnd = (unsigned long*)&_stack_end;
    *(u32*)thread->stackEnd = 0xDEADBABE;
    __gCurrentThread = thread;
    RunQueueBits = 0;
    RunQueueHint = 0;

    for (prio = 0; prio <= 31; prio++) {
        OSInitThreadQueue(&RunQueue[prio]);
    }
    OSInitThreadQueue(&__OSActiveThreadQueue);

    ENQUEUE_THREAD(thread, &__OSActiveThreadQueue, linkActive);

    OSClearContext(&IdleContext);
    Reschedule = 0;
}

void OSInitThreadQueue(struct OSThreadQueue* queue) {
    queue->head = queue->tail = 0;
}

struct OSThread* OSGetCurrentThread() {
    return __gCurrentThread;
}

static void __OSSwitchThread(struct OSThread* nextThread) {
    __gCurrentThread = nextThread;
    OSSetCurrentContext(&nextThread->context);
    OSLoadContext(&nextThread->context);
}

long OSDisableScheduler(void) {
    register int enabled;
    long count;

    enabled = OSDisableInterrupts();
    count = Reschedule;
    Reschedule = count + 1;
    OSRestoreInterrupts(enabled);
    return count;
}

long OSEnableScheduler(void) {
    register int enabled;
    long count;

    enabled = OSDisableInterrupts();
    count = Reschedule;
    Reschedule = count - 1;
    OSRestoreInterrupts(enabled);
    return count;
}

static void SetRun(struct OSThread* thread) {
    thread->queue = &RunQueue[thread->priority];

    ENQUEUE_THREAD(thread, thread->queue, link);

    RunQueueBits |= 1 << (0x1F - thread->priority);
    RunQueueHint = 1;
}

#pragma dont_inline on
static void UnsetRun(struct OSThread* thread) {
    struct OSThreadQueue* queue;

    queue = thread->queue;

    DEQUEUE_THREAD(thread, queue, link);

    if (!queue->head) {
        RunQueueBits &= ~(1 << (0x1F - thread->priority));
    }
    thread->queue = NULL;
}
#pragma dont_inline off

long __OSGetEffectivePriority(struct OSThread* thread) {
    long priority = thread->base;
    struct OSMutex* mutex;

    for (mutex = thread->queueMutex.head; mutex; mutex = mutex->link.next) {
        struct OSThread* blocked = mutex->queue.head;
        if (blocked && blocked->priority < priority) {
            priority = blocked->priority;
        }
    }
    return priority;
}

static struct OSThread* SetEffectivePriority(struct OSThread* thread, long priority) {
    switch (thread->state) {
    case 1:
        UnsetRun(thread);
        thread->priority = priority;
        SetRun(thread);
        break;
    case 4:
        DEQUEUE_THREAD(thread, thread->queue, link);
        thread->priority = priority;

        ENQUEUE_THREAD_PRIO(thread, thread->queue, link);

        if (thread->mutex) {
            return thread->mutex->thread;
        }
        break;
    case 2:
        RunQueueHint = 1;
        thread->priority = priority;
        break;
    }
    return 0;
}

static void UpdatePriority(struct OSThread* thread) {
    long priority;

    while (1) {
        if (thread->suspend > 0) {
            break;
        }
        priority = __OSGetEffectivePriority(thread);
        if (thread->priority == priority) {
            break;
        }
        thread = SetEffectivePriority(thread, priority);
        if (thread == 0) {
            break;
        }
    }
}

void __OSPromoteThread(struct OSThread* thread, long priority) {
    while (1) {
        if (thread->suspend > 0 || thread->priority <= priority) {
            break;
        }
        thread = SetEffectivePriority(thread, priority);
        if (thread == 0) {
            break;
        }
    }
}

static struct OSThread* SelectThread(int yield) {
    struct OSContext* currentContext;
    struct OSThread* currentThread;
    struct OSThread* nextThread;
    long priority;
    struct OSThreadQueue* queue;

    if (Reschedule > 0) {
        return NULL;
    }

    currentContext = OSGetCurrentContext();
    currentThread = OSGetCurrentThread();

    if (currentContext != &currentThread->context) {
        return NULL;
    }

    if (currentThread) {
        if (currentThread->state == 2) {
            if (yield == 0) {
                priority = __cntlzw(RunQueueBits);
                if (currentThread->priority <= priority) {
                    return NULL;
                }
            }
            currentThread->state = 1;
            SetRun(currentThread);
        }
        if (!(currentThread->context.state & 2) && (OSSaveContext(&currentThread->context) != 0)) {
            return NULL;
        }
    }

    __gCurrentThread = 0;

    if (RunQueueBits == 0) {
        OSSetCurrentContext(&IdleContext);
        do {
            OSEnableInterrupts();
            while (RunQueueBits == 0)
                ;
            OSDisableInterrupts();
        } while (RunQueueBits == 0);
        OSClearContext(&IdleContext);
    }

    RunQueueHint = 0;
    priority = __cntlzw(RunQueueBits);

    queue = &RunQueue[priority];
    nextThread = queue->head;

    DEQUEUE_HEAD(nextThread, queue, link);

    if (!queue->head) {
        RunQueueBits &= ~(1 << (0x1F - priority));
    }
    nextThread->queue = 0;
    nextThread->state = 2;
    __OSSwitchThread(nextThread);
    return nextThread;
}

void __OSReschedule(void) {
    if (RunQueueHint != 0) {
        SelectThread(0);
    }
}

int OSCreateThread(struct OSThread* thread, void* (*func)(void*), void* param, void* stack,
                   unsigned long stackSize, long priority, unsigned short attr) {
    int enabled;
    unsigned long sp;

    if ((priority < 0) || (priority > 0x1F)) {
        return 0;
    }

    thread->state = 1;
    thread->attr = attr & 1U;
    thread->base = priority;
    thread->priority = priority;
    thread->suspend = 1;
    thread->val = (void*)-1;
    thread->mutex = 0;
    OSInitThreadQueue(&thread->queueJoin);
    OSInitThreadQueue((void*)&thread->queueMutex);
    sp = (u32)stack;
    sp &= ~7;
    sp -= 8;
    ((u32*)sp)[0] = 0;
    ((u32*)sp)[1] = 0;
    OSInitContext(&thread->context, (u32)func, sp);
    thread->context.lr = (unsigned long)&OSExitThread;
    thread->context.gpr[3] = (unsigned long)param;
    thread->stackBase = stack;
    thread->stackEnd = (void*)((unsigned int)stack - stackSize);
    *thread->stackEnd = 0xDEADBABE;
    enabled = OSDisableInterrupts();

    ENQUEUE_THREAD(thread, &__OSActiveThreadQueue, linkActive);

    OSRestoreInterrupts(enabled);
    return 1;
}

void OSExitThread(void* val) {
    int enabled = OSDisableInterrupts();
    struct OSThread* currentThread = OSGetCurrentThread();

    OSClearContext(&currentThread->context);
    if (currentThread->attr & 1) {
        DEQUEUE_THREAD(currentThread, &__OSActiveThreadQueue, linkActive);
        currentThread->state = 0;
    } else {
        currentThread->state = 8;
        currentThread->val = val;
    }
    __OSUnlockAllMutex(currentThread);
    OSWakeupThread(&currentThread->queueJoin);
    RunQueueHint = 1;
    if (RunQueueHint != 0) {
        SelectThread(0);
    }
    OSRestoreInterrupts(enabled);
}

void OSCancelThread(struct OSThread* thread) {
    int enabled = OSDisableInterrupts();

    switch (thread->state) {
    case 1:
        if (thread->suspend <= 0) {
            UnsetRun(thread);
        }
        break;
    case 2:
        RunQueueHint = 1;
        break;
    case 4:
        DEQUEUE_THREAD(thread, thread->queue, link);
        thread->queue = 0;
        if ((thread->suspend <= 0) && (thread->mutex)) {
            UpdatePriority(thread->mutex->thread);
        }
        break;
    default:
        OSRestoreInterrupts(enabled);
        return;
    }
    OSClearContext(&thread->context);
    if (thread->attr & 1) {
        DEQUEUE_THREAD(thread, &__OSActiveThreadQueue, linkActive);
        thread->state = 0;
    } else {
        thread->state = 8;
    }
    __OSUnlockAllMutex(thread);
    OSWakeupThread(&thread->queueJoin);
    __OSReschedule();
    OSRestoreInterrupts(enabled);
}

long OSResumeThread(struct OSThread* thread) {
    int enabled = OSDisableInterrupts();
    long suspendCount;

    suspendCount = thread->suspend--;
    if (thread->suspend < 0) {
        thread->suspend = 0;
    } else if (thread->suspend == 0) {
        switch (thread->state) {
        case 1:
            thread->priority = __OSGetEffectivePriority(thread);
            SetRun(thread);
            break;
        case 4:
            DEQUEUE_THREAD(thread, thread->queue, link);
            thread->priority = __OSGetEffectivePriority(thread);
            ENQUEUE_THREAD_PRIO(thread, thread->queue, link);
            if (thread->mutex) {
                UpdatePriority(thread->mutex->thread);
            }
        }
        __OSReschedule();
    }
    OSRestoreInterrupts(enabled);
    return suspendCount;
}

long OSSuspendThread(struct OSThread* thread) {
    int enabled = OSDisableInterrupts();
    long suspendCount;

    suspendCount = thread->suspend++;
    if (suspendCount == 0) {
        switch (thread->state) {
        case 2:
            RunQueueHint = 1;
            thread->state = 1;
            break;
        case 1:
            UnsetRun(thread);
            break;
        case 4:
            DEQUEUE_THREAD(thread, thread->queue, link);
            thread->priority = 0x20;
            ENQUEUE_THREAD(thread, thread->queue, link);
            if (thread->mutex) {
                UpdatePriority(thread->mutex->thread);
            }
            break;
        }
        __OSReschedule();
    }
    OSRestoreInterrupts(enabled);
    return suspendCount;
}

void OSSleepThread(struct OSThreadQueue* queue) {
    int enabled = OSDisableInterrupts();
    struct OSThread* currentThread = OSGetCurrentThread();

    currentThread->state = 4;
    currentThread->queue = queue;
    ENQUEUE_THREAD_PRIO(currentThread, queue, link);
    RunQueueHint = 1;
    __OSReschedule();
    OSRestoreInterrupts(enabled);
}

void OSWakeupThread(struct OSThreadQueue* queue) {
    int enabled = OSDisableInterrupts();

    while (queue->head) {
        struct OSThread* thread = queue->head;

        DEQUEUE_HEAD(thread, queue, link);

        thread->state = 1;
        if (thread->suspend <= 0) {
            SetRun(thread);
        }
    }
    __OSReschedule();
    OSRestoreInterrupts(enabled);
}

