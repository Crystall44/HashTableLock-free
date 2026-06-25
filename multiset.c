#include "multiset.h"
#include <stdlib.h>
#include <stdio.h>

// Мультимножество - хэш-таблица, где значение - целое число (сколько раз элемент был добавлен)
MultiSet* multiset_create(int capacity) {
	MultiSet* ms = malloc(sizeof(MultiSet));
	ms->ht = ht_create(capacity);
	return ms;
}

void multiset_destroy(MultiSet* ms) {
	if (!ms) return;
	ht_destroy(ms->ht);
	free(ms);
}

// Добавляем элемент count раз
void multiset_add(MultiSet* ms, const char* key, int count) {
	int current = 0;
	//Смотрим сколько уже есть
	ht_get(ms->ht, key, &current);

	// Кладем сумму
	ht_put(ms->ht, key, current + count);
}

// Удаляет элемент count раз
bool multiset_remove(MultiSet* ms, const char* key, int count) {
	int current = 0;

	// Если элемента нет - удалять нечего
	if (!ht_get(ms->ht, key, &current)) return false;

	// Считаем, сколько останется
	int new_count = current - count;

	if (new_count <= 0) {
		//Убрали всё (или больше) - удаляем ключ
		return ht_remove(ms->ht, key);
	}
	else {
		// Иначе - обновляем счетчик
		ht_put(ms->ht, key, new_count);
		return true;
	}
}

// Возвращает число вхождение элемента
int multiset_count(const MultiSet* ms, const char* key) {
	int current = 0;
	ht_get(ms->ht, key, &current);
	return current;
}

bool multiset_contains(const MultiSet* ms, const char* key) {
	return multiset_count(ms, key) > 0;
}

void multiset_print(const MultiSet* ms) {
	printf("{ ");
	for (int i = 0; i < ms->ht->capacity; i++) {
		HNode* current = ms->ht->nodes[i];
		while (current) {
			printf("\"%s\":%d ", current->key, current->value);
			current = current->next;
		}
	}
	printf("}\n");
}