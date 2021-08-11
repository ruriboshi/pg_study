/*-------------------------------------------------------------------------
 *
 * ipc.c
 *
 * For Illustrative Purposes
 * =========================
 *
 * The dynamic shared memory(DSM) divides a whole of DSM into several
 * chunks, and these chunks are managed by using Table-Of-Contents(TOC).
 * 
 * +-------------------------- DSM --------------------------+
 * |                                                         |
 * | +--------- TOC ---------+   +------- CHUNK(s) -------+  |
 * | |                       |   |                        |  |
 * | | chunk1     ..... key1 ------> +----- chunk1 -----+ |  |
 * | |                       |   |   |                  | |  |
 * | |                       |   |   +------------------+ |  |
 * | |                       |   |                        |  |
 * | | chunk2     ..... key2 ------> +----- chunk2 -----+ |  |
 * | |                       |   |   |                  | |  |
 * | |                       |   |   |                  | |  |
 * | |                       |   |   +------------------+ |  |
 * : :                       :   :                        :  :
 * :                                                         :
 * +---------------------------------------------------------+
 *
 * The TOC created in DSM is identified by the magic number which must
 * be unique.
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "utils/memutils.h"

#include "test_dsm.h"


/* Helper function */
static void CreateMessageQueues(DynamicSharedInfo *dsi);


/*
 * Create the dynamic shared memory(DSM).
 */
DynamicSharedInfo *
CreateDynamicSharedMemory(int nworkers)
{
	MemoryContext			oldcontext;
	shm_toc_estimator		e;
	Size					segsize = 0;
	DynamicSharedObject	   *dso;
	DynamicSharedInfo	   *dsi;

	/* We might be running in a very short-lived memory context. */
	oldcontext = MemoryContextSwitchTo(TopTransactionContext);

	/* Initialize the DynamicSharedInfo structure */
	dsi = palloc0(sizeof(DynamicSharedInfo));
	dsi->nworkers = nworkers;

	/* 
	 * step 1. Estimate a size of memory we need to store something.
	 *			- shm_toc_initialize_estimator()
	 *				Initialize shm_toc_estimator to estimate memory size.
	 *			- shm_toc_estimate_chunk()
	 *				Add the size of chunk we need.
	 *			- shm_toc_estimate_keys()
	 *				Add the number of chunks.
	 *			- shm_toc_estimate()
	 *				Finally, fix the size and the number of chunks.
	 */
	shm_toc_initialize_estimator(&e);

	/* Estimate size of space for DynamicSharedObject */
	shm_toc_estimate_chunk(&e, sizeof(DynamicSharedObject));
	shm_toc_estimate_keys(&e, 1);

	/* Estimate ssize of space for message queues */
	shm_toc_estimate_chunk(&e,
						   mul_size(MESSAGE_QUEUE_SIZE, dsi->nworkers));
	shm_toc_estimate_keys(&e, 1);

	/* Fix size of the dynamic shared memory */
	segsize = shm_toc_estimate(&e);

	/*
	 * step 2. Create DSM and TOC.
	 *			- dsm_create
	 *				Create the DSM
	 *			- shm_toc_create
	 *				Create the TOC in the DSM
	 */
	dsi->seg = dsm_create(segsize, DSM_CREATE_NULL_IF_MAXSEGMENTS);
	if (dsi->seg != NULL)
		dsi->toc = shm_toc_create(TEST_DSM_MAGIC,
								  dsm_segment_address(dsi->seg), segsize);

	/*
	 * step 3. Initialize the chunk and register the chunk entry in TOC.
	 *			- shm_toc_allocate
	 *				Allocate the space enough to store the chunk.
	 *			- shm_toc_insert
	 *				Register the chunk entry and the unique key in TOC.
	 */
	dso = (DynamicSharedObject *) shm_toc_allocate(dsi->toc,
												   sizeof(DynamicSharedObject));
	dso->status = PS_INITIAL;
	SpinLockInit(&dso->mutex);
	dso->attached_workers = 0;
	shm_toc_insert(dsi->toc, DSM_KEY_DSO, dso);

	/* Allocate space for worker information. */
	dsi->worker = palloc0(sizeof(BackgroundWorkerInfo) * dsi->nworkers);

	/* Create message queues in DSM. */
	CreateMessageQueues(dsi);

	/* Restore previous memory context. */
	MemoryContextSwitchTo(oldcontext);

	return dsi;
}

/*
 * Create message queues in the dynamic shared memory.
 */
static void
CreateMessageQueues(DynamicSharedInfo *dsi)
{
	char	*mq_space;
	int		 i;

	/*
	 * step 1. Allocate space for shared memory queue.
	 */
	mq_space = shm_toc_allocate(dsi->toc,
								mul_size(MESSAGE_QUEUE_SIZE,
										 dsi->nworkers));

	/*
	 * step 2. Create the queues and register the role(sender or receiver).
	 */
	for (i = 0; i < dsi->nworkers; ++i)
	{
		shm_mq	*mq;

		/*
		 * Divide a memory allocated by shm_toc_allocate() into each queues.
		 *
		 * start address#1      start address#2      start address#3
		 * v                    v                    v
		 * +--------------------+--------------------+--------------------+..
		 * | queue for worker#1 | queue for worker#2 | queue for worker#3 |..
		 * +--------------------+--------------------+--------------------+..
		 * <-MESSAGE_QUEUE_SIZE->
		 *                      <-MESSAGE_QUEUE_SIZE->
		 *                                           <-MESSAGE_QUEUE_SIZE->
		 */
		mq = shm_mq_create(mq_space +
						   ((Size) i) * MESSAGE_QUEUE_SIZE,	/* start address */
						   (Size) MESSAGE_QUEUE_SIZE);		/* a queue size */
		shm_mq_set_sender(mq, MyProc);
		dsi->worker[i].mq_handle = shm_mq_attach(mq, dsi->seg, NULL);
	}

	/*
	 * step 3. Finally, Register the queues in TOC.
	 */
	shm_toc_insert(dsi->toc, DSM_KEY_DMQ, mq_space);
}
