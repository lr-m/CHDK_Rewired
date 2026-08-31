#ifndef TASK_H
#define TASK_H

// CHDK Task interface

// Note: used in modules and platform independent code. 
// Do not add platform dependent stuff in here (#ifdef/#endif compile options or camera dependent values)

extern int CreateTask (const char *name, int prio, int stack_size, void *entry);
extern void ExitTask();

extern void SleepTask(long msec);

// extern long taskLock();
// extern long taskUnlock();

// extern long task_lock();
// extern long task_unlock();
extern const char *task_name(int id);
int task_id_list_get(int *idlist,int size);

#endif

