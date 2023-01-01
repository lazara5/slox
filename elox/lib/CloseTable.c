#include <elox/CloseTable.h>
#include <elox/object.h>
#include <elox/state.h>

// Based on the deterministic hash table described by Jason Orendorff
// (see https://wiki.mozilla.org/User:Jorend/Deterministic_hash_tables).
// Originally attributed to Tyler Close

void initCloseTable(CloseTable *table) {
	table->count = 0;
	table->entriesCount = 0;
	table->tableSize = 0;
	table->modCount = 0;
	table->entries = NULL;
}

void freeCloseTable(VMCtx *vmCtx, CloseTable *table) {
	FREE_ARRAY(vmCtx, TableEntry, table->entries, table->tableSize);
	initCloseTable(table);
}

static TableEntry *lookup(ExecContext *execCtx, CloseTable *table, Value key, uint32_t keyHash) {
	uint32_t bucket = keyHash & (table->tableSize - 1);

	int32_t idx = table->entries[bucket].chain;
	while (idx >= 0) {
		TableEntry *entry = &table->entries[idx];
		if (!IS_UNDEFINED(key)) {
			if (valuesEquals(execCtx, entry->key, key))
				return entry;
		}
		idx = entry->next;
	}

	return NULL;
}

bool closeTableGet(ExecContext *execCtx, CloseTable *table, Value key, Value *value) {
	if (table->count == 0)
		return false;

	uint32_t keyHash = hashValue(execCtx, key);
	if (ELOX_UNLIKELY(execCtx->error))
		return false;

	TableEntry *e = lookup(execCtx, table, key, keyHash);
	if (e != NULL) {
		*value = e->value;
		return true;
	}

	return false;
}

int32_t closeTableGetNext(CloseTable *table, int32_t start, TableEntry **valueEntry) {
	if (start < 0) {
		return -1;
	}

	for (int i = start; i < table->entriesCount; i++) {
		TableEntry *entry = &table->entries[i];
		if (!IS_UNDEFINED(entry->key)) {
			*valueEntry = entry;
			return i + 1;
		}
	}

	return -1;
}

static void rehash(ExecContext *execCtx, CloseTable *table, int32_t newSize) {
	VMCtx *vmCtx = execCtx->vmCtx;

	if (newSize == 0)
		newSize = 8;

	if (newSize == table->tableSize) {
		// rehash in place
		// TODO: implement
	} else {
		TableEntry *newEntries = ALLOCATE(vmCtx, TableEntry, newSize);
		for (int i = 0; i < newSize; i++)
			newEntries[i].chain = -1;

		TableEntry *q = newEntries;
		for (TableEntry *p = table->entries, *end = table->entries + table->entriesCount; p != end; p++) {
			if (!IS_UNDEFINED(p->key)) {
				uint32_t keyHash = hashValue(execCtx, p->key);
				if (ELOX_UNLIKELY(execCtx->error))
					return;
				uint32_t bucket = keyHash & (newSize - 1);
				q->key = p->key;
				q->value = p->value;
				q->next = newEntries[bucket].chain;
				newEntries[bucket].chain = q - newEntries;
				q++;
			}
		}

		TableEntry *oldEntries = table->entries;
		int32_t oldSize = table->tableSize;

		table->entries = newEntries;
		table->tableSize = newSize;
		table->entriesCount = table->count;

		FREE_ARRAY(vmCtx, TableEntry, oldEntries, oldSize);
	}
}

bool closeTableSet(ExecContext *execCtx, CloseTable *table, Value key, Value value) {
	uint32_t keyHash = hashValue(execCtx, key);
	if (ELOX_UNLIKELY(execCtx->error))
		return false;

	TableEntry *e = NULL;
	if (table->count > 0) {
		TableEntry *e = lookup(execCtx, table, key, keyHash);
		if (e != NULL) {
			e->value = value;
			return false;
		}
	}

	table->modCount++;

	if (table->entriesCount == table->tableSize) {
		rehash(execCtx, table,
			   table->count >= table->tableSize * 0.75
					? 2 * table->tableSize
					: table->tableSize);
	}

	table->count++;
	e = &table->entries[table->entriesCount++];
	e->key = key;
	e->value = value;
	uint32_t bucket = keyHash & (table->tableSize - 1);
	e->next = table->entries[bucket].chain;
	table->entries[bucket].chain = table->entriesCount - 1;

	return true;
}

bool closeTableDelete(ExecContext *execCtx, CloseTable *table, Value key) {
	if (table->count == 0)
		return false;

	uint32_t keyHash = hashValue(execCtx, key);
	if (ELOX_UNLIKELY(execCtx->error))
		return false; // TODO

	TableEntry *e = lookup(execCtx, table, key, keyHash);
	if (e == NULL)
		return false;

	table->modCount++;

	e->key = UNDEFINED_VAL;
	table->count--;

	return true;
}

void markCloseTable(VMCtx *vmCtx, CloseTable *table) {
	for (int32_t i = 0; i < table->entriesCount; i++) {
		TableEntry *entry = &table->entries[i];
		if (!IS_UNDEFINED(entry->key)) {
			markValue(vmCtx, entry->key);
			markValue(vmCtx, entry->value);
		}
	}
}