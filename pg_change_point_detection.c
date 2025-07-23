#include "postgres.h"
#include "fmgr.h"
#include "utils/array.h"
#include "catalog/pg_type.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "access/htup_details.h"
#include "funcapi.h"
#include "executor/spi.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

PG_MODULE_MAGIC;

/* Structure to hold partial sums for empirical CDF */
typedef struct
{
    int **data;
    int k;
    int n;
} PartialSums;

/* Structure to hold changepoint detection results */
typedef struct
{
    int *changepoints;
    int count;
} ChangePointResult;

/* Function prototypes */
static PartialSums *get_partial_sums(double *data, int n, int k);
static void free_partial_sums(PartialSums *ps);
static double get_segment_cost(PartialSums *ps, int tau1, int tau2, int n);
static int which_min(double *values, int count);
static int double_compare(const void *a, const void *b);
static ChangePointResult *detect_changepoints(double *data, int n);
static void free_changepoint_result(ChangePointResult *result);

PG_FUNCTION_INFO_V1(pg_change_point_detection);

/* Helper function to compare doubles for qsort */
static int
double_compare(const void *a, const void *b)
{
    double da = *(const double *) a;
    double db = *(const double *) b;

    return (da > db) - (da < db);
}

/* Find index of minimum value in array */
static int
which_min(double *values, int count)
{
    double min_value;
    int    min_index;

    if (count == 0)
        elog(ERROR, "Array should contain elements");

    min_value = values[0];
    min_index = 0;

    for (int i = 1; i < count; i++)
    {
        if (values[i] < min_value)
        {
            min_value = values[i];
            min_index = i;
        }
    }

    return min_index;
}

/* Calculate partial sums for empirical CDF */
static PartialSums *
get_partial_sums(double *data, int n, int k)
{
    double      *sorted_data;
    PartialSums *ps;

    ps = (PartialSums *) palloc(sizeof(PartialSums));
    ps->k = k;
    ps->n = n;

    ps->data = (int **) palloc(k * sizeof(int *));
    for (int i = 0; i < k; i++)
        ps->data[i] = (int *) palloc0((n + 1) * sizeof(int));

    sorted_data = (double *) palloc(n * sizeof(double));
    memcpy(sorted_data, data, n * sizeof(double));
    qsort(sorted_data, n, sizeof(double), double_compare);

    for (int i = 0; i < k; i++)
    {
        double z = -1.0 + (2.0 * i + 1.0) / k;
        double p = 1.0 / (1.0 + pow(2.0 * n - 1.0, -z));
        double t = sorted_data[(int) floor((n - 1) * p)];

        for (int tau = 1; tau <= n; tau++)
        {
            ps->data[i][tau] = ps->data[i][tau - 1];

            if (data[tau - 1] < t)
                ps->data[i][tau] += 2;

            if (fabs(data[tau - 1] - t) < 1e-15)
                ps->data[i][tau] += 1;
        }
    }

    pfree(sorted_data);

    return ps;
}

/* Free partial sums structure */
static void
free_partial_sums(PartialSums *ps)
{
    if (ps)
    {
        for (int i = 0; i < ps->k; i++)
            pfree(ps->data[i]);

        pfree(ps->data);
        pfree(ps);
    }
}

/* Calculate cost of a segment */
static double
get_segment_cost(PartialSums *ps, int tau1, int tau2, int n)
{
    double sum = 0.0;
    double c;

    for (int i = 0; i < ps->k; i++)
    {
        int actual_sum = ps->data[i][tau2] - ps->data[i][tau1];

        if (actual_sum != 0 && actual_sum != (tau2 - tau1) * 2)
        {
            double fit = actual_sum * 0.5 / (tau2 - tau1);
            double lnp = (tau2 - tau1) * (fit * log(fit) + (1.0 - fit) * log(1.0 - fit));
            sum += lnp;
        }
    }

    c = -log(2.0 * n - 1.0);

    return 2.0 * c / ps->k * sum;
}

/* Free changepoint result structure */
static void
free_changepoint_result(ChangePointResult *result)
{
    if (result)
    {
        if (result->changepoints)
            pfree(result->changepoints);

        pfree(result);
    }
}

/* Main ED-PELT algorithm implementation */
static ChangePointResult *
detect_changepoints(double *data, int n)
{
    int         min_distance = 1;
    int         best_prev_tau_index;
    double      penalty;
    double      current_best_cost;
    int         k;
    int         new_prev_taus_size;
    PartialSums *partial_sums;
    double      *best_cost;
    int         *prev_changepoint_index;
    int         *prev_taus;
    double      *cost_for_prev_tau;
    int         prev_taus_count;
    int         *temp_changepoints;
    int         changepoint_count;
    int         current_index;
    ChangePointResult *result;

    if (n <= 2)
    {
        result = (ChangePointResult *) palloc(sizeof(ChangePointResult));
        result->changepoints = NULL;
        result->count = 0;
        return result;
    }

    if (min_distance < 1 || min_distance > n)
        elog(ERROR, "min_distance should be in range from 1 to data length");

    penalty = 3.0 * log(n);
    k = (int) fmin(n, ceil(4.0 * log(n)));

    partial_sums = get_partial_sums(data, n, k);

    best_cost = (double *) palloc((n + 1) * sizeof(double));
    prev_changepoint_index = (int *) palloc0((n + 1) * sizeof(int));

    best_cost[0] = -penalty;

    for (int current_tau = min_distance; current_tau < 2 * min_distance; current_tau++)
        best_cost[current_tau] = get_segment_cost(partial_sums, 0, current_tau, n);

    prev_taus = (int *) palloc((n + 1) * sizeof(int));
    cost_for_prev_tau = (double *) palloc((n + 1) * sizeof(double));
    prev_taus_count = 2;
    prev_taus[0] = 0;
    prev_taus[1] = min_distance;

    for (int current_tau = 2 * min_distance; current_tau <= n; current_tau++)
    {
        for (int i = 0; i < prev_taus_count; i++)
        {
            cost_for_prev_tau[i] = best_cost[prev_taus[i]] +
                                   get_segment_cost(partial_sums, prev_taus[i], current_tau, n) +
                                   penalty;
        }

        best_prev_tau_index = which_min(cost_for_prev_tau, prev_taus_count);
        best_cost[current_tau] = cost_for_prev_tau[best_prev_tau_index];
        prev_changepoint_index[current_tau] = prev_taus[best_prev_tau_index];

        current_best_cost = best_cost[current_tau];
        new_prev_taus_size = 0;

        for (int i = 0; i < prev_taus_count; i++)
        {
            if (cost_for_prev_tau[i] < current_best_cost + penalty)
                prev_taus[new_prev_taus_size++] = prev_taus[i];
        }

        if (prev_taus_count < n)
            prev_taus[new_prev_taus_size++] = current_tau - (min_distance - 1);

        prev_taus_count = new_prev_taus_size;
    }

    temp_changepoints = (int *) palloc(n * sizeof(int));
    changepoint_count = 0;

    current_index = prev_changepoint_index[n];
    while (current_index != 0)
    {
        temp_changepoints[changepoint_count++] = current_index - 1;
        current_index = prev_changepoint_index[current_index];
    }

    result = (ChangePointResult *) palloc(sizeof(ChangePointResult));
    result->count = changepoint_count;

    if (changepoint_count > 0)
    {
        result->changepoints = (int *) palloc(changepoint_count * sizeof(int));
        for (int i = 0; i < changepoint_count; i++)
            result->changepoints[i] = temp_changepoints[changepoint_count - 1 - i];
    }
    else
    {
        result->changepoints = NULL;
    }

    /* Cleanup */
    free_partial_sums(partial_sums);
    pfree(best_cost);
    pfree(prev_changepoint_index);
    pfree(prev_taus);
    pfree(cost_for_prev_tau);
    pfree(temp_changepoints);

    return result;
}

Datum
pg_change_point_detection(PG_FUNCTION_ARGS)
{
    ArrayType       *input_array;
    Datum           *elements;
    bool            *nulls;
    int              n;
    double          *data;
    ChangePointResult *result;
    Datum           *result_elements;
    ArrayType       *result_array;

    if (PG_ARGISNULL(0))
        PG_RETURN_NULL();

    input_array = PG_GETARG_ARRAYTYPE_P(0);

    deconstruct_array(input_array, FLOAT8OID, 8, FLOAT8PASSBYVAL, 'd',
                      &elements, &nulls, &n);

    if (n == 0)
        PG_RETURN_NULL();

    data = (double *) palloc(n * sizeof(double));
    for (int i = 0; i < n; i++)
    {
        if (nulls[i])
            elog(ERROR, "NULL values not supported in input array");

        data[i] = DatumGetFloat8(elements[i]);
    }

    result = detect_changepoints(data, n);

    if (result->count == 0)
    {
        pfree(data);
        free_changepoint_result(result);
        pfree(elements);
        pfree(nulls);
        PG_RETURN_NULL();
    }

    result_elements = (Datum *) palloc(result->count * sizeof(Datum));
    for (int i = 0; i < result->count; i++)
        result_elements[i] = Int32GetDatum(result->changepoints[i]);

    result_array = construct_array(result_elements, result->count,
                                   INT4OID, 4, true, 'i');

    pfree(data);
    pfree(result_elements);
    free_changepoint_result(result);
    pfree(elements);
    pfree(nulls);

    PG_RETURN_ARRAYTYPE_P(result_array);
}