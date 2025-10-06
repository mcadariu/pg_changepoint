# pg_changepoint

[Change points](https://en.wikipedia.org/wiki/Change_detection) are abrupt variations in time series data. `pg_changepoint` is a PostgreSQL extension for detecting change points. It is a port of [Andrey Akinshin's](https://aakinshin.net/posts/edpelt/) implementation of the ED-PELT algorithm in C#. 

For an intro to changepoint analysis, see this tutorial by [Dr Rebecca Killick](https://www.youtube.com/watch?v=WelmlZK5G2Y).

# Usage

## With arrays

```sql
test=# SELECT pg_change_point_detection(ARRAY[0,0,0,0,0,0,1,1,1,1,1,5,5,5,5,5,5,5,5]::float8[]) AS changepoints;
 changepoints 
--------------
 {5,10}
```

## Timeseries table

#### Step 1. Create temporary table

```sql
CREATE TEMP TABLE sensor_data (
    id serial PRIMARY KEY,
    timestamp timestamp,
    value double precision
);
```

#### Step 2. Populate table with data

```sql
INSERT INTO sensor_data (timestamp, value)
SELECT 
    now() - interval '1 hour' + (i * interval '1 minute'),
    CASE 
        WHEN i <= 20 THEN random() * 5 + 10     -- Normal operation: 10-15
        WHEN i <= 40 THEN random() * 8 + 25     -- Anomaly: 25-33  
        ELSE random() * 3 + 8                   -- Recovery: 8-11
    END
FROM generate_series(1, 60) AS i;
```

#### Step 3. Query

```sql
SELECT pg_change_point_detection_in_column(                                                                                                                                                                                                 'sensor_data',     -- table name
           'value',           -- column with values
           'timestamp'        -- order by column
       ) AS changepoints;
 changepoints 
--------------
 {19,39}
(1 row)
```
