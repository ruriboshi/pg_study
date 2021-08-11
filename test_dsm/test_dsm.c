/*-------------------------------------------------------------------------
 *
 * test_dsm.c
 *
 * A backend process creates the dynamic shared memory and launches the
 * background workers and broadcasts a message to each worker.
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "fmgr.h"
#include "miscadmin.h"
#include "utils/memutils.h"
#include "utils/builtins.h"

#include "test_dsm.h"

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(test_dsm);


/*----- Helper functions -----*/
static void LaunchBackgroundWorkers(DynamicSharedInfo *dsi);
static void WaitForAllWorkersFinished(DynamicSharedInfo *dsi);


Datum
test_dsm(PG_FUNCTION_ARGS)
{
	DynamicSharedInfo	*dsi;
	DynamicSharedObject	*dso;
	text		*message_text = PG_GETARG_TEXT_PP(0);
	Size		 message_size = VARSIZE_ANY_EXHDR(message_text);
	char		*message = text_to_cstring(message_text);
	int		nworkers = PG_GETARG_INT32(1);
	int		i;

	/* Create the dynamic shared memory. */
	dsi = CreateDynamicSharedMemory(nworkers);

	/* Launch the background workers. */
	LaunchBackgroundWorkers(dsi);

	/*
	 * Write the specified message into the shared message queues
	 * as if broadcasting.
	 */
	for (i = 0; i < dsi->nworkers; ++i)
	{
		shm_mq_result	result;

		if (dsi->worker[i].mq_handle == NULL)
			continue;

		/* Write the message you specified into a queue. */
		result = shm_mq_send(dsi->worker[i].mq_handle,
							 message_size, (char *) message, false);
		if (result != SHM_MQ_SUCCESS)
			ereport(ERROR,
					(errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
					 errmsg("could not send message to shared-memory queue")));
	}

	/*
	 * We won't let anybody interfere with our background workers.
	 * If we are processing a parallel queries, these parallel transactions
	 * must NOT be able to do COMMIT or ABORT until all of the workers have
	 * exited.
	 */
	HOLD_INTERRUPTS();
	WaitForAllWorkersFinished(dsi);
	RESUME_INTERRUPTS();

	/* Our parallel processing are finished. */
	dso = shm_toc_lookup(dsi->toc, DSM_KEY_DSO, false);
	dso->status = PS_FINISHED;

	/* Detach the dynamic shared message queues. */
	for (i = 0; i < dsi->nworkers; ++i)
	{
		shm_mq_detach(dsi->worker[i].mq_handle);
		dsi->worker[i].mq_handle = NULL;
	}

	/* Detach the dynamic shared memory. */
	if (dsi->seg != NULL)
	{
		dsm_detach(dsi->seg);
		dsi->seg = NULL;
	}

	/* Free the worker array itsself. */
	if (dsi->worker != NULL)
	{
		pfree(dsi->worker);
		dsi->worker = NULL;
	}

	PG_RETURN_VOID();
}

/*
 * Launch background workers.
 *
 * Register the entry point for worker. For details, see BackgroundWorkerMain().
 */
static void
LaunchBackgroundWorkers(DynamicSharedInfo *dsi)
{
	MemoryContext		oldcontext;
	BackgroundWorker	worker;
	BgwHandleStatus		status;
	int					i;

	Assert(dsi != NULL);

	/* We might be running in a short-lived memory context. */
	oldcontext = MemoryContextSwitchTo(TopTransactionContext);

	/* Configure a worker. */
	MemSet(&worker, 0, sizeof(worker));
	snprintf(worker.bgw_name, BGW_MAXLEN, "background worker launched by PID %d",
			 MyProcPid);
	snprintf(worker.bgw_type, BGW_MAXLEN, "background worker");
	worker.bgw_flags = BGWORKER_SHMEM_ACCESS;
	worker.bgw_start_time = BgWorkerStart_ConsistentState;
	worker.bgw_restart_time = BGW_NEVER_RESTART;
	sprintf(worker.bgw_library_name, "test_dsm");
	sprintf(worker.bgw_function_name, "BackgroundWorkerMain");
	worker.bgw_main_arg = UInt32GetDatum(dsm_segment_handle(dsi->seg));

	/* must set notify PID to wait for startup */
	worker.bgw_notify_pid = MyProcPid;

	/* Start background workers */
	for (i = 0; i < dsi->nworkers; ++i)
	{
		memcpy(worker.bgw_extra, &i, sizeof(int));
		if (!RegisterDynamicBackgroundWorker(&worker, &dsi->worker[i].handle))
			ereport(ERROR,
					(errcode(ERRCODE_INSUFFICIENT_RESOURCES),
					 errmsg("could not register background process"),
					 errhint("You may need to increase max_worker_processes.")));

		status = WaitForBackgroundWorkerStartup(dsi->worker[i].handle,
												&dsi->worker[i].pid);
		if (status != BGWH_STARTED)
			ereport(ERROR,
					(errcode(ERRCODE_INSUFFICIENT_RESOURCES),
					 errmsg("could not start background process"),
					 errhint("More details may be available in the server log.")));
	}

	/* Now, all workers are working. We can set handles for message queues. */
	for (i = 0; i < dsi->nworkers; ++i)
		shm_mq_set_handle(dsi->worker[i].mq_handle, dsi->worker[i].handle);

	/* Restore previous memory context. */
	MemoryContextSwitchTo(oldcontext);
}

/*
 * Wait for all workers until they are finished.
 */
static void
WaitForAllWorkersFinished(DynamicSharedInfo *dsi)
{
	int		i = 0;

	/* Wait until all workers actually shutdown. */
	for (i = 0; i < dsi->nworkers; ++i)
	{
		BgwHandleStatus	status;

		if (dsi->worker == NULL || dsi->worker[i].handle == NULL)
			continue;

		status = WaitForBackgroundWorkerShutdown(dsi->worker[i].handle);
		if (status == BGWH_STOPPED)
			continue;
		else if (status == BGWH_POSTMASTER_DIED)
			ereport(FATAL,
					(errcode(ERRCODE_ADMIN_SHUTDOWN),
					 errmsg("postmaster exited during a parallel processing")));

		/* Release memory */
		pfree(dsi->worker[i].handle);
		dsi->worker[i].handle = NULL;
	}
}
