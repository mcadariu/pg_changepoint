\echo Use "CREATE EXTENSION pg_change_point_detection" to load this file. \quit

-- Function to detect changepoints with default minimum distance of 1
CREATE OR REPLACE FUNCTION pg_change_point_detection(
    data double precision[]
)
RETURNS integer[]
AS 'MODULE_PATHNAME', 'pg_change_point_detection'
LANGUAGE C STRICT IMMUTABLE PARALLEL SAFE;


-- Convenience function to work with table columns
CREATE OR REPLACE FUNCTION pg_change_point_detection_in_column(
    table_name text,
    column_name text,
    order_column text DEFAULT NULL
)
RETURNS integer[]
AS $$
DECLARE
    query_text text;
    data_array double precision[];
BEGIN
    -- Build dynamic query
    IF order_column IS NULL THEN
        query_text := format('SELECT array_agg(%I) FROM %I', column_name, table_name);
    ELSE
        query_text := format('SELECT array_agg(%I ORDER BY %I) FROM %I', 
                           column_name, order_column, table_name);
    END IF;
    
    -- Execute query and get data array
    EXECUTE query_text INTO data_array;
    
    -- Return changepoints
    RETURN pg_change_point_detection(data_array);
END;
$$ LANGUAGE plpgsql;
