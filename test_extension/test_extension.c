/*-------------------------------------------------------------------------
 *
 * test_extension.c
 * 		Example code of User-Defined C functions.
 *
 * Portions Copyright (c) 2012-2014, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 * 		test_extension.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/relation.h"
#include "catalog/namespace.h"
#include "funcapi.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/rel.h"
#include "utils/varlena.h"

/* Declarations for dynamic loading */
PG_MODULE_MAGIC;


PG_FUNCTION_INFO_V1(get_column_names);


Datum
get_column_names(PG_FUNCTION_ARGS)
{
	text			*relname = PG_GETARG_TEXT_PP(0);
	Relation		 rel;
	RangeVar		*relrv;
	TupleDesc		 tupdesc;
	ArrayBuildState	*astate = NULL;
	int			 i;

	/* convert from table name to RangeVar struct */
	relrv = makeRangeVarFromNameList(textToQualifiedNameList(relname));

	/* 
	 * Open the table with AccessShareLock. relation_open() is able to be
	 * used if you know OID of the relation.
	 */
	rel = relation_openrv(relrv, AccessShareLock);

	/* get a TupleDesc of the table */
	tupdesc = rel->rd_att;

	/* close the table */
	relation_close(rel, AccessShareLock);

	/* create record for showing column names */
	for (i = 0; i < tupdesc->natts; i++)
		astate = accumArrayResult(astate,
								  CStringGetTextDatum(tupdesc->attrs[i].attname.data),
								  false, TEXTOID, CurrentMemoryContext);

	PG_RETURN_ARRAYTYPE_P(makeArrayResult(astate, CurrentMemoryContext));
}
