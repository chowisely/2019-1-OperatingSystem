#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <execinfo.h>
#include <pthread.h>
#include <memory.h>
#include <errno.h>
#include <string.h>

#define TRUE 1
#define FALSE 0
#define MAX_LOCK 100
#define MAX_THR 10

typedef struct lock {
	pthread_mutex_t* addr; // address of a mutex
	int isLocked; // if a mutex is locked
	int beingModified; // if any operation is changing data of Lock
	pthread_t hold; // tid which holds a mutex
} Lock;

typedef struct Thrinfo {
	pthread_t tid;
	pthread_mutex_t* hold[100]; // list of mutexes that a thread holds
	pthread_mutex_t* acquire[100]; // list of mutexes that a thread has acquired
	int holdNum; // #mutexes in hold array
	int acqNum; // #mutexes in acquire array
	int visited; // indicate if a thread has been visited before to detect a cycle
} Thrinfo;

int findThread(pthread_t tid);
int findLock(pthread_mutex_t* mutex);
int findHoldIdx(int tIdx, pthread_mutex_t* m);
int findAcqIdx(int tIdx, pthread_mutex_t* m);
void rearrangeHold(int tIdx, int holdIdx);
void rearrangeAcq(int tIdx, int acqIdx);
int isCyclic();
int recursive(int lNum);


Lock *lock = 0x0;
Thrinfo *tinfo = 0x0;
int tNum = 0;
int lNum = 0;


int findThread(pthread_t tid) { // find which index of Thrinfo array has the specific thread info
	for(int i = 0; i < 10; i++) {
		if(pthread_equal(tid, tinfo[i].tid) != 0)
			return i;
	}
	return -1;
}

int findLock(pthread_mutex_t* mutex) { // find which index of Lock array has the specific lock info
	for(int i = 0; i < 100; i++) {
		if(lock[i].addr == mutex)
			return i;
	}
	return -1;
}

int findHoldIdx(int tIdx, pthread_mutex_t* m) { // find which index of hold array has the specific mutex
	for(int i = 0; i < tinfo[tIdx].holdNum; i++) {
		if(tinfo[tIdx].hold[i] == m)
			return i;
	}
	return -1;
}

int findAcqIdx(int tIdx, pthread_mutex_t* m) { // find which index of acquire array has the specific mutex
	for(int i = 0; i < tinfo[tIdx].acqNum; i++) {
		if(tinfo[tIdx].acquire[i] == m)
			return i;
	}
	return -1;
}

void rearrangeHold(int tIdx, int holdIdx) {
	for(int i = holdIdx + 1; i < tinfo[tIdx].holdNum; i++)
		tinfo[tIdx].hold[i - 1] = tinfo[tIdx].hold[i];
	tinfo[tIdx].holdNum--;
}

void rearrangeAcq(int tIdx, int acqIdx) {
	for(int i = acqIdx + 1; i < tinfo[tIdx].acqNum; i++)
		tinfo[tIdx].acquire[i - 1] = tinfo[tIdx].acquire[i];
	tinfo[tIdx].acqNum--;
}

int pthread_mutex_lock(pthread_mutex_t *mutex) {
	int (*lockp)(pthread_mutex_t *mutex);
	char *error;
	int ret;

	/* allocate memory to save lock info */
	if(lock == 0x0)
		lock = (Lock*)malloc(sizeof(Lock) * MAX_LOCK);
	/* allocate memory to save thread info */
	if(tinfo == 0x0)
		tinfo = (Thrinfo*)malloc(sizeof(Thrinfo) * MAX_THR);

	pthread_t tid = pthread_self();
	int tIdx = findThread(tid);
	int lIdx = findLock(mutex);

	/* initialization of thread info */
	if(tIdx == -1) {
		tinfo[tNum].tid = tid;
		tinfo[tNum].holdNum = 0;
		tinfo[tNum].acqNum = 0;
		tinfo[tNum].visited = FALSE;
		tIdx = findThread(tid);
		tNum++;
	}

	/* initialization of lock info */
	if(lIdx == -1) {
		lock[lNum].addr = mutex;
		lock[lNum].isLocked = FALSE;
		lock[lNum].beingModified = FALSE;
		lNum++;
		lIdx = findLock(mutex);
	}

	/* dynamic loading at runtime */
	lockp = dlsym(RTLD_NEXT, "pthread_mutex_lock");
	if((error = dlerror()) != 0x0)
		exit(1);

	ret = pthread_mutex_trylock(mutex); // try to hold a mutex
																		// do not wait for it if it's not available

	/* if a thread holds a lock successfully */
	if(ret == 0) {
		tinfo[tIdx].acquire[tinfo[tIdx].acqNum++] = mutex;
		tinfo[tIdx].hold[tinfo[tIdx].holdNum++] = mutex;
		lock[lIdx].isLocked = TRUE;
		lock[lIdx].hold = tid;

		return ret;
	}

	/* if a thread fails to hold a lock because it's locked */
	else if(ret == EBUSY) {
		tinfo[tIdx].acquire[tinfo[tIdx].acqNum++] = mutex;

		/* In case of falling into a deadlock state,
		* check if pthread_mutex_lock() leads to any deadlock
		* before executing the operation.
		*/
		if(isCyclic()) {
			fputs("*** Deadlock Detected ***\n", stderr);
			exit(0);
		}

		lockp(mutex); // pthread_mutex_lock()

		/* after it holds the lock, wait until pthread_mutex_unlock() is ended */
		while(lock[lIdx].beingModified == TRUE);

		/* add lock info */
		tinfo[tIdx].hold[tinfo[tIdx].holdNum++] = mutex;
		lock[lIdx].isLocked = TRUE;
		lock[lIdx].hold = tid;
	}

	return ret;
}

int pthread_mutex_unlock(pthread_mutex_t *mutex) {
	int (*unlockp)(pthread_mutex_t *mutex);
	char *error;
	int ret;

	/* allocate memory to save lock info */
	if(lock == 0x0)
		lock = (Lock*)malloc(sizeof(Lock) * MAX_LOCK);
	/* allocate memory to save thread info */
	if(tinfo == 0x0)
		tinfo = (Thrinfo*)malloc(sizeof(Thrinfo) * MAX_THR);

	int tIdx = findThread(pthread_self());
	int lIdx = findLock(mutex);
	int holdIdx = findHoldIdx(tIdx, mutex);
	int acqIdx = findAcqIdx(tIdx, mutex);

	/* dynamic loading at runtime */
	unlockp = dlsym(RTLD_NEXT, "pthread_mutex_unlock");
	if((error = dlerror()) != 0x0)
		exit(1);

	/* to prevent a waiting thread from modifying related data before this operation is completely finished */
	lock[lIdx].beingModified = TRUE;

	ret = unlockp(mutex); // pthread_mutex_unlock()

	/* if a thread successfully releases a lock */
	if(ret == 0) {
		tinfo[tIdx].hold[holdIdx] = 0;
		tinfo[tIdx].acquire[acqIdx] = 0;
		lock[lIdx].isLocked = FALSE;
		rearrangeHold(tIdx, holdIdx);
		rearrangeAcq(tIdx, acqIdx);

		lock[lIdx].beingModified = FALSE; // let the waiting thread modify lock info
	}
	return ret;
}

int isCyclic() {
	int i, j;

	/* initialization */
	for(i = 0; i < tNum; i++)
		tinfo[i].visited = FALSE;

	for(i = 0; i < lNum; i++) {
		if(lock[i].isLocked && !lock[i].beingModified) {
			if(recursive(i))
				return TRUE;

			/* re-initialization every time it sets a lock as a start node */
			for(j = 0; j < tNum; j++)
				tinfo[j].visited = FALSE;
		}
	}
	return FALSE;
}


int recursive(int lNum) {
	int i, j;
	int tIdx = findThread(lock[lNum].hold);
	int tmp;

	if(tinfo[tIdx].visited) // if it is a lock we've already visited
		return TRUE;

	if(!lock[lNum].isLocked) // if it is a lock not helded by any thread
		return FALSE;

	tinfo[tIdx].visited = TRUE; // mark a lock as visited

	/* find a lock that a thread doesn't hold and waits for */
	for(i = 0; i < tinfo[tIdx].acqNum; i++) {
		for (j = 0; j < tinfo[tIdx].holdNum; j++) {
			if(tinfo[tIdx].acquire[i] == tinfo[tIdx].hold[j]) // a thread already holds a lock
				break;
		}

		/* if there's no corresponding mutex in hold, it means a thread doesn't hold it yet */
		if(j == tinfo[tIdx].holdNum && lock[findLock(tinfo[tIdx].acquire[i])].isLocked) {
			if(recursive(findLock(tinfo[tIdx].acquire[i])))
				return TRUE;
		}
	}
	return FALSE; // a thread holds every lock so it doesn't wait for any lock
}

