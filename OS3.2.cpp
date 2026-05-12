#include <stdio.h>
#include <omp.h>
#include <chrono>

int main() {
    const int N = 100000000;
    const int CHUNK_SIZE = 10 * 431624;
    int threads_to_test[] = { 1, 2, 4, 8, 12, 16 };

    for (int t = 0; t < 6; t++) {
        int num_threads = threads_to_test[t];
        double pi = 0.0;

        // Устанавливаем количество потоков
        omp_set_num_threads(num_threads);

        auto start = std::chrono::high_resolution_clock::now();
#pragma omp parallel for reduction(+:pi) schedule(dynamic, CHUNK_SIZE)
        for (int i = 0; i < N; i++) {
            double x = (i + 0.5) / N;
            pi += 4.0 / (1.0 + x * x);
        }

        pi /= N;

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end - start;

        printf("Threads: %2d | Pi: %.10f | Time: %.4f sec\n", num_threads, pi, elapsed.count());
    }

    return 0;
}