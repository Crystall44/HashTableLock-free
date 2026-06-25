#define _CRT_NONSTDC_NO_DEPRECATE
#include "hashtable.h"
#include <stdlib.h> // Для malloc, free, calloc
#include <string.h> // Для strcmp, strdup
#include <stdio.h> // Для printf

// Хэш-функция FNV-1a (32 бита)
uint32_t fnv1a_hash(const char* key) {
	// Начальное значение хэша - не случайное
	uint32_t hash = 0x811c9dc5u;

	// Простое число, на которое будем умножать
	const uint32_t prime = 0x01000193u;

	// Идем посимвольно пока не встретим '\0'
	while (*key) {
		// Шаг 1: XOR текущего хэша с байтом строки
		hash ^= (uint8_t)(*key);

		// Шаг 2: умножаем на простое число
		hash *= prime;

		// Переходим к следующему символу
		key++;
	}

	return hash;
}

// Вспомогательные функции для узлов

// Создать узел цепочки
static HNode* node_create(const char* key, int value) {
	// Выделяем память
	HNode* node = malloc(sizeof(HNode));
	if (!node) return NULL;

	// Копируем строку ключа
	node->key = strdup(key);
	node->value = value;
	node->deleted = false;
	node->next = NULL;

	return node;
}

// Удалить узел
static void node_destroy(HNode* node) {
	if (!node) return;
	free(node->key); // Освобождаем строку
	free(node); // И сам узел
}

// Рекурсивно освободить всю цепочку
static void chain_destroy(HNode* head)
{
	while (head) {
		HNode* next = head->next;
		node_destroy(head);
		head = next;
	}
}

// Длина цепочки - только живые узлы, без метки на удаление
static int chain_live_length(HNode* head)
{
	int len = 0;
	while (head) {
		if (!head->deleted) len++;
		head = head->next;
	}
	return len;
}

// Длина цепочки - все, включая удаленные
static int chain_total_length(HNode* head)
{
	int len = 0;
	while (head) {
		len++;
		head = head->next;
	}
	return len;
}

// Очистка узлов - физическое удаление помеченных узлов. Вызывается когда их количества становится слишком большим
void ht_cleanup(HashTable* ht)
{
	int total_deleted = 0;

	for (int i = 0; i < ht->capacity; i++) {
		HNode* current = ht->nodes[i];
		HNode* pred = NULL;

		while (current) {
			if (current->deleted) {
				// Этот узел надо удалить физически 
				HNode* to_delete = current;
				current = current->next;

				if (pred) {
					pred->next = current;
				}
				else {
					ht->nodes[i] = current;
				}

				node_destroy(to_delete);
				total_deleted++;
			}
			else {
				pred = current;
				current = current->next;
			}
		}
	}

	ht->total_nodes -= total_deleted;

	if (total_deleted > 0) {
		printf("[cleanup] физически удалено %d tombstones\n", total_deleted);
	}
}

// Рехеширование (автоматическое расширение)
static void ht_resize(HashTable* ht)
{
	int old_capacity = ht->capacity;
	int new_capacity = old_capacity * 2;

	// Запоминаем старый массив цепочек
	HNode* volatile* old_nodes = ht->nodes;

	// Создаем новый, заполненный NULL
	ht->nodes = calloc(new_capacity, sizeof(HNode*));
	ht->capacity = new_capacity;
	ht->size = 0;
	ht->total_nodes = 0;

	// Перекладываем все элементы в новые цепочки
	for (int i = 0; i < old_capacity; i++) {
		HNode* chain = old_nodes[i];
		old_nodes[i] = NULL;

		while (chain) {
			// Отцепляем узел от старой цепочки
			HNode* node = chain;
			chain = chain->next;

			// Удаленные просто выбрасываем
			if (node->deleted) {
				node_destroy(node);
				continue;
			}

			// Вычисляем новый индекс
			uint32_t idx = fnv1a_hash(node->key) % new_capacity;

			// Вставляем в начало новой цепочки
			node->next = ht->nodes[idx];
			ht->nodes[idx] = node;
			ht->size++;
			ht->total_nodes++;
		}
	}
	// Освобождаем старый массив
	free((void*)old_nodes);
}

// Хэш-таблица

// Создать таблицу
HashTable* ht_create(int capacity) {
	// Выделяем память
	HashTable* ht = malloc(sizeof(HashTable));
	if (!ht) return NULL;

	ht->capacity = capacity;
	ht->size = 0;
	ht->total_nodes = 0;
	ht->tombstone_threshold = 0.5; // Чистим при > 50% удаленных
	ht->load_factor = 5.0; // Цепочка длиннее 5 - расширяем

	// Выделяем массив указателей на головы цепочек
	ht->nodes = calloc(capacity, sizeof(HNode*));

	return ht;
}

// Удалить таблицу
void ht_destroy(HashTable* ht) {
	if (!ht) return;

	// Освобождаем все цепочки
	for (int i = 0; i < ht->capacity; i++) {
		chain_destroy(ht->nodes[i]);
	}

	free((void*)ht->nodes);
	free(ht);
}

// Неблокирующая вставка (lock-free)
// Алгоритм:
// 1. Проверяем, нет ли уже такого ключа в цепочке, а если есть просто обновляем значение
// 2. Если ключа нет - создаем новый узел
// 3. Пытаемся вставить узел в начало цепочки через CAS:
// InterlockedCompareExchangePointer атомарно проверяет - голова все ещё равна head?
// Если да - заменяет на new_node, возвращает старое значение, а если нет - возвращает текущее значение (другой поток уже вставил)
// 4. Если CAS не удался то читаем новую голову и пробуем снова

// Это гарантирует: ни один поток не потеряет свой элемент, даже если несколько потоков вставляют одновременно в одну цепочку
void ht_put(HashTable* ht, const char* key, int value) {
	// Вычисляем индекс узла: берем хэш и остаток от деления
	uint32_t idx = fnv1a_hash(key) % ht->capacity;

	// Шаг 1: проверка, есть ли уже такой ключ

	HNode* head = ht->nodes[idx];
	HNode* current = head;
	HNode* first_deleted = NULL; // Первый удаленный узел с таким же ключом

	while (current) {
		if (strcmp(current->key, key) == 0) {
			if (current->deleted) {
				// нашли помеченный - запоминаем, но продолжаем искать живой
				if (!first_deleted) first_deleted = current;
			}
			else {
				// Нашли такой ключ - обновляем значение
				// Запись в int атомарна на x86/x64 для выровненных данных
				current->value = value;
				return;
			}
		}
		current = current->next;
	}

	// Если нашли помеченный с таким ключом - оживляем его
	if (first_deleted) {
		first_deleted->deleted = false;
		first_deleted->value = value;
		ht->size++;
		return;
	}

	// Шаг 2: ключа нет - создаем новый узел
	HNode* node = node_create(key, value);
	if (!node) return;

	// Шаг 3: lock-free вставка через CAS
	do {
		// Читаем текущую голову цепочки
		head = ht->nodes[idx];

		// Новый узел будет указывать на текущую голову
		node->next = head;

		// Пытаемся атомарно заменить голову на новый узел
		// InterlockedCompareExchangePointer(адрес, новое, ожидаемое_старое):
		// Если *адрес == ожидаемое старое, то пишем новое, возвращаем ожидаемое старое
		// Если *адрес != ожидаемое старое, то ничего не пишем, возвращаем *адрес
	} while (InterlockedCompareExchangePointer(
		(volatile PVOID*)&ht->nodes[idx], // Куда пишем - адрес
		(PVOID)node, // Что пишем - новый узел
		(PVOID)head // Что ожидаем увидеть - ожидаемое старое
	) != head); // Повторяем, пока не получится

	// Шаг 4: увеличиваем счетчик и проверяем
	ht->size++;
	ht->total_nodes++;

	// Если цепочка слишком длинная - расширяем таблицу
	if (chain_live_length(ht->nodes[idx]) > ht->load_factor) {
		ht_resize(ht);
	}
}

// Проверяем, есть ли ключ в таблице
bool ht_contains(const HashTable* ht, const char* key) {
	return ht_get(ht, key, NULL);
}

// Поиск - потокобезопасное чтение. Оно не требует блокировок, потому что:
// Узлы после создания не меняют свой key и next
// value может поменятся, но атомарно
// Даже если другой поток сейчас делает рехэш - мы читаем либо старую, либо новую цепочку - обе валидны
bool ht_get(const HashTable* ht, const char* key, int* out_value) {
	// Вычисляем индекс
	uint32_t idx = fnv1a_hash(key) % ht->capacity;

	// Идем по цепочке
	HNode* current = ht->nodes[idx];
	while (current) {
		// Удаленные пропускаем - их как бы нет
		if (!current->deleted && strcmp(current->key, key) == 0) {
			//Ключ найден
			if (out_value) *out_value = current->value; // Записываем значение если передан указатель
			return true;
		}
		current = current->next;
	}

	// Не нашли
	return false;
}

// Ленивое удаление. Не удаляем узел физически, а ставим флаг deleted = true
// Это безопасно для многопоточности. А физическая очистка происходит:
// При вызове ht_clenup() или при рехэшировании ht_resize()
bool ht_remove(HashTable* ht, const char* key) {
	uint32_t idx = fnv1a_hash(key) % ht->capacity;
	HNode* current = ht->nodes[idx];

	// Идем по цепочке
	while (current) {
		if (!current->deleted && strcmp(current->key, key) == 0) {
			// ленивое удаление - просто ставим флаг
			current->deleted = true;
			ht->size--;

			// Проверяем, не пора ли почистить помеченные 
			int total = ht->total_nodes;
			int live = ht->size;
			if (total > 0 && (double)(total - live) / total > ht->tombstone_threshold) {
				ht_cleanup(ht);
			}

			return true;
		}

		// Переходим к сдедующему узлу
		current = current->next;
	}

	// Прошли все, ключ не нашли
	return false;
}

// Количество элементов в таблице
int ht_size(const HashTable* ht) {
	return ht->size;
}

