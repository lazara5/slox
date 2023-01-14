#ifndef ELOX_BUILTINS_H
#define ELOX_BUILTINS_H

#include "elox/state.h"

static String eloxBuiltinModule = STRING_INITIALIZER("<builtin>");

void registerBuiltins(VMCtx *vmCtx);

void markBuiltins(VMCtx *vmCtx);

void clearBuiltins(VM *vm);

//--- String ----------------------

Value stringFmt(Args *args);
Value printFmt(Args *args);
Value stringMatch(Args *args);
Value stringGsub(Args *args);
Value gmatchIteratorHasNext(Args *args);
Value gmatchIteratorNext(Args *args);
Value stringGmatch(Args *args);
Value stringStartsWith(Args *args);
Value stringEndsWith(Args *args);
Value stringUpper(Args *args);
Value stringLower(Args *args);

#endif // ELOX_BUILTINS_H
