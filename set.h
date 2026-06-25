#ifndef SET_H
#define SET_H
#include "hashtable.h"

// Множество - обертка над HashTable
// Множество хранит только уникальные элементы, value всегда = 1
// 
// Ключ = значение элемента, value всегда 1
typedef struct {
	HashTable* ht; // внутри хэш-таблица
} Set;

// Создать пустое множество
Set* set_create(int capacity);

// Удаляет множество и освобождает память
void set_destroy(Set* set);

// Добавить элемент
void set_add(Set* set, const char* key);

// Проверяет, есть ли элемент в массиве
bool set_contains(const Set* set, const char* key);

// Удаляет элемент
bool set_remove(Set* set, const char* key);

// Количество уникальных элементов
int set_size(const Set* set);

// Обьединение
Set* set_union(const Set* a, const Set* b);

// Пересечение
Set* set_intersection(const Set* a, const Set* b);

// Разность A \ B
Set* set_difference(const Set* a, const Set* b);

// Выводит множество 
void set_print(const Set* set);

#endif