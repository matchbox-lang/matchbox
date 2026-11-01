#include "common/table.h"
#include "common/memory.h"
#include "common/string_view.h"
#include "common/util.h"
#include <stdbool.h>
#include <stdlib.h>

TableItem* create_table_item(StringView key, void* value, TableItem* next)
{
    TableItem* item = allocate_memory(sizeof(TableItem));
    item->key = key;
    item->value = value;
    item->next = next;

    return item;
}

void free_table_item(TableItem* item)
{
    if (item->next) {
        free_table_item(item->next);
    }

    free_memory(item);
}

void init_table(Table* table, size_t capacity)
{
    if (capacity == 0) {
        capacity = DEFAULT_TABLE_CAPACITY;
    }

    table->capacity = capacity;
    table->data = allocate_zeroed_memory(table->capacity, sizeof(TableItem*));
    table->count = 0;
}

void free_table(Table* table)
{
    for (size_t i = 0; i < table->capacity; i++) {
        TableItem* item = table->data[i];

        if (item) {
            free_table_item(item);
        }
    }

    free_memory(table->data);
}

size_t count_table(const Table* table)
{
    return table->count;
}

void* get_table_at(const Table* table, StringView key)
{
    if (table->count == 0) {
        return NULL;
    }

    size_t index = key.hash % table->capacity;
    const TableItem* current = table->data[index];

    while (current && !compare_string_view(current->key, key)) {
        current = current->next;
    }

    if (current) {
        return current->value;
    }
    
    return NULL;
}

bool set_table_at(Table* table, StringView key, void* value)
{
    size_t index = key.hash % table->capacity;
    TableItem* current = table->data[index];

    while (current) {
        if (compare_string_view(current->key, key)) {
            return false;
        }

        current = current->next;
    }

    TableItem* next = table->data[index];
    table->data[index] = create_table_item(key, value, next);
    table->count++;

    return true;
}

bool delete_table_at(Table* table, StringView key)
{
    size_t index = key.hash % table->capacity;
    TableItem* prev = NULL;
    TableItem* current = table->data[index];

    while (current && !compare_string_view(current->key, key)) {
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

    free_memory(current);
    table->count--;

    return true;
}
