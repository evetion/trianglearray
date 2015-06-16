MODULES = triangle_array
EXTENSION = triangle_array
DATA = triangle_array--0.1.sql
PGFILEDESC = "Triangle Array prototype to store TINs"
PGXS := $(shell pg_config --pgxs)
include $(PGXS)

