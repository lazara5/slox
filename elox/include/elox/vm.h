#ifndef ELOX_VM_H
#define ELOX_VM_H

#include "elox.h"
#include "elox/memory.h"
#include "elox/chunk.h"
#include "elox/object.h"
#include "elox/table.h"
#include "elox/handleSet.h"
#include "elox/function.h"
#include "elox/rand.h"
#include "elox/primegen.h"

typedef struct CompilerState CompilerState;

typedef struct {
	Chunk *chunk;
	uint8_t *ip;
	CallFrame frames[FRAMES_MAX];
	int frameCount;

	Value *stack;
	Value *stackTop;
	Value *stackTopMax;
	int stackCapacity;
	int handlingException;

	Table strings;
	ObjUpvalue *openUpvalues;
	stc64_t prng;
	PrimeGen primeGen;
// globals
	CloseTable globalNames;
	ValueArray globalValues;
// modules
	Table modules;
	Table builtinSymbols;
// builtins
	ObjString *iteratorString;
	ObjString *hasNextString;
	ObjString *nextString;

	ObjString *hashCodeString;
	ObjString *equalsString;
	ObjString *toStringString;
	ObjClass *stringClass;
	ObjClass *numberClass;
	ObjClass *boolClass;
	ObjString *trueString;
	ObjString *falseString;
	ObjClass *exceptionClass;
	ObjClass *runtimeExceptionClass;
	ObjClass *arrayIteratorClass;
	uint16_t arrayIteratorArrayIndex;
	uint16_t arrayIteratorCurrentIndex;
	uint16_t arrayIteratorModCountIndex;
	ObjClass *arrayClass;
	ObjClass *mapIteratorClass;
	uint16_t mapIteratorMapIndex;
	uint16_t mapIteratorCurrentIndex;
	uint16_t mapIteratorModCountIndex;
	ObjClass *mapClass;
	ObjClass *iteratorClass;
// handles
	HandleSet handles;
// compilers
	int compilerCount;
	int compilerCapacity;
	CompilerState **compilerStack;
// for GC
	size_t bytesAllocated;
	size_t nextGC;
	Obj *objects;
	int grayCount;
	int grayCapacity;
	Obj **grayStack;
} VM;

typedef struct VMCtx VMCtx;

void initVM(VMCtx *vmCtx);
void destroyVMCtx(VMCtx *vmCtx);
void pushCompilerState(VMCtx *vmCtx, CompilerState *compilerState);
void popCompilerState(VMCtx *vmCtx);
EloxInterpretResult interpret(VMCtx *vmCtx, char *source, const String *moduleName);
void push(VM *vm, Value value);
Value pop(VM *vm);
void popn(VM *vm, uint8_t n);
void pushn(VM *vm, uint8_t n);
Value peek(VM *vm, int distance);

#ifdef ELOX_DEBUG_TRACE_EXECUTION
void printStack(VMCtx *vmCtx);
#define DBG_PRINT_STACK(label, vmCtx) \
	ELOX_WRITE(vmCtx, ELOX_IO_DEBUG, "[" label "]"); printStack(vmCtx);
#else
#define DBG_PRINT_STACK(label, vm)
#endif

void registerNativeFunction(VMCtx *vmCtx, const String *name, const String *moduleName,
							NativeFn function, uint16_t arity, bool hasVarargs);

// Error handling

Value runtimeError(VMCtx *vmCtx, const char *format, ...) ELOX_PRINTF(2, 3);

typedef struct Error {
	VMCtx *vmCtx;
	bool raised;
	Value errorVal;
} Error;

#define ___ELOX_RAISE(error, fmt, ...) \
	if (!(error)->raised) { \
		(error)->raised = true; \
		runtimeError((error)->vmCtx, fmt, ## __VA_ARGS__); \
		(error)->errorVal = peek(&((error)->vmCtx->vm), 0); \
	}

#define ELOX_RAISE(error, fmt, ...) \
	{ \
		___ELOX_RAISE(error, fmt, ## __VA_ARGS__) \
	}

#define ELOX_RAISE_RET(error, fmt, ...) \
	{ \
		___ELOX_RAISE(error, fmt, ## __VA_ARGS__) \
		return; \
	}

#define ELOX_RAISE_RET_VAL(val, error, fmt, ...) \
	{ \
		___ELOX_RAISE(error, fmt, ## __VA_ARGS__) \
		return (val); \
	}

#define ELOX_RAISE_GOTO(label, error, fmt, ...) \
	{ \
		ELOX_RAISE(error, fmt, ## __VA_ARGS__) \
		goto label; \
	}

#define ___ON_ERROR_RETURN return _error

#define ___ELOX_GET_ARG(var, args, idx, IS, AS, TYPE, ON_ERROR) \
	{ \
		Value val = getValueArg(args, idx); \
		if (ELOX_LIKELY(IS(val))) { \
			*(var) = AS(val); \
		} else { \
			Value _error = runtimeError(args->vmCtx, "Invalid argument type, expecting TYPE"); \
			ON_ERROR; \
		} \
	}

#define ELOX_GET_STRING_ARG_ELSE_RET(var, args, idx) \
	___ELOX_GET_ARG(var, args, idx, IS_STRING, AS_STRING, string, ___ON_ERROR_RETURN)

#define ELOX_GET_NUMBER_ARG_ELSE_RET(var, args, idx) \
	___ELOX_GET_ARG(var, args, idx, IS_NUMBER, AS_NUMBER, number, ___ON_ERROR_RETURN)

int elox_printf(VMCtx *vmCtx, EloxIOStream stream, const char *format, ...) ELOX_PRINTF(3, 4);

int elox_vprintf(VMCtx *vmCtx, EloxIOStream stream, const char *format, va_list args);

#define ELOX_WRITE(vmCtx, stream, string_literal) \
	(vmCtx)->write(stream, ELOX_STR_AND_LEN(string_literal))

bool setInstanceField(ObjInstance *instance, ObjString *name, Value value);

typedef struct ExecContext {
	VMCtx *vmCtx;
	bool error;
} ExecContext;

#define EXEC_CTX_INITIALIZER(VMCTX) { \
	.vmCtx = (VMCTX), \
	.error = false \
}

EloxInterpretResult run(VMCtx *vmCtx, int exitFrame);
Value doCall(VMCtx *vmCtx, int argCount);
bool isCallable(Value val);
bool isFalsey(Value value);
Value toString(ExecContext *execCtx, Value value);

#endif // ELOX_VM_H
