#ifndef TABLE_H
#define TABLE_H

#include "common/string_view.h"
#include <stddef.h>

#define DEFAULT_TABLE_CAPACITY 8

typedef struct TableItem TableItem;

typedef struct TableItem
{
    StringView key;
    void* value;
    TableItem* next;
} TableItem;

typedef struct Table
{
    TableItem** data;
    size_t capacity;
    size_t count;
} Table;

TableItem* createTableItem(StringView key, void* value, TableItem* next);
void freeTableItem(TableItem* item);
void initTable(Table* table, size_t capacity);
void freeTable(Table* table);
size_t countTable(Table* table);
void* getTableAt(Table* table, StringView key);
bool setTableAt(Table* table, StringView key, void* value);
bool deleteTableAt(Table* table, StringView key);

#endif
