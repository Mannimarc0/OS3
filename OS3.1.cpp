#include <stdio.h>
#include <windows.h>
#include <chrono>

const int N = 100000000;
const int BLOCK = 10 * 431624;
const int TOTAL_BLOCKS = (N + BLOCK - 1) / BLOCK;
const int MAX_THREADS = 64;

// Данные, разделяемые между главным и рабочими потоками
int currentBlock[MAX_THREADS];          // номер блока для потока i
volatile bool shouldExit[MAX_THREADS];  // флаг завершения
double partialSums[MAX_THREADS];        // частичные суммы
HANDLE hDoneEvent[MAX_THREADS];         // событие "блок обработан"
HANDLE threads[MAX_THREADS];

DWORD WINAPI ThreadFunc(LPVOID param) {
    int id = (int)(long long)param;
    // На первой итерации поток только что разбужен из CREATE_SUSPENDED,
    // currentBlock[id] уже выставлен главным потоком.
    while (true) {
        if (shouldExit[id]) break;

        // 1) Обрабатываем выданный блок
        int b = currentBlock[id];
        int startIdx = b * BLOCK;
        int endIdx = (startIdx + BLOCK > N) ? N : startIdx + BLOCK;
        double localSum = 0.0;
        for (int i = startIdx; i < endIdx; i++) {
            double x = (i + 0.5) / N;
            localSum += 4.0 / (1.0 + x * x);
        }
        partialSums[id] += localSum;

        // 2) Сообщаем главному потоку, что блок готов
        SetEvent(hDoneEvent[id]);

        // 3) Приостанавливаем самих себя — ждём, пока главный
        //    выдаст новый блок и сделает ResumeThread.
        SuspendThread(GetCurrentThread());
    }
    return 0;
}

int main() {
    int threads_to_test[] = { 1, 2, 4, 8, 12, 16 };

    for (int t = 0; t < 6; t++) {
        int num_threads = threads_to_test[t];

        // Сброс состояния
        for (int i = 0; i < num_threads; i++) {
            partialSums[i] = 0.0;
            shouldExit[i] = false;
            currentBlock[i] = -1;
            hDoneEvent[i] = CreateEvent(NULL, FALSE, FALSE, NULL); // auto-reset
        }

        auto start = std::chrono::high_resolution_clock::now();

        // Создаём все потоки СРАЗУ в приостановленном состоянии
        for (int i = 0; i < num_threads; i++) {
            threads[i] = CreateThread(NULL, 0, ThreadFunc,
                                      (LPVOID)(long long)i,
                                      CREATE_SUSPENDED, NULL);
        }

        // Первая раздача блоков + ResumeThread для запуска
        int nextBlock = 0;
        int activeThreads = 0;
        for (int i = 0; i < num_threads; i++) {
            if (nextBlock < TOTAL_BLOCKS) {
                currentBlock[i] = nextBlock++;
                activeThreads++;
                ResumeThread(threads[i]);
            } else {
                // Блоков уже не осталось — гасим лишний поток
                shouldExit[i] = true;
                ResumeThread(threads[i]);
            }
        }

        // Главный цикл диспетчеризации
        while (activeThreads > 0) {
            DWORD r = WaitForMultipleObjects(num_threads, hDoneEvent,
                                             FALSE, INFINITE);
            int i = (int)(r - WAIT_OBJECT_0);

            // Гонка: после SetEvent поток ещё может не успеть вызвать
            // SuspendThread(self). Дождёмся, пока его suspend count >= 1.
            // SuspendThread возвращает предыдущий счётчик: если >= 1,
            // поток уже сам себя приостановил.
            while (true) {
                DWORD prev = SuspendThread(threads[i]);
                if (prev == (DWORD)-1) break; // ошибка
                if (prev >= 1) {
                    // Поток уже самоприостановлен — откатываем нашу лишнюю приостановку
                    ResumeThread(threads[i]);
                    break;
                }
                // prev == 0: поток ещё бежит, отменяем нашу приостановку и уступаем квант
                ResumeThread(threads[i]);
                Sleep(0);
            }

            // Выдаём ему следующий блок или сигнал на выход.
            if (nextBlock < TOTAL_BLOCKS) {
                currentBlock[i] = nextBlock++;
            } else {
                shouldExit[i] = true;
                activeThreads--;
            }
            ResumeThread(threads[i]); // освобождаем поток
        }

        // Ждём, пока все потоки реально выйдут из ThreadFunc
        WaitForMultipleObjects(num_threads, threads, TRUE, INFINITE);

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end - start;

        double pi = 0.0;
        for (int i = 0; i < num_threads; i++) {
            pi += partialSums[i];
            CloseHandle(threads[i]);
            CloseHandle(hDoneEvent[i]);
        }
        pi /= N;

        printf("Threads: %2d | Pi: %.10f | Time: %.4f sec\n",
               num_threads, pi, elapsed.count());
    }
    return 0;
}
