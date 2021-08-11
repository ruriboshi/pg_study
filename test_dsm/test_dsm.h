/*
 * test_dsm.h
 *
 */

#ifndef TEST_DSM_H
#define TEST_DSM_H

#include "postmaster/bgworker.h"
#include "storage/dsm.h"
#include "storage/shm_mq.h"
#include "storage/shm_toc.h"
#include "storage/spin.h"

/* Magic number to identify the TOC used by this extension. */
#define TEST_DSM_MAGIC		0x71578a3b

/* Magic number to identify the chunk. */
#define DSM_KEY_DSO			UINT64CONST(0xFFFFFFFFFFFF0001)
#define	DSM_KEY_DMQ			UINT64CONST(0xFFFFFFFFFFFF0002)

#define	MESSAGE_QUEUE_SIZE	16384


/*
 * The information of background worker.
 */
typedef struct BackgroundWorkerInfo
{
	BackgroundWorkerHandle	*handle;
	shm_mq_handle			*mq_handle;
	int32		pid;
} BackgroundWorkerInfo;

/*
 * The information of the dynamic shared memory and background worker.
 */
typedef struct DynamicSharedInfo
{
	int			 	 nworkers;
	dsm_segment		*seg;
	shm_toc			*toc;
	BackgroundWorkerInfo	*worker;
} DynamicSharedInfo;

typedef enum
{
	PS_INITIAL,
	PS_FINISHED
} ParallelStatus;

/*
 * This struct object would be stored in the dynamic shared memory, and
 * a backend process and background workers are able to use it.
 */
typedef struct DynamicSharedObject
{
	ParallelStatus	status;

	slock_t	mutex;
	int		attached_workers;
} DynamicSharedObject;


/* Unique number for identifying each worker. */
extern PGDLLIMPORT int	MyNumber;

/* exported functions */
extern DynamicSharedInfo *CreateDynamicSharedMemory(int nworkers);
extern void BackgroundWorkerMain(Datum main_arg);

#endif	/* TEST_DSM_H */
