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

TableItem* create_table_item(StringView key, void* value, TableItem* next);
void free_table_item(TableItem* item);
void init_table(Table* table, size_t capacity);
void free_table(Table* table);
size_t count_table(const Table* table);
void* get_table_at(const Table* table, StringView key);
bool set_table_at(Table* table, StringView key, void* value);
bool delete_table_at(Table* table, StringView key);

#endif
