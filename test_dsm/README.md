# test_dsm

`test_dsm` is a sample extension for learning behavior of the dynamic shared memory(DSM).

`test_dsm` launches the dynamic background worker(s) and then broadcasts message to each worker.

## Installation steps
1. Set pg_config binary location in PATH environment variable.
2. Compile and install the extension modules to your PostgreSQL install location.
```sql
USE_PGXS=1 make
USE_PGXS=1 make install
```
3. Enable `test_dsm` extension.
```sql
psql -c "CREATE EXTENSION test_dsm"
```

## Usage:
```sql
SELECT test_dsm('<arbitrary message>', '<launched #workers>');
```

## Example:
```sql
SELECT test_dsm('Hello PostgreSQL', 3);
```

```console
NOTICE:  [14774] ------- Start parallel something! -------
2018-12-01 22:59:40.213 JST [14780] LOG:
        [worker#0(pid:14780)]
         - attaching #workers: 1
         - read message      : "Hello PostgreSQL"(size:16)
2018-12-01 22:59:40.214 JST [14781] LOG:
        [worker#1(pid:14781)]
         - attaching #workers: 2
         - read message      : "Hello PostgreSQL"(size:16)
2018-12-01 22:59:40.215 JST [14782] LOG:
        [worker#2(pid:14782)]
         - attaching #workers: 3
         - read message      : "Hello PostgreSQL"(size:16)
NOTICE:  [14774] --------------- Good bye! ---------------
 test_dsm
----------

(1 row)
```
