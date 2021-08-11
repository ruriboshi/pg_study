/*-------------------------------------------------------------------------
 *
 * bgworker.c
 *
 * A worker process attaches a queue on the dynamic shared memory and
 * reads a message.
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "miscadmin.h"
#include "tcop/tcopprot.h"
#include "utils/memutils.h"
#include "utils/resowner.h"

#include "test_dsm.h"

/*
 * Unique number given each worker which is used for attaching
 * a queue on the dynamic shared memory(shm_mq).
 */
int		MyNumber = -1;

/*
 * Entry point for a background worker.
 *
 * Please see LaunchBackgroundWorkers() in test_dsm.c
 */
void
BackgroundWorkerMain(Datum main_arg)
{
	dsm_segment			*seg;
	shm_toc				*toc;
	shm_mq				*mq;
	shm_mq_handle		*mq_handle;
	shm_mq_result		 res;
	char				*mq_space;
	Size				 nbytes;
	void				*data;
	DynamicSharedObject	*dso;

	/* Establish signal handlers; once that's done, unblock signals. */
	pqsignal(SIGTERM, die);
	BackgroundWorkerUnblockSignals();

	/* Get the my unique number */
	Assert(MyNumber == -1);
	memcpy(&MyNumber, MyBgworkerEntry->bgw_extra, sizeof(int));

	/* 
	 * step 1. Set up a memory context and resource owner.
	 */
	Assert(CurrentResourceOwner == NULL);
	CurrentResourceOwner = ResourceOwnerCreate(NULL, "test_dsm");
	CurrentMemoryContext = AllocSetContextCreate(TopMemoryContext,
												 "worker for test_dsm",
												 ALLOCSET_DEFAULT_SIZES);

	/*
	 * step 2. Attach to the dynamic shared memory and get the
	 *			Table-Of-Contents(TOC).
	 */
	seg = dsm_attach(DatumGetUInt32(main_arg));
	if (seg == NULL)
		ereport(ERROR,
				(errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
				 errmsg("could not map dynamic shared memory segment")));
	toc = shm_toc_attach(TEST_DSM_MAGIC, dsm_segment_address(seg));
	if (toc == NULL)
		ereport(ERROR,
				(errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
				 errmsg("invalid magic number in dynamic shared memory segment")));

	/*
	 * step 3. Look up the chunk.
	 */
	dso = shm_toc_lookup(toc, DSM_KEY_DSO, false);

	/* Set up for using shared message queue. */
	mq_space = shm_toc_lookup(toc, DSM_KEY_DMQ, false);
	mq = (shm_mq *) (mq_space + MyNumber * MESSAGE_QUEUE_SIZE);
	shm_mq_set_receiver(mq, MyProc);
	mq_handle = shm_mq_attach(mq, seg, NULL);

	/*
	 * step 4. Do something.
	 */

	/* Increment a integer variable on the DSM atomically. */
	SpinLockInit(&dso->mutex);
	dso->attached_workers++;
	SpinLockRelease(&dso->mutex);

	/* Read data from the message queue. */
	res = shm_mq_receive(mq_handle, &nbytes, &data, false);
	if (res != SHM_MQ_SUCCESS)
		ereport(ERROR,
				(errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
				 errmsg("lost connection to parallel worker")));
	elog(LOG, "\n[worker#%d(pid:%d)]\n - attaching #workers: %d\n - read message      : \"%s\"(size:%zu)",
				MyNumber, MyProcPid, dso->attached_workers, (char *) data, nbytes);

	/*
	 * step 5. Finally, detach the dynamic shared memory.
	 */
	shm_mq_detach(mq_handle);
	mq_handle = NULL;
	dsm_detach(seg);
	seg = NULL;
}
