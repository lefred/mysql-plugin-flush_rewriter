# mysql-plugin-flush_rewriter

A MySQL Plugin to rewrite all `FLUSH` statements as `FLUSH LOCAL` to avoid those
commands to be sent to replicas.

Sending `FLUSH` commands to replicas can be an issue, but for old DBAs like me,
the `LOCAL` keyword is not something we often use.

See https://lefred.be/content/as-a-dba-develop-a-critical-mind-when-receiving-advice/ 

```
 MySQL > install plugin flush_rewriter SONAME 'flush_rewriter.so';
 Query OK, 0 rows affected (0.0099 sec)
 
 MySQL > SELECT PLUGIN_NAME, PLUGIN_STATUS, PLUGIN_TYPE 
         FROM INFORMATION_SCHEMA.PLUGINS WHERE PLUGIN_NAME LIKE 'flush%';
+----------------+---------------+-------------+
| PLUGIN_NAME    | PLUGIN_STATUS | PLUGIN_TYPE |
+----------------+---------------+-------------+
| flush_rewriter | ACTIVE        | AUDIT       |
+----------------+---------------+-------------+
1 row in set (0.0016 sec)

 MySQL > show global variables like 'flush_rewriter%';
+----------------------------+-------+
| Variable_name              | Value |
+----------------------------+-------+
| flush_rewriter_force_local | OFF   |
+----------------------------+-------+
1 row in set (0.0041 sec)

 MySQL > flush status;
Query OK, 0 rows affected (0.0081 sec)

 MySQL > set global flush_rewriter_force_local=1;
Query OK, 0 rows affected (0.0004 sec)

 MySQL > show global variables like 'flush_rewriter%';
+----------------------------+-------+
| Variable_name              | Value |
+----------------------------+-------+
| flush_rewriter_force_local | ON    |
+----------------------------+-------+
1 row in set (0.0041 sec)

 MySQL > flush status;
Query OK, 0 rows affected, 1 warning (0.0007 sec)
Note (code 1105): Query 'flush status' rewritten to 'FLUSH LOCAL status' by a query rewrite plugin

 MySQL > flush NO_WRITE_TO_BINLOG status;
Query OK, 0 rows affected (0.0005 sec)

 MySQL > flush /*fred*/ status;
Query OK, 0 rows affected, 1 warning (0.0003 sec)
Note (code 1105): Query 'flush /*fred*/ status' rewritten to 'FLUSH LOCAL /*fred*/ status' by a query rewrite plugin
```

## Limitations

The plugin doesn't rewrite correctly the command when the statement starts with a comment:

```
 MySQL > /* fred */ flush status;
Query OK, 0 rows affected (0.0018 sec)
```

## Instrumentation

The plugin's memory consumption is intrumented in Performance_Schema:

```
 MySQL > SELECT * FROM performance_schema.memory_summary_global_by_event_name 
         where event_name like '%flush_rewriter'\G
*************************** 1. row ***************************
                  EVENT_NAME: memory/rewriter/flush_rewriter
                 COUNT_ALLOC: 2
                  COUNT_FREE: 2
   SUM_NUMBER_OF_BYTES_ALLOC: 111
    SUM_NUMBER_OF_BYTES_FREE: 111
              LOW_COUNT_USED: 0
          CURRENT_COUNT_USED: 0
             HIGH_COUNT_USED: 1
    LOW_NUMBER_OF_BYTES_USED: 0
CURRENT_NUMBER_OF_BYTES_USED: 0
   HIGH_NUMBER_OF_BYTES_USED: 60
1 row in set (0.0064 sec)
```
