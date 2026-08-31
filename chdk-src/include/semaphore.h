#ifndef SEMAPHORE_H
#define SEMAPHORE_H

// Note: used in modules and platform independent code.
// Do not add platform dependent stuff in here (#ifdef/#endif compile options or camera dependent values)

//==========================================================

// Semaphore & Assert

extern void DebugAssert(char*, int);

extern int CreateBinarySemaphore(char* name, int init);
extern int TakeSemaphore(int sem, int timeout);
extern void GiveSemaphore(int sem);
extern void DeleteSemaphore(int sem);

//---------------------------------------------------------------

#endif
