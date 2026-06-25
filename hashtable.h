#ifndef HASHTABLE_H
#define HASHTABLE_H

// HashTable - неблокирующая хэш-таблица

#include <stdint.h> // Для uint32_t
#include <stdbool.h> // Для bool
#include <windows.h> // InterlockedCompareExchangePointer, InterlockedExchangePointer

// Узел цепочки (после создания не меняется - иммутабельный), кроме value
typedef struct HNode {
	char* key; // Строка-ключ
	int value; // Значение
	bool deleted; // Флаг удаления
	struct HNode* next; // Указатель на следующий узел
} HNode;

// Хэш-таблица
typedef struct {
	HNode* volatile* nodes; // Массив цепочек
	int capacity; // Количество связных списков
	int size; // Сколько всего элементов в таблице - живых
	int total_nodes; // Всего, даже удаленных 
	double load_factor; // Макс. длина цепочки до расширения
	double tombstone_threshold; // Порог для очистки
} HashTable;

// Хэш-функция
uint32_t fnv1a_hash(const char* key);

// Хэш-таблица
HashTable* ht_create(int capacity);
void ht_destroy(HashTable* ht);
void ht_put(HashTable* ht, const char* key, int value);
bool ht_contains(const HashTable* ht, const char* key);
bool ht_get(const HashTable* ht, const char* key, int* out_value);
bool ht_remove(HashTable* ht, const char* key); // Ленивое удаление
int ht_size(const HashTable* ht);
void ht_cleanup(HashTable* ht); // Физическое удаление

#endif


