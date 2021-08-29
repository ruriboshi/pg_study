/* test_extension/test_extension--1.0.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION test_extension" to load this file. \quit

CREATE FUNCTION get_column_names(relname text)
RETURNS text[]
AS 'MODULE_PATHNAME', 'get_column_names'
LANGUAGE C STRICT PARALLEL SAFE;
