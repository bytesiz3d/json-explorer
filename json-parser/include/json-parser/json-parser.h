#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "json-parser/Exports.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct J_Version
{
	uint8_t major, minor, patch;
} Version;

typedef struct J_JSON J_JSON;

typedef enum J_JSON_KIND
{
	J_JSON_NULL,
	J_JSON_BOOL,
	J_JSON_NUMBER,
	J_JSON_STRING,
	J_JSON_ARRAY,
	J_JSON_OBJECT,
} J_JSON_KIND;

typedef bool J_Bool;
typedef double J_Number;
typedef const char* J_String;

typedef struct J_Array
{
	J_JSON* ptr;
	size_t count;
} J_Array;

typedef struct J_Pair J_Pair;
typedef struct J_Object
{
	J_Pair* pairs;
	size_t count;
} J_Object;

struct J_JSON
{
	J_JSON_KIND kind;

	union
	{
		bool as_bool;
		double as_number;
		J_String as_string;
		J_Array as_array;
		J_Object as_object;
	};
};

struct J_Pair
{
	J_String key;
	J_JSON value;
};

typedef struct J_Parse_Result
{
	J_JSON json;
	const char* err;
} J_Parse_Result;

JSON_PARSER_EXPORT J_Version
j_version();

JSON_PARSER_EXPORT J_Parse_Result
j_parse(const char* json_string);

JSON_PARSER_EXPORT const char*
j_dump(J_JSON json);

#define j_get(J_TYPE, json) j_get_##J_TYPE(json)

JSON_PARSER_EXPORT J_Bool
j_get_J_Bool(J_JSON json);

JSON_PARSER_EXPORT J_Number
j_get_J_Number(J_JSON json);

JSON_PARSER_EXPORT J_String
j_get_J_String(J_JSON json);

JSON_PARSER_EXPORT J_Array
j_get_J_Array(J_JSON json);

JSON_PARSER_EXPORT J_Object
j_get_J_Object(J_JSON json);

#ifdef __cplusplus
}
#endif
