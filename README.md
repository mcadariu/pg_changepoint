# pg_changepoint

(*Under development*) 

A PostgreSQL extension implementing the ED-PELT (Efficient Detection - Pruned Exact Linear Time) algorithm for changepoint detection in time series data. It's a conversion to C of [Andrey Akinshin's C# implementation](https://aakinshin.net/posts/edpelt/).

For an intro to changepoint analysis, see this tutorial by [Dr Rebecca Killick](https://www.youtube.com/watch?v=WelmlZK5G2Y).

# Usage

## With ARRAY

```sql
test=# SELECT pg_change_point_detection(ARRAY[0,0,0,0,0,0,1,1,1,1,1,5,5,5,5,5,5,5,5]::float8[]) AS changepoints;
 changepoints 
--------------
 {5,10}
```

## Timeseries table

### Create temporary table

```sql
CREATE TEMP TABLE sensor_data (
    id serial PRIMARY KEY,
    timestamp timestamp,
    value double precision
);
```

### Populate table with data

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

### 

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
