#include <stdio.h>
#include <windows.h>
#include <chrono>
#include <atomic>

const int N = 100000000;
const int BLOCK = 10 * 431624;

std::atomic<int> nextBlockIndex(0);
double partialSums[64] = { 0 };

DWORD WINAPI ThreadFunc(LPVOID param) {
    int id = (int)(long long)param;
    int totalBlocks = (N + BLOCK - 1) / BLOCK;

    while (true) {
        int b = nextBlockIndex.fetch_add(1);
        if (b >= totalBlocks) break;

        int startIdx = b * BLOCK;
        int endIdx = (startIdx + BLOCK > N) ? N : startIdx + BLOCK;

        double localSum = 0.0;
        for (int i = startIdx; i < endIdx; i++) {
            double x = (i + 0.5) / N;
            localSum += 4.0 / (1.0 + x * x);
        }
        partialSums[id] += localSum;
    }
    return 0;
}

int main() {
    int threads_to_test[] = { 1, 2, 4, 8, 12, 16 };

    for (int t = 0; t < 6; t++) {
        int num_threads = threads_to_test[t];

        // Сброс данных для нового теста
        nextBlockIndex = 0;
        for (int i = 0; i < 64; i++) partialSums[i] = 0.0;

        HANDLE* threads = new HANDLE[num_threads];

        auto start = std::chrono::high_resolution_clock::now();

        // Запуск потоков
        for (int i = 0; i < num_threads; i++) {
            threads[i] = CreateThread(NULL, 0, ThreadFunc, (LPVOID)(long long)i, 0, NULL);
        }

        // Ожидание завершения
        WaitForMultipleObjects(num_threads, threads, TRUE, INFINITE);
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end - start;
        double pi = 0.0;
        for (int i = 0; i < num_threads; i++) {
            pi += partialSums[i];
            CloseHandle(threads[i]);
        }
        pi /= N;
        delete[] threads;

        printf("Threads: %2d | Pi: %.10f | Time: %.4f sec\n", num_threads, pi, elapsed.count());
    }

    return 0;
}