/* contrib/test_dsm/test_dsm--1.0.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION test_dsm" to load this file. \quit

-- Register functions.
CREATE FUNCTION test_dsm(IN message text, IN nworkers integer)
RETURNS void
AS 'MODULE_PATHNAME'
LANGUAGE C PARALLEL SAFE;

