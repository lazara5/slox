// This Source Code Form is subject to the terms of the
// Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
// with this file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <time.h>
#include <math.h>
#include <inttypes.h>
#include <stdio.h>
#include <assert.h>

#include <elox/builtins.h>
#include <elox/builtins/string.h>
#include <elox/builtins/array.h>

static Value printNative(Args *args) {
	VMCtx *vmCtx = args->vmCtx;

	for (int i = 0; i < args->count; i++) {
		printValue(vmCtx, ELOX_IO_OUT, getValueArg(args, i));
		ELOX_WRITE(vmCtx, ELOX_IO_OUT, " ");
	}
	ELOX_WRITE(vmCtx, ELOX_IO_OUT, "\n");
	return NIL_VAL;
}

static Value assertNative(Args *args) {
	VMCtx *vmCtx = args->vmCtx;
	VM *vm = &vmCtx->vm;

	if (args->count > 0) {
		if (isFalsey(getValueArg(args, 0))) {
			if (args->count < 2)
				return runtimeError(vmCtx, "Assertion failed");
			else {
				Error error = ERROR_INITIALIZER(vmCtx);
				Value strVal = toString(getValueArg(args, 1), &error);
				if (ELOX_UNLIKELY(error.raised))
					return strVal;
				const char *str = AS_CSTRING(strVal);
				push(vm, strVal);
				Value errorVal = runtimeError(vmCtx, "Assertion failed: %s", str);
				Value exception = pop(vm);
				pop(vm);
				push(vm, exception);
				return errorVal;
			}
		}
	}
	return NIL_VAL;
}

//--- Object --------------------

static Value objectToString(Args *args) {
	VMCtx *vmCtx = args->vmCtx;

	HeapCString ret;
	initHeapStringWithSize(vmCtx, &ret, 16);
	ObjInstance *inst = AS_INSTANCE(getValueArg(args, 0));
	heapStringAddFmt(vmCtx, &ret, "%s@%u", inst->clazz->name->string.chars, inst->identityHash);
	return OBJ_VAL(takeString(vmCtx, ret.chars, ret.length, ret.capacity));
}

static Value objectHashCode(Args *args) {
	ObjInstance *inst = AS_INSTANCE(getValueArg(args, 0));
	return NUMBER_VAL(inst->identityHash);
}

//--- String --------------------

static Value stringToString(Args *args) {
	ObjString *inst = AS_STRING(getValueArg(args, 0));
	return OBJ_VAL(inst);
}

static Value stringHashCode(Args *args) {
	ObjString *inst = AS_STRING(getValueArg(args, 0));
	return NUMBER_VAL(inst->hash);
}

static Value stringLength(Args *args) {
	ObjString *inst = AS_STRING(getValueArg(args, 0));
	return NUMBER_VAL(inst->string.length);
}

//--- Number --------------------

static Value numberToString(Args *args) {
	VMCtx *vmCtx = args->vmCtx;

	double n = AS_NUMBER(getValueArg(args, 0));
	HeapCString ret;
	initHeapString(vmCtx, &ret);
	if (trunc(n) == n)
		heapStringAddFmt(vmCtx, &ret, "%" PRId64, (int64_t)n);
	else
		heapStringAddFmt(vmCtx, &ret, "%g", n);
	return OBJ_VAL(takeString(vmCtx, ret.chars, ret.length, ret.capacity));
}

//--- Bool ----------------------

static Value boolToString(Args *args) {
	VMCtx *vmCtx = args->vmCtx;
	VM *vm = &vmCtx->vm;

	bool b = AS_BOOL(getValueArg(args, 0));
	return b ? OBJ_VAL(vm->builtins.trueString) : OBJ_VAL(vm->builtins.falseString);
}

//--- Exception -----------------

static Value exceptionInit(Args *args) {
	VMCtx *vmCtx = args->vmCtx;

	ObjInstance *inst = AS_INSTANCE(getValueArg(args, 0));
	ObjString *msg = AS_STRING(getValueArg(args, 1));
	ObjString *msgName = copyString(vmCtx, ELOX_USTR_AND_LEN("message"));
	PHandle protectedName = protectObj((Obj *)msgName);
	setInstanceField(inst, msgName, OBJ_VAL(msg));
	unprotectObj(protectedName);
	return OBJ_VAL(inst);
}

//--- Error ---------------------

static Value errorInit(Args *args) {
	VMCtx *vmCtx = args->vmCtx;
	VM *vm = &vmCtx->vm;

	return NIL_VAL;
}

//--- Map -----------------------

static Value mapIteratorHasNext(Args *args) {
	VMCtx *vmCtx = args->vmCtx;
	VM *vm = &vmCtx->vm;
	struct MapIterator *mi = &vm->builtins.mapIterator;

	ObjInstance *inst = AS_INSTANCE(getValueArg(args, 0));
	ObjMap *map = AS_MAP(inst->fields.values[mi->_map]);
	int current = AS_NUMBER(inst->fields.values[mi->_current]);

	TableEntry *entry;
	int32_t nextIndex = valueTableGetNext(&map->items, current, &entry);

	return BOOL_VAL(nextIndex >= 0);
}

static Value mapIteratorNext(Args *args) {
	VMCtx *vmCtx = args->vmCtx;
	VM *vm = &vmCtx->vm;
	struct MapIterator *mi = &vm->builtins.mapIterator;

	ObjInstance *inst = AS_INSTANCE(getValueArg(args, 0));
	ObjMap *map = AS_MAP(inst->fields.values[mi->_map]);
	int current = AS_NUMBER(inst->fields.values[mi->_current]);
	uint32_t modCount = AS_NUMBER(inst->fields.values[mi->_modCount]);

	if (ELOX_UNLIKELY(modCount != map->items.modCount))
		return runtimeError(vmCtx, "Map modified during iteration");

	TableEntry *entry;
	int nextIndex = valueTableGetNext(&map->items, current, &entry);

	inst->fields.values[mi->_current] = NUMBER_VAL(nextIndex);

	ObjArray *ret = newArray(vmCtx, 2, OBJ_TUPLE);
	PHandle protectedRet = protectObj((Obj *)ret);
	appendToArray(vmCtx, ret, entry->key);
	appendToArray(vmCtx, ret, entry->value);
	unprotectObj(protectedRet);
	return OBJ_VAL(ret);
}

static Value mapSize(Args *args) {
	ObjMap *inst = AS_MAP(getValueArg(args, 0));
	return NUMBER_VAL(inst->items.liveCount);
}

static Value mapPut(Args *args) {
	VMCtx *vmCtx = args->vmCtx;

	ObjMap *inst = AS_MAP(getValueArg(args, 0));
	Value key = getValueArg(args, 1);
	Value val = getValueArg(args, 2);

	Error error = ERROR_INITIALIZER(vmCtx);
	valueTableSet(&inst->items, key, val, &error);
	if (ELOX_UNLIKELY(error.raised))
		return EXCEPTION_VAL;

	return NIL_VAL;
}

static Value mapRemove(Args *args) {
	VMCtx *vmCtx = args->vmCtx;

	ObjMap *inst = AS_MAP(getValueArg(args, 0));
	Value key = getValueArg(args, 1);

	Error error = ERROR_INITIALIZER(vmCtx);
	bool deleted = valueTableDelete(&inst->items, key, &error);
	if (ELOX_UNLIKELY(error.raised))
		return EXCEPTION_VAL;

	return BOOL_VAL(deleted);
}

static Value mapIterator(Args *args) {
	VMCtx *vmCtx = args->vmCtx;
	VM *vm = &vmCtx->vm;
	struct MapIterator *mi = &vm->builtins.mapIterator;

	ObjMap *inst = AS_MAP(getValueArg(args, 0));

	ObjInstance *iter = newInstance(vmCtx, mi->_class);
	iter->fields.values[mi->_map] = OBJ_VAL(inst);
	iter->fields.values[mi->_current] = NUMBER_VAL(0);
	iter->fields.values[mi->_modCount] = NUMBER_VAL(inst->items.modCount);
	return OBJ_VAL(iter);
}

static Value notImplementedMethod(Args *args) {
	VMCtx *vmCtx = args->vmCtx;

	return runtimeError(vmCtx, "Not implemented");
}

uint16_t builtinConstant(VMCtx *vmCtx, const String *name) {
	VM *vm = &vmCtx->vm;

	ObjString *nameString = copyString(vmCtx, name->chars, name->length);
	PHandle protectedName = protectObj((Obj *)nameString);

	Value indexValue;
	if (tableGet(&vm->builtinSymbols, nameString, &indexValue)) {
		// already present
		unprotectObj(protectedName);
		return (uint16_t)AS_NUMBER(indexValue);
	}

	assert(vm->heap == &vm->permHeap);

	uint16_t newIndex = (uint16_t)vm->builtinValues.count;
	valueArrayPush(vmCtx, &vm->builtinValues, UNDEFINED_VAL);
	tableSet(vmCtx, &vm->builtinSymbols, nameString, NUMBER_VAL((double)newIndex));
	unprotectObj(protectedName);

#ifdef ELOX_DEBUG_PRINT_CODE
	eloxPrintf(vmCtx, ELOX_IO_DEBUG, ">>>Builtin[%5u] (%.*s)\n", newIndex,
			   name->length, name->chars);
#endif

	return newIndex;
}


static ObjClass *registerStaticClass(VMCtx *vmCtx, const String *name, const String *moduleName,
									 ObjClass *super) {
	VM *vm = &vmCtx->vm;

	bool isBuiltin = stringEquals(moduleName, &eloxBuiltinModule);
	ObjString *className = copyString(vmCtx, name->chars, name->length);
	push(vm, OBJ_VAL(className));
	ObjClass *clazz = newClass(vmCtx, className);
	push(vm, OBJ_VAL(clazz));

	if (isBuiltin) {
		uint16_t builtinIdx = builtinConstant(vmCtx, name);
		vm->builtinValues.values[builtinIdx] = peek(vm, 0);
	} else {
		uint16_t globalIdx = globalIdentifierConstant(vmCtx, name, moduleName);
		vm->globalValues.values[globalIdx] = peek(vm, 0);
	}

	popn(vm, 2);
	if (super != NULL) {
		clazz->super = OBJ_VAL(super);
		clazz->classId = clazz->baseId * super->classId;
		for (int i = 0; i < super->fields.capacity; i++) {
			Entry *entry = &super->fields.entries[i];
			if (entry->key != NULL)
				tableSet(vmCtx, &clazz->fields, entry->key, entry->value);
		}
		tableAddAll(vmCtx, &super->methods, &clazz->methods);
		clazz->initializer = super->initializer;
	} else
		clazz->classId = clazz->baseId;

	return clazz;
}

void registerBuiltins(VMCtx *vmCtx) {
	VM *vm = &vmCtx->vm;

	vm->builtins.anonInitString = copyString(vmCtx, ELOX_USTR_AND_LEN("$init"));

	vm->builtins.iteratorString = copyString(vmCtx, ELOX_USTR_AND_LEN("iterator"));
	vm->builtins.hasNextString = copyString(vmCtx, ELOX_USTR_AND_LEN("hasNext"));
	vm->builtins.nextString = copyString(vmCtx, ELOX_USTR_AND_LEN("next"));

	vm->builtins.hashCodeString = copyString(vmCtx, ELOX_USTR_AND_LEN("hashCode"));
	vm->builtins.equalsString = copyString(vmCtx, ELOX_USTR_AND_LEN("equals"));
	vm->builtins.toStringString = copyString(vmCtx, ELOX_USTR_AND_LEN("toString"));

	const String objectName = STRING_INITIALIZER("Object");
	ObjClass *objectClass = registerStaticClass(vmCtx, &objectName, &eloxBuiltinModule, NULL);
	addNativeMethod(vmCtx, objectClass, "toString", objectToString, 1, false);
	addNativeMethod(vmCtx, objectClass, "hashCode", objectHashCode, 1, false);

	const String iteratorName = STRING_INITIALIZER("Iterator");
	ObjClass *iteratorClass = registerStaticClass(vmCtx, &iteratorName, &eloxBuiltinModule, objectClass);
	addNativeMethod(vmCtx, iteratorClass, "hasNext", notImplementedMethod, 1, false);
	addNativeMethod(vmCtx, iteratorClass, "next", notImplementedMethod, 1, false);
	addNativeMethod(vmCtx, iteratorClass, "remove", notImplementedMethod, 1, false);
	vm->builtins.iteratorClass = iteratorClass;

	const String gmatchIteratorName = STRING_INITIALIZER("$GmatchIterator");
	ObjClass *gmatchIteratorClass = registerStaticClass(vmCtx, &gmatchIteratorName, &eloxBuiltinModule, iteratorClass);
	vm->builtins.gmatchIterator = (struct GmatchIterator){
		._string = addClassField(vmCtx, gmatchIteratorClass, "string"),
		._pattern = addClassField(vmCtx, gmatchIteratorClass, "pattern"),
		._offset = addClassField(vmCtx, gmatchIteratorClass, "offset"),
		._cachedNext = addClassField(vmCtx, gmatchIteratorClass, "cachedNext"),
		._class = gmatchIteratorClass
	};
	addNativeMethod(vmCtx, gmatchIteratorClass, "hasNext", gmatchIteratorHasNext, 1, false);
	addNativeMethod(vmCtx, gmatchIteratorClass, "next", gmatchIteratorNext, 1, false);

	const String stringName = STRING_INITIALIZER("String");
	ObjClass *stringClass = registerStaticClass(vmCtx, &stringName, &eloxBuiltinModule, objectClass);
	addNativeMethod(vmCtx, stringClass, "toString", stringToString, 1, false);
	addNativeMethod(vmCtx, stringClass, "hashCode", stringHashCode, 1, false);
	addNativeMethod(vmCtx, stringClass, "length", stringLength, 1, false);
	addNativeMethod(vmCtx, stringClass, "fmt", stringFmt, 1, true);
	addNativeMethod(vmCtx, stringClass, "find", stringFind, 3, false);
	addNativeMethod(vmCtx, stringClass, "findMatch", stringFindMatch, 3, false);
	addNativeMethod(vmCtx, stringClass, "match", stringMatch, 3, false);
	addNativeMethod(vmCtx, stringClass, "gmatch", stringGmatch, 2, false);
	vm->builtins.stringGsub = addNativeMethod(vmCtx, stringClass, "gsub", stringGsub, 4, false);
	addNativeMethod(vmCtx, stringClass, "startsWith", stringStartsWith, 2, false);
	addNativeMethod(vmCtx, stringClass, "endsWith", stringEndsWith, 2, false);
	addNativeMethod(vmCtx, stringClass, "upper", stringUpper, 1, false);
	addNativeMethod(vmCtx, stringClass, "lower", stringLower, 1, false);
	addNativeMethod(vmCtx, stringClass, "trim", stringTrim, 1, false);

	vm->builtins.stringClass = stringClass;

	const String numberName = STRING_INITIALIZER("Number");
	ObjClass *numberClass = registerStaticClass(vmCtx, &numberName, &eloxBuiltinModule, objectClass);
	addNativeMethod(vmCtx, numberClass, "toString", numberToString, 1, false);
	vm->builtins.numberClass = numberClass;

	vm->builtins.trueString = copyString(vmCtx, ELOX_USTR_AND_LEN("true"));
	vm->builtins.falseString = copyString(vmCtx, ELOX_USTR_AND_LEN("false"));

	const String boolName = STRING_INITIALIZER("Bool");
	ObjClass *boolClass = registerStaticClass(vmCtx, &boolName, &eloxBuiltinModule, objectClass);
	addNativeMethod(vmCtx, boolClass, "toString", boolToString, 1, false);
	vm->builtins.boolClass = boolClass;

	const String instanceName = STRING_INITIALIZER("$Instance");
	ObjClass *instanceClass = registerStaticClass(vmCtx, &instanceName, &eloxBuiltinModule, objectClass);
	vm->builtins.instanceClass = instanceClass;

	const String className = STRING_INITIALIZER("Class");
	ObjClass *classClass = registerStaticClass(vmCtx, &className, &eloxBuiltinModule, objectClass);
	vm->builtins.classClass = classClass;

	const String throwableName = STRING_INITIALIZER("Throwable");
	ObjClass *throwableClass = registerStaticClass(vmCtx, &throwableName, &eloxBuiltinModule, objectClass);
	addClassField(vmCtx, throwableClass, "message");
	vm->builtins.throwableClass = throwableClass;

	const String exceptionName = STRING_INITIALIZER("Exception");
	ObjClass *exceptionClass = registerStaticClass(vmCtx, &exceptionName, &eloxBuiltinModule, throwableClass);
	addClassField(vmCtx, exceptionClass, "stacktrace");
	addNativeMethod(vmCtx, exceptionClass, "Exception", exceptionInit, 2, false);
	vm->builtins.exceptionClass = exceptionClass;

	const String runtimeExceptionName = STRING_INITIALIZER("RuntimeException");
	ObjClass *runtimeExceptionClass = registerStaticClass(vmCtx, &runtimeExceptionName, &eloxBuiltinModule, exceptionClass);
	vm->builtins.runtimeExceptionClass = runtimeExceptionClass;

	const String errorName = STRING_INITIALIZER("Error");
	ObjClass *errorClass = registerStaticClass(vmCtx, &errorName, &eloxBuiltinModule, throwableClass);
	addNativeMethod(vmCtx, errorClass, "Error", errorInit, 2, false);
	vm->builtins.errorClass = errorClass;

	const String arrayIteratorName = STRING_INITIALIZER("$ArrayIterator");
	ObjClass *arrayIteratorClass = registerStaticClass(vmCtx, &arrayIteratorName, &eloxBuiltinModule, iteratorClass);
	vm->builtins.arrayIterator = (struct ArrayIterator){
		._array = addClassField(vmCtx, arrayIteratorClass, "array"),
		._cursor = addClassField(vmCtx, arrayIteratorClass, "cursor"),
		._lastRet = addClassField(vmCtx, arrayIteratorClass, "lastRet"),
		._modCount = addClassField(vmCtx, arrayIteratorClass, "modCount"),
		._class = arrayIteratorClass
	};
	addNativeMethod(vmCtx, arrayIteratorClass, "hasNext", arrayIteratorHasNext, 1, false);
	addNativeMethod(vmCtx, arrayIteratorClass, "next", arrayIteratorNext, 1, false);
	addNativeMethod(vmCtx, arrayIteratorClass, "remove", arrayIteratorRemove, 1, false);

	const String arrayName = STRING_INITIALIZER("Array");
	ObjClass *arrayClass = registerStaticClass(vmCtx, &arrayName, &eloxBuiltinModule, objectClass);
	addNativeMethod(vmCtx, arrayClass, "length", arrayLength, 1, false);
	addNativeMethod(vmCtx, arrayClass, "add", arrayAdd, 2, false);
	addNativeMethod(vmCtx, arrayClass, "removeAt", arrayRemoveAt, 2, false);
	addNativeMethod(vmCtx, arrayClass, "iterator", arrayIterator, 1, false);
	vm->builtins.arrayClass = arrayClass;

	const String tupleName = STRING_INITIALIZER("Tuple");
	ObjClass *tupleClass = registerStaticClass(vmCtx, &tupleName, &eloxBuiltinModule, objectClass);
	addNativeMethod(vmCtx, tupleClass, "length", arrayLength, 1, false);
	addNativeMethod(vmCtx, tupleClass, "iterator", arrayIterator, 1, false);
	vm->builtins.tupleClass = tupleClass;

	const String mapIteratorName = STRING_INITIALIZER("$MapIterator");
	ObjClass *mapIteratorClass = registerStaticClass(vmCtx, &mapIteratorName, &eloxBuiltinModule, iteratorClass);
	vm->builtins.mapIterator = (struct MapIterator){
		._map = addClassField(vmCtx, mapIteratorClass, "map"),
		._current = addClassField(vmCtx, mapIteratorClass, "current"),
		._modCount = addClassField(vmCtx, mapIteratorClass, "modCount"),
		._class = mapIteratorClass
	};
	addNativeMethod(vmCtx, mapIteratorClass, "hasNext", mapIteratorHasNext, 1, false);
	addNativeMethod(vmCtx, mapIteratorClass, "next", mapIteratorNext, 1, false);

	const String mapName = STRING_INITIALIZER("Map");
	ObjClass *mapClass = registerStaticClass(vmCtx, &mapName, &eloxBuiltinModule, objectClass);
	addNativeMethod(vmCtx, mapClass, "size", mapSize, 1, false);
	addNativeMethod(vmCtx, mapClass, "put", mapPut, 3, false);
	addNativeMethod(vmCtx, mapClass, "remove", mapRemove, 2, false);
	addNativeMethod(vmCtx, mapClass, "iterator", mapIterator, 1, false);
	vm->builtins.mapClass = mapClass;

	const String printName = STRING_INITIALIZER("print");
	registerNativeFunction(vmCtx, &printName, &eloxBuiltinModule, printNative, 0, true);

	const String printfName = STRING_INITIALIZER("printf");
	registerNativeFunction(vmCtx, &printfName, &eloxBuiltinModule, printFmt, 1, true);

	const String assertName = STRING_INITIALIZER("assert");
	registerNativeFunction(vmCtx, &assertName, &eloxBuiltinModule, assertNative, 0, true);
}

void clearBuiltins(VM *vm) {
	vm->builtins.anonInitString = NULL;

	vm->builtins.iteratorString = NULL;
	vm->builtins.hasNextString = NULL;
	vm->builtins.nextString = NULL;

	vm->builtins.hashCodeString = NULL;
	vm->builtins.equalsString = NULL;
	vm->builtins.toStringString = NULL;

	vm->builtins.stringClass = NULL;
	vm->builtins.gmatchIterator._class = NULL;

	vm->builtins.numberClass = NULL;

	vm->builtins.boolClass = NULL;
	vm->builtins.trueString = NULL;
	vm->builtins.falseString = NULL;
	vm->builtins.instanceClass = NULL;
	vm->builtins.classClass = NULL;

	vm->builtins.oomError = NULL;
	vm->builtins.errorClass = NULL;
	vm->builtins.runtimeExceptionClass = NULL;
	vm->builtins.exceptionClass = NULL;
	vm->builtins.throwableClass = NULL;
	vm->builtins.arrayIterator._class = NULL;
	vm->builtins.arrayClass = NULL;
	vm->builtins.tupleClass = NULL;
	vm->builtins.mapIterator._class = NULL;
	vm->builtins.mapClass = NULL;
	vm->builtins.iteratorClass = NULL;
}
