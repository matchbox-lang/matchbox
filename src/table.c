#include "table.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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

    while (current != NULL && !compareString(current->key, key)) {
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

    while (current != NULL) {
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

    while (current != NULL && !compareString(current->key, key)) {
        prev = current;
        current = current->next;
    }

    if (current == NULL) {
        return false;
    }

    if (prev == NULL) {
        table->data[index] = current->next;
    } else {
        prev->next = current->next;
    }

    freeTableItem(current);

    return true;
}
