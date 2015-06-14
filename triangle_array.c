#include "postgres.h"	
#include "fmgr.h"
#include "utils/array.h" // FOR ARRAYS
#include "utils/lsyscache.h"
#include "access/htup_details.h" // NO TUP WARNINGS
#include "funcapi.h"
#include <string.h>
#include <stdlib.h>
#include "executor/executor.h"  /* for GetAttributeByName() */
#include "executor/spi.h"
#include "libpq/pqformat.h"
#include "catalog/pg_type.h"

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif



typedef struct
{
    float4  x, y, z;
} Point;

typedef struct
{
    double x, y, z;
} Pointd;

typedef struct
{
	int32 a, b, c, n1, n2, n3;
} TTriangle;

typedef struct {
  uint16_t patch;
  uint16_t point;
} Link;


Datum trianglez_bytea(PG_FUNCTION_ARGS);
Datum tinz_bytea(PG_FUNCTION_ARGS);
Datum trianglexyz(PG_FUNCTION_ARGS);
void* trianglez_to_geometry_wkb(float8 *points, TTriangle *triangle, size_t *wkbsize, int patch);
float8* overocean(int pointid, int patchid);
uint32_t stitch(uint16_t patch_id, uint16_t point_id);
Link* unstitch(uint32_t packed);
char machine_endian(void);


uint32_t stitch(uint16_t patch_id, uint16_t point_id)
{
  uint32_t *packed = palloc0(sizeof(uint32_t));
  *packed = (uint32_t) ((point_id << 16) | patch_id);
  ////elog(INFO,"Stuff in %i %i, out %i, ",patch_id,point_id,packed);
  return *packed;
}

Link* unstitch(uint32_t packed)
{
  Link *link;
  link = palloc(sizeof(Link));

  ////elog(INFO,"Packed %i",packed);
  link->point = (packed >> 16);
  link->patch = (packed & 0xFFFF);
  ////elog(INFO,"Unpacked %i %i",link->patch,link->point);
  return link;
}

char machine_endian(void)
{
	static int check_int = 1; /* dont modify this!!! */
	return *((char *) &check_int); /* 0 = big endian | xdr,
	                               * 1 = little endian | ndr
                                   */
}

/* NEW FUNCTION TO EXPLODE RETURNS BYTEA

TINZ 1016
TRIANGLEZ 1017
 01 10000080 01000000 
 01 11000080 01000000 0400000000000000 0000f03f00000000 0000f03f00000000 0000f03f00000000 0000004000000000000000400000000000000040000000000000084000000000000008400000000000000840000000000000f03f000000000000f03f000000000000f03f"
 BINARY REPRESENTATION OF ONLY 1 TRIANGLE IN TINZ

<tinz binary representation> ::= 
    <byte order> <wkbtinz> <num> <trianglez binary representation>

SELECT encode(ST_GEOMFROMTEXT('TINZ(((1 1 1, 2 2 2, 3 3 3, 1 1 1)))'),'hex')

WKT:

TIN (((0 0, 0 1, 1 0, 0 0)), ((0 1, 1 1, 1 0, 0 1)))

WKB:

01|10000000|02000000|
01|11000000|01000000|04000000|<64 bytes of 4 vertices>
01|11000000|01000000|04000000|<64 bytes of 4 vertices>

*/
float8* overocean(int pointid, int patchid) {
	/* 	For the rare edge case that we need the xyz from another patch
		use sql. */

	char sql[100];
  	HeapTuple row;
  	TupleDesc rdesc;
  	float8 *p = palloc(3*sizeof(float8));
  	bool isnull = false;
  	

	//elog(INFO,"Bonny?.");

  	sprintf(sql, "SELECT (trianglexyz(%i,points)).* FROM multitin_l7_td WHERE id = %i",pointid,patchid);
  	//elog(INFO,"Asking point %i in patch %i",pointid, patchid);
  	SPI_execute(sql, true, 1);
	//elog(INFO,"Bonny?!.");
  	rdesc = SPI_tuptable->tupdesc;
	//elog(INFO,"Bonny?!.");
  	row = SPI_tuptable->vals[0];
	//elog(INFO,"Bonny?!.");
  	p[0] = DatumGetFloat8(SPI_getbinval(row, rdesc, 1, &isnull));
 	p[1] = DatumGetFloat8(SPI_getbinval(row, rdesc, 2, &isnull));
  	p[2] = DatumGetFloat8(SPI_getbinval(row, rdesc, 3, &isnull));
	//elog(INFO,"Bonny says hi.");

  	return p;
}

PG_FUNCTION_INFO_V1(trianglexyz);
Datum trianglexyz(PG_FUNCTION_ARGS)
/*  return x, y, z in tuple for point
	needs i, points bytea as input */
{	
	float8 *points;
	int32 i;
	float8 x;
	float8 y;
	float8 z;

    TupleDesc resultTupleDesc; 
    Oid resultTypeId; 
    Datum retvals[3]; 
    bool  retnulls[3]; 
    HeapTuple rettuple; 

	i = PG_GETARG_INT32(0);

	points = (float8*) ( ( (uint8_t*) PG_GETARG_BYTEA_P(1) ) + 4);
   	x = points[3*i];
  	y = points[3*i+1];
  	z = points[3*i+2];

	get_call_result_type(fcinfo, &resultTypeId, &resultTupleDesc); 
    Assert(resultTypeId == TYPEFUNC_COMPOSITE); 
  	BlessTupleDesc(resultTupleDesc);	

   	retvals[0] = Float8GetDatum(x);
   	retvals[1] = Float8GetDatum(y);
   	retvals[2] = Float8GetDatum(z);

   	retnulls[0] = false;
   	retnulls[1] = false;
   	retnulls[2] = false;

    rettuple = heap_form_tuple( resultTupleDesc, retvals, retnulls ); 
   	PG_RETURN_DATUM( HeapTupleGetDatum( rettuple ) );
}

PG_FUNCTION_INFO_V1(tinz_bytea);
Datum tinz_bytea(PG_FUNCTION_ARGS)
{	/* Return one triangle in wkb.
	expects triangle id, points and triangles
	*/
	uint32_t wkbtype = 1006; //MULTIPOLYGONZ 1016; /* WKB TINZ */
	uint32_t numt = PG_GETARG_INT32(0);
	float8 *points = (float8*) ( ( (uint8_t*) PG_GETARG_BYTEA_P(1) ) + 4);
	TTriangle *triangles = (TTriangle*) ( ( (uint8_t*) PG_GETARG_BYTEA_P(2) ) + 4);
	int patch = PG_GETARG_INT32(3);
	size_t size = 1 + 4 + 4 +(numt * 109); /* endian + type + numt + numt*size of TriangleZ */
	uint8 *bytes;
	size_t bytes_size;
	size_t bytes_size_check = 1 + 4 + 4; /* extra check */
	bytea *wkb;
	size_t wkb_size;
	uint8_t *wkbs, *ptr;
	int i;

	//elog(INFO,"TINZ Got started with input.");

	wkbs = palloc(size);
	ptr = wkbs;

	ptr[0] = machine_endian(); /* Endian flag */
	ptr += 1;

	memcpy(ptr, &wkbtype, 4); /* WKB type */
	ptr += 4;

	memcpy(ptr, &numt, 4); /* num of linear rings type */
	ptr += 4;

	for (i=0;i<numt;i++) {
		bytes_size = 0;
		bytes = trianglez_to_geometry_wkb(points, &triangles[i], &bytes_size, patch);
		//elog(INFO,"Received bytes with len %i",(int)bytes_size);
		memcpy(ptr, bytes, bytes_size);
		bytes_size_check += bytes_size;
		ptr += bytes_size;
	}
	//SPI_finish();


	if (size != bytes_size_check) {
		elog(WARNING,"Something fucked up.");
	}

	wkb_size = VARHDRSZ + size;
	wkb = palloc(wkb_size);
	memcpy(VARDATA(wkb), wkbs, size);
	SET_VARSIZE(wkb, wkb_size);
	//elog(INFO,"Got wkb.");

	pfree(wkbs);
	//elog(INFO,"Returning.");


	PG_RETURN_BYTEA_P(wkb);
}

PG_FUNCTION_INFO_V1(trianglez_bytea);
Datum trianglez_bytea(PG_FUNCTION_ARGS)
{	/* Return one triangle in wkb.
	expects triangle id, points and triangles
	*/
	
	int i = PG_GETARG_INT32(0);
	float8 *points = (float8*) ( ( (uint8_t*) PG_GETARG_BYTEA_P(1) ) + 4);
	TTriangle *triangles = (TTriangle*) ( ( (uint8_t*) PG_GETARG_BYTEA_P(2) ) + 4);
	int patch = PG_GETARG_INT32(3);
	uint8 *bytes;
	size_t bytes_size;
	bytea *wkb;
	size_t wkb_size;

	//elog(INFO,"Got started with input.");

	bytes = trianglez_to_geometry_wkb(points, &triangles[i], &bytes_size, patch);
	wkb_size = VARHDRSZ + bytes_size;
	wkb = palloc(wkb_size);
	memcpy(VARDATA(wkb), bytes, bytes_size);
	SET_VARSIZE(wkb, wkb_size);
	//elog(INFO,"Got wkb.");

	pfree(bytes);
	//elog(INFO,"Returning.");

	SPI_finish();
	PG_RETURN_BYTEA_P(wkb);
}

void* trianglez_to_geometry_wkb(float8 *points, TTriangle *triangle, size_t *wkbsize, int patch)
{
	/* With a given array of points and one triangle, we cast this triangle into a 
	well known binary form */

	uint32_t wkbtype = 1003; // POLYGONZ 1017; /* WKB TRIANGLEZ */
	uint32_t nump = 4; /* WKB TRIANGLEZ */
	uint32_t numr = 1; /* WKB TRIANGLEZ */

	size_t size = 1 + 4 + 4 + 4 + (4*8*3); /* endian + type + rings + num + 4xyz doubles */
	uint8_t *wkb, *ptr;
	Link *a, *b, *c;
	
  	SPI_connect();

	wkb = palloc(size);
	ptr = wkb;

	ptr[0] = machine_endian(); /* Endian flag */
	ptr += 1;

	memcpy(ptr, &wkbtype, 4); /* WKB type */
	ptr += 4;

	memcpy(ptr, &numr, 4); /* num of linear rings type */
	ptr += 4;

	memcpy(ptr, &nump, 4); /* num of points type */
	ptr += 4;

	/* triangle vertices are stitched as point, patch pair */
	a = unstitch(triangle->a);
	b = unstitch(triangle->b);
	c = unstitch(triangle->c);

	if (a->patch != patch) {
		float8 *p = overocean(a->point,a->patch);
		memcpy(ptr, p, 24); /* A */
		ptr += 24;
	} else {
		memcpy(ptr, &points[3*(a->point)], 24); /* A */
		ptr += 24;
	}

	if (b->patch != patch) {
		float8 *p = overocean(b->point,b->patch);
		memcpy(ptr, p, 24); /* A */
		ptr += 24;
	} else {
		memcpy(ptr, &points[3*(b->point)], 24); /* B */
		ptr += 24;
	}

	if (c->patch != patch) {
		float8 *p = overocean(c->point,c->patch);
		memcpy(ptr, p, 24); /* A */
		ptr += 24;
	} else {
		memcpy(ptr, &points[3*(c->point)], 24); /* C */
		ptr += 24;
	}

	if (a->patch != patch) {
		float8 *p = overocean(a->point,a->patch);
		memcpy(ptr, p, 24); /* A */
		ptr += 24;
	} else {
		memcpy(ptr, &points[3*(a->point)], 24); /* A */
		ptr += 24;
	}
	
	SPI_finish();
	
	if ( wkbsize ) *wkbsize = size;
	return wkb;
}


