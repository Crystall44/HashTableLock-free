#include <stdio.h>
#include <string.h>
#include <locale.h>
#include <windows.h>
#include "hashtable.h"
#include "set.h"
#include "multiset.h"

#define NUM_THREADS 4
#define ITEMS_PER_THREAD 500
#define ITEMS_PER_THREAD_2 250

// Счетчики для итогов
static int passed = 0;
static int failed = 0;

// Функция для проверки условий и вывода результата
static void check(const char* description, int condition) {
	if (condition) {
		printf("Успешно %s\n", description);
		passed++;
	}
	else {
		printf("Ошибка %s\n", description);
		failed++;
	}
}

// Тест 1: базовые операции хэш-таблицы
static void test_basic_operations(void) {
	printf("\nТест 1: базовые операции\n");
	HashTable* ht = ht_create(16);

	check("пустая таблица, размер 0", ht_size(ht) == 0);

	// Вставка
	ht_put(ht, "apple", 100);
	ht_put(ht, "banana", 200);
	ht_put(ht, "cherry", 300);

	check("размер == 3", ht_size(ht) == 3);

	// Поиск существующих ключей
	int val;
	check("apple == 100", ht_get(ht, "apple", &val) && val == 100);
	check("banana == 200", ht_get(ht, "banana", &val) && val == 200);
	check("cherry == 300", ht_get(ht, "cherry", &val) && val == 300);
	check("grape отсутствует", !ht_contains(ht, "grape"));

	// Поиск несуществующих ключей
	check("six не найден", !ht_contains(ht, "six"));
	check("пустая строка не найдена", !ht_contains(ht, ""));
	check("\"hello\" не найден", !ht_contains(ht, "hello"));

	// Обновление существующего ключа
	ht_put(ht, "apple", 999);
	check("обновление apple == 999", ht_get(ht, "apple", &val) && val == 999);
	check("размер не изменился (3)", ht_size(ht) == 3);

	// Многократное обновление
	ht_put(ht, "apple", 999);
	ht_put(ht, "apple", 9999);
	ht_put(ht, "apple", 99999);
	check("apple после трёх обновлений == 99999", ht_get(ht, "apple", &val) && val == 99999);
	check("размер всё ещё 3", ht_size(ht) == 3);

	// Удаление
	check("удаление banana", ht_remove(ht, "banana"));
	check("размер == 2", ht_size(ht) == 2);
	check("banana отсутствует", !ht_contains(ht, "banana"));
	check("повторное удаление banana", !ht_remove(ht, "banana"));

	// ht_get с NULL (только проверка наличия)
	check("ht_get(apple, NULL) == true", ht_get(ht, "apple", NULL));
	check("ht_get(banana, NULL) == false", !ht_get(ht, "banana", NULL));

	ht_destroy(ht);
}

// Тест 2: коллизии
static void test_collisions(void) {
	printf("\nТест 2: работа с коллизиями\n");

	// Много элементов в маленькой таблице
	printf("Много ключей, мало цепочек (4 цепочки, 20 ключей)\n");
	HashTable* ht = ht_create(4);

	char key[8];
	for (int i = 0; i < 20; i++) {
		snprintf(key, sizeof(key), "k%d", i);
		ht_put(ht, key, i * 10);
	}
	check("размер == 20", ht_size(ht) == 20);

	// Проверяем, что все значения правильные
	int all_correct = 1;
	int val;
	for (int i = 0; i < 20; i++) {
		snprintf(key, sizeof(key), "k%d", i);
		if (!ht_get(ht, key, &val) || val != i * 10) {
			all_correct = 0;
			break;
		}
	}
	check("все 20 ключей найдены с правильными значениями", all_correct);

	// Удаление части элементов из цепочки
	check("удаление k0", ht_remove(ht, "k0"));
	check("удаление k5", ht_remove(ht, "k5"));
	check("удаление k10", ht_remove(ht, "k10"));
	check("удаление k15", ht_remove(ht, "k15"));
	check("удаление k19", ht_remove(ht, "k19"));

	check("размер == 15", ht_size(ht) == 15);
	check("k0 отсутствует", !ht_contains(ht, "k0"));
	check("k5 отсутствует", !ht_contains(ht, "k5"));
	check("k1 всё ещё есть", ht_contains(ht, "k1"));
	check("k18 всё ещё есть", ht_contains(ht, "k18"));

	// Вставка новых элемнтов после удаления
	ht_put(ht, "new1", 100);
	ht_put(ht, "new2", 200);
	ht_put(ht, "new3", 300);
	check("размер == 18", ht_size(ht) == 18);
	check("new1 == 100", ht_get(ht, "new1", &val) && val == 100);
	check("new2 == 200", ht_get(ht, "new2", &val) && val == 200);
	check("new3 == 300", ht_get(ht, "new3", &val) && val == 300);

	ht_destroy(ht);
}

// Тест 3: Автоматическое расширение
static void test_resize(void) {
	printf("\nТест 3: Автоматическое расширение\n");

	// Начинаем с 4 цепочек - таблица должнга расшириться
	HashTable* ht = ht_create(4);

	printf("Вставляем 100 элементов в таблицу из 4 цепочек...\n");
	char key[16];
	for (int i = 0; i < 100; i++) {
		snprintf(key, sizeof(key), "key%d", i);
		ht_put(ht, key, i * 10);
	}

	printf("Начальная capacity: 4\n");
	printf("Итоговая capacity: %d\n", ht->capacity);

	check("таблица расширилась (capacity > 4)", ht->capacity > 4);
	check("размер == 100", ht_size(ht) == 100);

	// Проверяем что все элементы на месте
	int all_found = 1;
	int val;
	for (int i = 0; i < 100; i++) {
		snprintf(key, sizeof(key), "key%d", i);
		if (!ht_get(ht, key, &val) || val != i * 10) {
			all_found = 0;
			printf(" потерян: %s\n", key);
			break;
		}
	}

	check("все 100 элементов найдены после расширения", all_found);

	ht_destroy(ht);
}

// Тест 4: множество
static void test_set(void) {
	printf("\nТест 4: множество (Set)\n");

	// Создание и базовые операции
	printf("Создание и базовые операции\n");
	Set* s = set_create(16);
	check("пустое множество, размер 0", set_size(s) == 0);
	check("пустое не содержит \"a\"", !set_contains(s, "a"));

	set_add(s, "red");
	set_add(s, "green");
	set_add(s, "blue");

	check("размер == 3", set_size(s) == 3);
	check("red есть", set_contains(s, "red"));
	check("yellow нет", !set_contains(s, "yellow"));

	set_add(s, "red");
	check("размер не изменился (дубликат)", set_size(s) == 3);

	check("удаление green", set_remove(s, "green"));
	check("размер == 2", set_size(s) == 2);
	check("green отсутствует", !set_contains(s, "green"));
	check("повторное удаление green - false", !set_remove(s, "green"));

	printf("Содержимое: ");
	set_print(s);

	printf("\nОперации над множествами\n");
	Set* a = set_create(16);
	Set* b = set_create(16);

	set_add(a, "red");
	set_add(a, "green");
	set_add(a, "blue");

	set_add(b, "blue");
	set_add(b, "yellow");
	set_add(b, "purple");

	printf("A: "); set_print(a);
	printf("B: "); set_print(b);

	Set* u = set_union(a, b);
	printf("A union B: "); set_print(u);

	check("union размер == 5", set_size(u) == 5);
	check("red есть", set_contains(u, "red"));
	check("green есть", set_contains(u, "green"));
	check("blue есть", set_contains(u, "blue"));
	check("yellow есть", set_contains(u, "yellow"));
	check("purple есть", set_contains(u, "purple"));

	Set* inter = set_intersection(a, b);
	printf("A intersect B: "); set_print(inter);

	check("пересечение размер == 1", set_size(inter) == 1);
	check("blue есть в пересечении", set_contains(inter, "blue"));
	check("red нет в пересечении", !set_contains(inter, "red"));

	Set* diff_ab = set_difference(a, b);
	printf("A \\ B: "); set_print(diff_ab);

	check("A\\B размер == 2", set_size(diff_ab) == 2);
	check("red есть в A\\B", set_contains(diff_ab, "red"));
	check("green есть в A\\B", set_contains(diff_ab, "green"));
	check("blue нет в A\\B", !set_contains(diff_ab, "blue"));

	Set* diff_ba = set_difference(b, a);
	printf("B \\ A: "); set_print(diff_ba);

	check("B\\A размер == 2", set_size(diff_ba) == 2);
	check("yellow есть в B\\A", set_contains(diff_ba, "yellow"));
	check("purple есть в B\\A", set_contains(diff_ba, "purple"));

	// Операции с пустым множеством
	printf("\nОперации с пустым множеством\n");
	Set* empty = set_create(8);
	Set* union_with_empty = set_union(a, empty);
	check("A union пустое == A (размер 3)", set_size(union_with_empty) == 3);

	Set* inter_with_empty = set_intersection(a, empty);
	check("A пересечение с пустым == пустое (размер 0)", set_size(inter_with_empty) == 0);

	Set* diff_with_empty = set_difference(a, empty);
	check("A \\ пустое == A (размер 3)", set_size(diff_with_empty) == 3);

	// Множество с одинаковыми элементами
	printf("\nПересечение множества с самим собой\n");
	Set* same = set_intersection(a, a);
	check("A пересечение с A == A (размер 3)", set_size(same) == 3);

	Set* diff_same = set_difference(a, a);
	check("A \\ A == пустое (размер 0)", set_size(diff_same) == 0);

	set_destroy(s);
	set_destroy(a);
	set_destroy(b);
	set_destroy(u);
	set_destroy(inter);
	set_destroy(diff_ab);
	set_destroy(diff_ba);
	set_destroy(empty);
	set_destroy(union_with_empty);
	set_destroy(inter_with_empty);
	set_destroy(diff_with_empty);
	set_destroy(same);
	set_destroy(diff_same);
}

// Тест 5: мультимножество
static void test_multiset(void) {
	printf("\nТест 5: мультимножество\n");

	// Создание
	printf("Создание мультимножества и добавление элементов\n");
	MultiSet* ms = multiset_create(16);
	check("мультимножество создано", ms != NULL);

	multiset_add(ms, "apple", 5);
	check("apple == 5", multiset_count(ms, "apple") == 5);

	multiset_add(ms, "banana", 3);
	check("banana == 3", multiset_count(ms, "banana") == 3);

	multiset_add(ms, "cherry", 0);
	check("cherry == 0 (добавили 0)", multiset_count(ms, "cherry") == 0);

	printf("Содержимое: ");
	multiset_print(ms);

	// Добавление к существующему
	printf("\nДобавление к существующему элементу\n");
	multiset_add(ms, "apple", 3);
	check("apple == 8", multiset_count(ms, "apple") == 8);

	multiset_add(ms, "banana", 1);
	check("banana == 4", multiset_count(ms, "banana") == 4);

	printf("После добавления: ");
	multiset_print(ms);

	// Проверка наличия
	printf("\nПроверка наличия\n");
	check("apple содержится", multiset_contains(ms, "apple"));
	check("banana содержится", multiset_contains(ms, "banana"));
	check("cherry не содержится (добавляли 0)", !multiset_contains(ms, "cherry"));
	check("grape не содержится", !multiset_contains(ms, "grape"));

	// Частичное удаление
	printf("\nЧастичное удаление\n");
	check("удаление 2 apple", multiset_remove(ms, "apple", 2));
	check("apple == 6 (было 8, убрали 2)", multiset_count(ms, "apple") == 6);
	check("apple всё ещё содержится", multiset_contains(ms, "apple"));

	check("удаление 1 banana", multiset_remove(ms, "banana", 1));
	check("banana == 3", multiset_count(ms, "banana") == 3);

	printf("После частичного удаления: ");
	multiset_print(ms);

	// Полное удаление
	printf("\nПолное удаление (счётчик в 0)\n");
	check("удаление 3 banana", multiset_remove(ms, "banana", 3));
	check("banana == 0", multiset_count(ms, "banana") == 0);
	check("banana не содержится", !multiset_contains(ms, "banana"));

	printf("После полного удаления banana: ");
	multiset_print(ms);

	// Удаление больше, чем есть
	printf("\nУдаление больше, чем есть\n");
	check("удаление 100 apple", multiset_remove(ms, "apple", 100));
	check("apple == 0", multiset_count(ms, "apple") == 0);
	check("apple не содержится", !multiset_contains(ms, "apple"));

	printf("После удаления всего: ");
	multiset_print(ms);

	// Удаление несуществующего
	printf("\nУдаление несуществующего элемента\n");
	check("удаление несуществующего", !multiset_remove(ms, "grape", 1));
	check("удаление уже удалённого", !multiset_remove(ms, "banana", 1));

	// Снова добавляем
	printf("\nПовторное добавление после удаления\n");
	multiset_add(ms, "apple", 10);
	check("apple == 10", multiset_count(ms, "apple") == 10);

	multiset_add(ms, "banana", 7);
	check("banana == 7", multiset_count(ms, "banana") == 7);

	printf("Финальное состояние: ");
	multiset_print(ms);

	multiset_destroy(ms);
}

// Тест 6: многопоточная вставка
typedef struct {
	HashTable* ht;
	int thread_id;
	int items_per_thread;
} ThreadData;

// Функция, которую выполняет каждый поток
static DWORD WINAPI thread_insert(LPVOID arg) {
	ThreadData* data = (ThreadData*)arg;
	char key[32];

	for (int i = 0; i < data->items_per_thread; i++) {
		// Каждый поток создает уникальные ключи
		snprintf(key, sizeof(key), "thread%d_item%d", data->thread_id, i);
		ht_put(data->ht, key, data->thread_id * 1000 + i);
	}
	return 0;
}

static void test_multithread(void) {
	printf("\nТест 6: многопоточная вставка\n");

	HashTable* ht = ht_create(4096);
	ht->load_factor = 10000.0;

	HANDLE threads[NUM_THREADS];
	ThreadData data[NUM_THREADS];

	printf("Запускаем %d потоков, каждый вставляет %d элемиентов...\n", NUM_THREADS, ITEMS_PER_THREAD);
	printf("Всего будет вставлено: %d элементов\n\n", NUM_THREADS * ITEMS_PER_THREAD);

	// Создаем потоки
	for (int i = 0; i < NUM_THREADS; i++) {
		data[i].ht = ht;
		data[i].thread_id = i;
		data[i].items_per_thread = ITEMS_PER_THREAD;
		threads[i] = CreateThread(
			NULL, // Атрибуты безопасности (по умолчанию)
			0, // Размера стека (по умолчанию)
			thread_insert, // Функция потока
			&data[i], // Аргумент функции
			0, // Флаги создание (0 = запустить сразу)
			NULL // ID потока (не нужен)
		);
	}

	// Ждем завершения всех потоков
	printf("Ожидаем завершения всех потоков...\n");
	WaitForMultipleObjects(NUM_THREADS, threads, TRUE, INFINITE);

	for (int i = 0; i < NUM_THREADS; i++) {
		CloseHandle(threads[i]);
	}

	printf("Все потоки завершены.\n\n");
	printf("Результаты:\n");
	printf("Размер таблицы: %d (ожидалось %d)\n", ht_size(ht), NUM_THREADS * ITEMS_PER_THREAD);

	check("размер == 2000", ht_size(ht) == (size_t)(NUM_THREADS * ITEMS_PER_THREAD));

	// проверяем, что все элементы на месте и с правильными значениями */
	printf("\nПроверяем все %d элементов...\n", NUM_THREADS * ITEMS_PER_THREAD);

	int lost = 0;
	int wrong_value = 0;
	int val;
	char key[32];

	for (int t = 0; t < NUM_THREADS; t++) {
		for (int i = 0; i < ITEMS_PER_THREAD; i++) {
			snprintf(key, sizeof(key), "thread%d_item%d", t, i);

			if (!ht_get(ht, key, &val)) {
				lost++;

				if (lost <= 3) {
					printf("ПОТЕРЯН КЛЮЧ: %s\n", key);
				}
			}
			else if (val != t * 1000 + i) {
				wrong_value++;

				if (wrong_value <= 3) {

					printf("НЕВЕРНОЕ ЗНАЧЕНИЕ: %s = %d (ожидалось %d)\n", key, val, t * 1000 + i);
				}
			}
		}
	}

	if (lost == 0 && wrong_value == 0) {
		printf("Все элементы найдены, все значения верны.\n");
	}
	else {
		printf("Потеряно ключей: %d\n", lost);
		printf("Неверных значений: %d\n", wrong_value);
	}

	check("нет потерянных ключей", lost == 0);
	check("все значения правильные", wrong_value == 0);

	ht_destroy(ht);
}

// Тест 7: ленивое удаление (tombstones)
static void test_lazy_deletion(void) {
	printf("\nТест 7: ленивое удаление (tombstones)\n");

	HashTable* ht = ht_create(8);

	// Вставляем 5 элементов
	ht_put(ht, "a", 1);
	ht_put(ht, "b", 2);
	ht_put(ht, "c", 3);
	ht_put(ht, "d", 4);
	ht_put(ht, "e", 5);

	check("размер == 5", ht_size(ht) == 5);
	check("total_nodes == 5", ht->total_nodes == 5);

	// Лениво удаляем два элемента
	printf("Лениво удаляем b и d (ставим флаг deleted)...\n");
	check("удаление b (ленивое)", ht_remove(ht, "b"));
	check("удаление d (ленивое)", ht_remove(ht, "d"));

	// Размер уменьшился, но узлы висят
	check("размер == 3 (живых)", ht_size(ht) == 3);
	check("total_nodes == 5 (удалённые ещё висят)", ht->total_nodes == 5);

	// Поиск не находит удалённые
	check("b не находится", !ht_contains(ht, "b"));
	check("d не находится", !ht_contains(ht, "d"));
	check("a находится", ht_contains(ht, "a"));
	check("c находится", ht_contains(ht, "c"));
	check("e находится", ht_contains(ht, "e"));

	// Оживляем tombstone через вставку
	printf("Оживляем b через ht_put...\n");
	ht_put(ht, "b", 222);
	check("b снова находится", ht_contains(ht, "b"));

	int val;
	check("b == 222", ht_get(ht, "b", &val) && val == 222);
	check("размер == 4 (оживили tombstone)", ht_size(ht) == 4);
	check("total_nodes == 5 (не создавали новый узел)", ht->total_nodes == 5);

	// Принудительная очистка
	printf("Вызов ht_cleanup() для физического удаления tombstones...\n");
	ht_cleanup(ht);
	check("после cleanup total_nodes == 4", ht->total_nodes == 4);
	check("после cleanup размер == 4", ht_size(ht) == 4);

	// Проверяем, что всё работает после очистки
	check("a всё ещё есть", ht_contains(ht, "a"));
	check("b всё ещё есть", ht_contains(ht, "b"));
	check("c всё ещё есть", ht_contains(ht, "c"));
	check("e всё ещё есть", ht_contains(ht, "e"));
	check("d нет", !ht_contains(ht, "d"));

	// Много удалений — автоматическая очистка
	printf("Создаём много tombstones для автоматической очистки...\n");
	ht_put(ht, "x1", 1);
	ht_put(ht, "x2", 2);
	ht_put(ht, "x3", 3);
	ht_put(ht, "x4", 4);
	ht_put(ht, "x5", 5);

	check("total_nodes == 9", ht->total_nodes == 9);

	// Удаляем 5 из 9 — больше 50%, должна сработать автоочистка
	ht_remove(ht, "x1");
	ht_remove(ht, "x2");
	ht_remove(ht, "x3");
	ht_remove(ht, "x4");
	ht_remove(ht, "x5");

	// После автоочистки total_nodes должно уменьшиться
	check("total_nodes уменьшился после автоочистки", ht->total_nodes < 9);

	ht_destroy(ht);
}

// Тест 8: множество в многопоточном режиме
typedef struct {
	Set* set;
	int  thread_id;
	int  items_per_thread;
} SetThreadData;

static DWORD WINAPI set_thread_insert(LPVOID arg) {
	SetThreadData* data = (SetThreadData*)arg;
	char key[32];

	for (int i = 0; i < data->items_per_thread; i++) {
		snprintf(key, sizeof(key), "thread%d_item%d", data->thread_id, i);
		set_add(data->set, key);
	}
	return 0;
}

static void test_set_multithread(void) {
	printf("\nТест 8: множество в многопоточном режиме\n");

	Set* set = set_create(4096);
	set->ht->load_factor = 10000.0;

	HANDLE threads[NUM_THREADS];
	SetThreadData data[NUM_THREADS];

	printf("Запускаем %d потоков, добавляют одинаковые ключи...\n", NUM_THREADS);

	for (int i = 0; i < NUM_THREADS; i++) {
		data[i].set = set;
		data[i].thread_id = i;
		data[i].items_per_thread = ITEMS_PER_THREAD_2;
		threads[i] = CreateThread(NULL, 0, set_thread_insert, &data[i], 0, NULL);
	}

	WaitForMultipleObjects(NUM_THREADS, threads, TRUE, INFINITE);

	for (int i = 0; i < NUM_THREADS; i++) {
		CloseHandle(threads[i]);
	}

	printf("Размер множества: %d\n", set_size(set));

	// Проверяем несколько ключей
	int expected = NUM_THREADS * ITEMS_PER_THREAD_2;  
	check("размер == 1000 (все ключи уникальные)", set_size(set) == (int)expected);

	char key[32];
	snprintf(key, sizeof(key), "thread0_item0");
	check("thread0_item0 есть", set_contains(set, key));

	snprintf(key, sizeof(key), "thread1_item100");
	check("thread1_item100 есть", set_contains(set, key));

	snprintf(key, sizeof(key), "thread3_item249");
	check("thread3_item249 есть", set_contains(set, key));

	snprintf(key, sizeof(key), "thread0_item999");
	check("thread0_item999 нет", !set_contains(set, key));

	set_destroy(set);
}

// Тест 9: примеры из жизни
static void test_real(void) {
	printf("\nТест 9: примеры реального использования\n");

	// Частотный словарь
	printf("\nХеш-таблица как частотный словарь\n");
	HashTable* freq = ht_create(32);
	const char* words[] = {
		"hello", "world", "hello", "hash", "table",
		"world", "hello", "c", "hash", "function",
		"hello", "world", NULL
	};

	for (int i = 0; words[i]; i++) {
		int cnt = 0;
		ht_get(freq, words[i], &cnt);
		ht_put(freq, words[i], cnt + 1);
	}

	int val;
	printf("Статистика слов:\n");
	printf("hello: %d (ожидалось 4)\n", ht_get(freq, "hello", &val) ? val : 0);
	printf("world: %d (ожидалось 3)\n", ht_get(freq, "world", &val) ? val : 0);
	printf("hash:  %d (ожидалось 2)\n", ht_get(freq, "hash", &val) ? val : 0);
	printf("table: %d (ожидалось 1)\n", ht_get(freq, "table", &val) ? val : 0);
	printf("function: %d (ожидалось 1)\n", ht_get(freq, "function", &val) ? val : 0);
	printf("c: %d (ожидалось 1)\n", ht_get(freq, "c", &val) ? val : 0);

	check("hello == 4", ht_get(freq, "hello", &val) && val == 4);
	check("world == 3", ht_get(freq, "world", &val) && val == 3);
	check("hash == 2", ht_get(freq, "hash", &val) && val == 2);
	check("table == 1", ht_get(freq, "table", &val) && val == 1);
	check("python (нет) == 0", !ht_contains(freq, "python"));

	ht_destroy(freq);

	// множество - уникальные посетители
	printf("\nМножество — учёт уникальных посетителей\n");
	Set* visitors = set_create(32);

	const char* visits[] = {
		"alice", "bob", "alice", "charlie", "bob",
		"diana", "alice", "eve", "charlie", "frank",
		NULL
	};

	printf("Всего визитов: 10\n");
	for (int i = 0; visits[i]; i++) {
		set_add(visitors, visits[i]);
	}

	printf("Уникальные посетители: ");
	set_print(visitors);

	check("уникальных посетителей == 6", set_size(visitors) == 6);
	check("alice учтена", set_contains(visitors, "alice"));
	check("bob учтён", set_contains(visitors, "bob"));
	check("frank учтён", set_contains(visitors, "frank"));
	check("незнакомец не учтён", !set_contains(visitors, "unknown"));

	set_destroy(visitors);

	// Мультимножество - корзина покупок
	printf("\nМультимножество - корзина интернет-магазина\n");
	MultiSet* cart = multiset_create(32);

	printf("Добавляем товары:\n");
	multiset_add(cart, "хлеб", 2);
	printf("+ 2 хлеба\n");
	multiset_add(cart, "молоко", 1);
	printf("+ 1 молоко\n");
	multiset_add(cart, "яблоки", 6);
	printf("+ 6 яблок\n");
	multiset_add(cart, "хлеб", 1);
	printf("+ 1 хлеб (стало 3)\n");

	printf("Корзина: ");
	multiset_print(cart);

	check("хлеб == 3", multiset_count(cart, "хлеб") == 3);
	check("молоко == 1", multiset_count(cart, "молоко") == 1);
	check("яблоки == 6", multiset_count(cart, "яблоки") == 6);
	check("сыр == 0 (нет в корзине)", multiset_count(cart, "сыр") == 0);

	printf("\nУбираем 1 хлеб и 3 яблока:\n");
	multiset_remove(cart, "хлеб", 1);
	multiset_remove(cart, "яблоки", 3);

	printf("Корзина после изменений: ");
	multiset_print(cart);

	check("хлеб == 2", multiset_count(cart, "хлеб") == 2);
	check("яблоки == 3", multiset_count(cart, "яблоки") == 3);

	printf("\nУбираем всё молоко:\n");
	multiset_remove(cart, "молоко", 1);
	check("молоко == 0", multiset_count(cart, "молоко") == 0);
	check("молока нет в корзине", !multiset_contains(cart, "молоко"));

	printf("Итоговая корзина: ");
	multiset_print(cart);

	multiset_destroy(cart);

	// Множество - фильтрация дубликатов
	printf("\nМножество — удаление дубликатов из списка\n");
	Set* unique = set_create(32);

	const char* items[] = { "C", "C", "Python", "Java", "C", "Python", "Go", "Rust", "Go", NULL };
	printf("Исходный список: C, C, Python, Java, C, Python, Go, Rust, Go\n");
	printf("Уникальные: ");

	for (int i = 0; items[i]; i++) {
		if (!set_contains(unique, items[i])) {
			printf("%s ", items[i]);
		}
		set_add(unique, items[i]);
	}
	printf("\n");

	check("уникальных языков == 5", set_size(unique) == 5);
	check("C есть", set_contains(unique, "C"));
	check("Python есть", set_contains(unique, "Python"));
	check("Java есть", set_contains(unique, "Java"));
	check("Go есть", set_contains(unique, "Go"));
	check("Rust есть", set_contains(unique, "Rust"));
	check("JavaScript нет", !set_contains(unique, "JavaScript"));

	set_destroy(unique);
}

int main(void) {
	setlocale(LC_ALL, "Russian");
	printf("Тесты Хэш-Таблицы, Множества, Мультимножества\n\n");

	test_basic_operations();
	test_collisions();
	test_resize();
	test_set();
	test_multiset();
	test_multithread();
	test_lazy_deletion();
	test_set_multithread();
	test_real();

	printf("Итоги:\n");
	printf("Пройдено: %d\n", passed);
	printf("Провалено: %d\n", failed);
	printf("Всего: %d\n", passed + failed);

	if (failed == 0) {
		printf("\nВсе тесты пройдены успешно!\n");
	}
	else {
		printf("\nЕсть проваленные тесты...\n");
	}

	return 0;
}