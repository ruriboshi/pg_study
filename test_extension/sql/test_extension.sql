CREATE EXTENSION test_extension;

--
-- get_column_names
--   Return the column name as an array of text of specified table.
--
CREATE TABLE t1 (c1 integer, c2 char, c3 text);
SELECT get_column_names('t1');
