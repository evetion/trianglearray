/* triangle_array--0.1.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION triangle_array" to load this file. \quit


CREATE OR REPLACE FUNCTION trianglexyz(IN integer, IN bytea,
    OUT x double precision, OUT y double precision, OUT z double precision)
  RETURNS SETOF record 
  AS  'MODULE_PATHNAME', 'trianglexyz'
  LANGUAGE c IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION trianglez_bytea(IN integer, IN bytea, IN bytea, IN integer,
    OUT geom bytea)
  RETURNS bytea
  AS  'MODULE_PATHNAME', 'trianglez_bytea'
  LANGUAGE c IMMUTABLE STRICT;
  
CREATE OR REPLACE FUNCTION tinz_bytea(IN integer, IN bytea, IN bytea, IN integer,
    OUT geom bytea)
  RETURNS bytea
  AS  'MODULE_PATHNAME', 'tinz_bytea'
  LANGUAGE c IMMUTABLE STRICT;