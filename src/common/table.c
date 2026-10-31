#include "common/table.h"
#include "common/memory.h"
#include "common/string_view.h"
#include "common/util.h"
#include <stdbool.h>
#include <stdlib.h>

TableItem* createTableItem(StringView key, void* value, TableItem* next)
{
    TableItem* item = allocateMemory(sizeof(TableItem));
    item->key = key;
    item->value = value;
    item->next = next;

    return item;
}

void freeTableItem(TableItem* item)
{
    if (item->next) {
        freeTableItem(item->next);
    }

    freeMemory(item);
}

void initTable(Table* table, size_t capacity)
{
    if (capacity == 0) {
        capacity = DEFAULT_TABLE_CAPACITY;
    }

    table->capacity = capacity;
    table->data = allocateZeroedMemory(table->capacity, sizeof(TableItem*));
    table->count = 0;
}

void freeTable(Table* table)
{
    for (size_t i = 0; i < table->capacity; i++) {
        TableItem* item = table->data[i];

        if (item) {
            freeTableItem(item);
        }
    }

    freeMemory(table->data);
}

size_t countTable(const Table* table)
{
    return table->count;
}

void* getTableAt(const Table* table, StringView key)
{
    if (table->count == 0) {
        return NULL;
    }

    size_t index = key.hash % table->capacity;
    const TableItem* current = table->data[index];

    while (current && !compareStringView(current->key, key)) {
        current = current->next;
    }

    if (current) {
        return current->value;
    }
    
    return NULL;
}

bool setTableAt(Table* table, StringView key, void* value)
{
    size_t index = key.hash % table->capacity;
    TableItem* current = table->data[index];

    while (current) {
        if (compareStringView(current->key, key)) {
            return false;
        }

        current = current->next;
    }

    TableItem* next = table->data[index];
    table->data[index] = createTableItem(key, value, next);
    table->count++;

    return true;
}

bool deleteTableAt(Table* table, StringView key)
{
    size_t index = key.hash % table->capacity;
    TableItem* prev = NULL;
    TableItem* current = table->data[index];

    while (current && !compareStringView(current->key, key)) {
        prev = current;
        current = current->next;
    }

    if (!current) {
        return false;
    }

    if (!prev) {
        table->data[index] = current->next;
    } else {
        prev->next = current->next;
    }

    freeMemory(current);
    table->count--;

    return true;
}
