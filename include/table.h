#ifndef TABLE_H
#define TABLE_H

#include "stringobject.h"
#include <stddef.h>

typedef struct TableItem TableItem;

typedef struct TableItem
{
    StringObject* key;
    void* value;
    TableItem* next;
} TableItem;

typedef struct Table
{
    TableItem** data;
    size_t capacity;
    size_t count;
} Table;

TableItem* createTableItem(StringObject* key, void* value, TableItem* next);
void freeTableItem(TableItem* table);
void initTable(Table* table, int capacity);
void freeTable(Table* table);
size_t countTable(Table* table);
void* getTableAt(Table* table, StringObject* key);
bool setTableAt(Table* table, StringObject* key, void* value);
bool deleteTableAt(Table* table, StringObject* key);

#endif
