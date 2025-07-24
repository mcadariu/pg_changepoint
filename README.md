# pg_change_point_detection

(*Under development*) 

A PostgreSQL extension implementing the ED-PELT (Efficient Detection - Pruned Exact Linear Time) algorithm for changepoint detection in time series data. It's a conversion to C of [Andrey Akinshin's C# implementation](https://aakinshin.net/posts/edpelt/).

Reference paper:

Haynes, Kaylea, Paul Fearnhead, and Idris A. Eckley. "A computationally efficient nonparametric approach for changepoint detection." Statistics and Computing 27, no. 5 (2017): 1293-1305.

# Usage

```sql
test=# SELECT pg_change_point_detection(ARRAY[0,0,0,0,0,0,1,1,1,1,1,5,5,5,5,5,5,5,5]::float8[]) AS changepoints;
 changepoints 
--------------
 {5,10}
```
