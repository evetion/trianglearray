# triangle_array 0.1
Triangle Array data structure for storing TINs in PostgreSQL packaged as an extension.

The Triangle Array implementation is made to mimic the OracleSDO_TIN structure.
Only three functions are exposed to PostgreSQL, the tinz_bytea function and
the trianglez_bytea function, as well as a helper function used internally. 

## Data used
See the thesis_tools repository for the file triangle_array.py, which can create the data structure queried. At the moment the tablename is hardcoded once in the source code, which should be changed according to the name used in the source code here.

## Use
After compiling with the provided Makefile and loading the provided extension, it is used as follows:

```SQL
SELECT ST_AsTEXT(tinz_bytea(numt,points,triangles,id)::geometry) FROM
triangle_array WHERE id = 1
```

It is useful when combined with PostGIS:

```SQL
SELECT ST_AsTEXT(tinz_bytea(numt,points,triangles,id)::geometry) FROM
triangle_array WHERE bbox && ST_MakePOINT(1,2)
```

## WKB Output
Both the TINZ and TRIANGLEZ output can be changed forth and back to MULTIPOLYGONZ and POLYGONZ by changing the following two lines:

```SQL
/* in tinz_bytea */
uint32_t wkbtype = 1006; /* MULTIPOLYGONZ | WKB TINZ is code 1016 */
/* in trianglez_to_geometry_wkb */
uint32_t wkbtype = 1003; /* POLYGONZ | WKB TRIANGLEZ is code 1017 */
```

The code used to cast to WKB has been adapted from the PostgreSQL pointcloud extension at https://github.com/pgpointcloud/pointcloud