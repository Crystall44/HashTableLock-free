#include "set.h"
#include <stdlib.h>
#include <stdio.h>

Set* set_create(int capacity) {
	Set* set = malloc(sizeof(Set));
	set->ht = ht_create(capacity);
	return set;
}

void set_destroy(Set* set) {
	if (!set) return;
	ht_destroy(set->ht);
	free(set);
}

// Добавить элемент во множество. Если уже есть то ничего не делаем (без дубликатов)
void set_add(Set* set, const char* key) {
	// Просто кладем 1 по ключу
	ht_put(set->ht, key, 1);
}

bool set_contains(const Set* set, const char* key) {
	return ht_contains(set->ht, key);
}

bool set_remove(Set* set, const char* key) {
	return ht_remove(set->ht, key);
}

int set_size(const Set* set) {
	return ht_size(set->ht);
}

// Обьединение множеств - все элементы множества из A плюс все из B
Set* set_union(const Set* a, const Set* b) {
	// Создаем новое множество, размер побольше
	Set* result = set_create(a->ht->capacity + b->ht->capacity);

	// Проходим по всем цепочкам таблицы A
	for (int i = 0; i < a->ht->capacity; i++) {
		HNode* current = a->ht->nodes[i];
		while (current) {
			set_add(result, current->key);
			current = current->next;
		}
	}

	// Проходим по всем цепочкам таблицы B
	for (int i = 0; i < b->ht->capacity; i++) {
		HNode* current = b->ht->nodes[i];
		while (current) {
			set_add(result, current->key);
			current = current->next;
		}
	}

	return result;
}

// Пересичение множеств: только те элементы, которые есть и в A, и в B
Set* set_intersection(const Set* a, const Set* b) {
	// Выбираем меньшее множество для перебора
	int cap = (a->ht->capacity < b->ht->capacity) ? a->ht->capacity : b->ht->capacity;
	Set* result = set_create(cap);

	// Перебираем элементы множества A
	for (int i = 0; i < a->ht->capacity; i++) {
		HNode* current = a->ht->nodes[i];
		while (current) {
			// Если элемент есть в B - добавляем
			if (set_contains(b, current->key)) set_add(result, current->key);
			current = current->next;
		}
	}

	return result;
}

// Разность множеств: элементы из A, которых нет в B
Set* set_difference(const Set* a, const Set* b) {
	Set* result = set_create(a->ht->capacity);

	for (int i = 0; i < a->ht->capacity; i++) {
		HNode* current = a->ht->nodes[i];
		while (current) {
			if (!set_contains(b, current->key)) set_add(result, current->key);
			current = current->next;
		}
	}

	return result;
}

void set_print(const Set* set) {
	printf("{ ");
	for (int i = 0; i < set->ht->capacity; i++) {
		HNode* current = set->ht->nodes[i];
		while (current) {
			if (!current->deleted)
				printf("\"%s\" ", current->key);
			current = current->next;
		}
	}

	printf("}\n");
}
