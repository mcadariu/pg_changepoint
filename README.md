# pg_changepoint

(*Under development*) 

A PostgreSQL extension implementing the ED-PELT (Efficient Detection - Pruned Exact Linear Time) algorithm for changepoint detection in time series data. It's a conversion to C of [Andrey Akinshin's C# implementation](https://aakinshin.net/posts/edpelt/).

For an intro to changepoint analysis, see this tutorial by [Dr Rebecca Killick](https://www.youtube.com/watch?v=WelmlZK5G2Y).

# Usage

```sql
test=# SELECT pg_change_point_detection(ARRAY[0,0,0,0,0,0,1,1,1,1,1,5,5,5,5,5,5,5,5]::float8[]) AS changepoints;
 changepoints 
--------------
 {5,10}
```
