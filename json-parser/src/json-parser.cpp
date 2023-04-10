#include "json-parser/json-parser.h"
#include <assert.h>

J_Version
j_version()
{
	return {0, 1, 0};
}

J_Parse_Result
j_parse(const char* json_string)
{
	return {J_JSON{}, "NotYetImplemented"};
}

J_Bool
j_get_J_Bool(J_JSON json)
{
	assert(json.kind == J_JSON_BOOL);
	return {};
}

J_Number
j_get_J_Number(J_JSON json)
{
	assert(json.kind == J_JSON_NUMBER);
	return {};
}

J_String
j_get_J_String(J_JSON json)
{
	assert(json.kind == J_JSON_STRING);
	return {};
}

J_Array
j_get_J_Array(J_JSON json)
{
	assert(json.kind == J_JSON_ARRAY);
	return {};
}

J_Object
j_get_J_Object(J_JSON json)
{
	assert(json.kind == J_JSON_OBJECT);
	return {};
}
