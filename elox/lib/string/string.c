#include <elox/state.h>

Value stringStartsWith(Args *args) {
	ObjString *inst = AS_STRING(getValueArg(args, 0));
	ObjString *prefix;
	ELOX_GET_STRING_ARG_ELSE_RET(&prefix, args, 1);

	const char *strChars = inst->string.chars;
	const char *prefixChars = prefix->string.chars;

	if ((!strChars) || (!prefixChars))
		return BOOL_VAL(false);

	int strLen = inst->string.length;
	int prefixLen = prefix->string.length;
	if (prefixLen > strLen)
		return BOOL_VAL(false);
	return BOOL_VAL(memcmp(strChars, prefixChars, prefixLen) == 0);
}

Value stringEndsWith(Args *args) {
	ObjString *inst = AS_STRING(getValueArg(args, 0));
	ObjString *suffix;
	ELOX_GET_STRING_ARG_ELSE_RET(&suffix, args, 1);

	const char *strChars = inst->string.chars;
	const char *suffixChars = suffix->string.chars;

	if ((!strChars) || (!suffixChars))
		return BOOL_VAL(false);
	int strLen = inst->string.length;
	int suffixLen = suffix->string.length;
	if (suffixLen > strLen)
		return BOOL_VAL(false);
	return BOOL_VAL(memcmp(strChars + (strLen -suffixLen), suffixChars, suffixLen) == 0);
}