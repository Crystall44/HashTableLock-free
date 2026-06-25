#ifndef MULTISET_H
#define MULTISET_H
#include "hashtable.h"


// Мультимножество похоже на обычное множество, но каждый элемент может встречатся несколько раз. Хранится счетчик вхождений
// Ключ = значение элемента, а value сколько раз он встречается
typedef struct {
	HashTable* ht;
} MultiSet;

// Создает пустое мультимножество
MultiSet* multiset_create(int capacity);

// Удаляет мультимножество
void multiset_destroy(MultiSet* ms);

// Добавляет элемент count раз
void multiset_add(MultiSet* ms, const char* key, int count);

// Удаляет элемент count раз
bool multiset_remove(MultiSet* ms, const char* key, int count);

// Количество вхождений элемента
int multiset_count(const MultiSet* ms, const char* key);

// Проверяет, есть ли хотя бы одно вхождение
bool multiset_contains(const MultiSet* ms, const char* key);

// Вывести содержимое
void multiset_print(const MultiSet* ms);

#endif