PGFILEDESC = "Extension to detect change points in timeseries data"
EXTENSION = pg_change_point_detection
DATA = pg_change_point_detection--1.0.sql
# Can be overridden by setting the PG_CONFIG environment variable.
PG_CONFIG ?= pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
MODULES = pg_change_point_detection
include $(PGXS)