#include "table.h"
#include <stdlib.h>

TableItem* createTableItem(StringObject* key, void* value, TableItem* next)
{
    TableItem* item = malloc(sizeof(TableItem));
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

    free(item);
}

void initTable(Table* table, int capacity)
{
    table->capacity = capacity;
    table->data = calloc(table->capacity, sizeof(TableItem*));
    table->count = 0;
}

void freeTable(Table* table)
{
    for (int i = 0; i < table->capacity; i++) {
        TableItem* item = table->data[i];

        if (item) {
            freeTableItem(item);
        }
    }

    free(table->data);
}

size_t countTable(Table* table)
{
    return table->count;
}

void* getTableAt(Table* table, StringObject* key)
{
    if (table->count == 0) {
        return NULL;
    }

    size_t index = key->hash % table->capacity;
    TableItem* current = table->data[index];

    while (current && !compareString(current->key, key)) {
        current = current->next;
    }

    if (current) {
        return current->value;
    }
    
    return NULL;
}

bool setTableAt(Table* table, StringObject* key, void* value)
{
    size_t index = key->hash % table->capacity;
    TableItem* current = table->data[index];

    while (current) {
        if (compareString(current->key, key)) {
            return false;
        }

        current = current->next;
    }

    TableItem* next = table->data[index];
    table->data[index] = createTableItem(key, value, next);
    table->count++;

    return true;
}

bool deleteTableAt(Table* table, StringObject* key)
{
    size_t index = key->hash % table->capacity;
    TableItem* prev = NULL;
    TableItem* current = table->data[index];

    while (current && !compareString(current->key, key)) {
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

    freeTableItem(current);

    return true;
}
