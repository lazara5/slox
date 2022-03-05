#ifndef SLOX_VALUE_TABLE_H
#define SLOX_VALUE_TABLE_H

#include "common.h"
#include "value.h"

typedef struct ExecContext ExecContext;

typedef struct {
	Value key;
	Value value;
} ValueEntry;

typedef struct {
	ValueEntry *entries;
	int count;
	int capacity;
	int modCount;
} ValueTable;

void initValueTable(ValueTable *table);
void freeValueTable(VMCtx *vmCtx, ValueTable *table);
bool valueTableGet(ValueTable *table, Value key, uint32_t keyHash, Value *value);
int valueTableGetNext(ValueTable *table, int start, ValueEntry **valueEntry);
bool valueTableSet(VMCtx *vmCtx, ExecContext *execCtx, ValueTable *table, Value key, Value value);
bool valueTableDelete(ValueTable *table, Value key, uint32_t keyHash);
void markValueTable(VMCtx *vmCtx, ValueTable *table);

uint32_t hashValue(VMCtx *vmCtx, ExecContext *execCtx, Value value);

#endif // SLOX_VALUE_TABLE_H
