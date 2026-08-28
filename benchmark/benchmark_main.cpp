// Harness de benchmark do núcleo de ruído.
//
// Existe para que os tempos citados no relatório sejam reproduzíveis. O
// número de ~6ms que aparece no capítulo 4 era medição manual, sem
// nenhum código que o gerasse; qualquer valor publicado a partir de
// agora deve sair daqui, com a configuração de build declarada junto.
//
//   cmake -B build-bench -G Ninja -DPERLIN_BUILD_EXTENSION=OFF \
//         -DPERLIN_BUILD_BENCHMARK=ON -DCMAKE_BUILD_TYPE=Release
//   cmake --build build-bench
//   ./build-bench/perlin_benchmark          # tabela legível
//   ./build-bench/perlin_benchmark --csv    # CSV para o relatório
//
// O laço reproduz NoiseBase::get_fbm_image_data sobre um buffer simples:
// a matemática medida é a mesma, sem o custo de godot-cpp no meio.

#include "fractal.hpp"
#include "perlin_core.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#ifdef _OPENMP
#include <omp.h>
#endif

using godot::calculate_fractal;
using godot::FractalKind;
using godot::PerlinCore;

namespace {

constexpr int64_t BENCH_SEED = 42;
constexpr double BENCH_SCALE = 0.01;

struct Case {
    const char* label;
    int64_t width;
    int64_t height;
    int octaves;
};

// Espelha NoiseBase::get_fbm_image_data. O resultado é somado em um
// acumulador devolvido ao chamador para impedir que o compilador elimine
// o laço inteiro como código morto sob -O3.
uint64_t render(
    const PerlinCore& core,
    int64_t width,
    int64_t height,
    int octaves,
    std::vector<uint8_t>& buffer,
    bool parallel
) {
    uint8_t* ptr = buffer.data();

#ifdef _OPENMP
#pragma omp parallel for if (parallel)
#endif
    for (int64_t y = 0; y < height; ++y) {
        for (int64_t x = 0; x < width; ++x) {
            const double value = calculate_fractal(
                FractalKind::FBM, octaves, 0.5, 2.0,
                [&](double freq) {
                    return core.calculate_2d(
                        x * BENCH_SCALE * freq,
                        y * BENCH_SCALE * freq
                    );
                }
            );
            const int64_t gray = std::clamp(
                static_cast<int64_t>((value + 1.0) * 127.5),
                int64_t(0), int64_t(255)
            );
            ptr[y * width + x] = static_cast<uint8_t>(gray);
        }
    }

    (void)parallel;

    uint64_t checksum = 0;
    for (size_t i = 0; i < buffer.size(); i += 997) checksum += ptr[i];
    return checksum;
}

double median(std::vector<double> samples) {
    std::sort(samples.begin(), samples.end());
    const size_t n = samples.size();
    if (n % 2 == 1) return samples[n / 2];
    return 0.5 * (samples[n / 2 - 1] + samples[n / 2]);
}

struct Result {
    double median_ms;
    double best_ms;
};

Result measure(
    const PerlinCore& core, const Case& c, bool parallel, int repeats
) {
    std::vector<uint8_t> buffer(
        static_cast<size_t>(c.width * c.height)
    );

    // Aquecimento: primeira execução paga falta de página e escalonamento
    // inicial das threads.
    volatile uint64_t sink = render(
        core, c.width, c.height, c.octaves, buffer, parallel
    );
    (void)sink;

    std::vector<double> samples;
    samples.reserve(static_cast<size_t>(repeats));

    for (int i = 0; i < repeats; ++i) {
        const auto start = std::chrono::steady_clock::now();
        sink = render(core, c.width, c.height, c.octaves, buffer, parallel);
        const auto stop = std::chrono::steady_clock::now();
        samples.push_back(
            std::chrono::duration<double, std::milli>(stop - start).count()
        );
    }

    Result r;
    r.median_ms = median(samples);
    r.best_ms = *std::min_element(samples.begin(), samples.end());
    return r;
}

void print_environment() {
    std::cout << "Ambiente da medicao\n";
    std::cout << "  compilador   ";
#if defined(__clang__)
    std::cout << "clang " << __clang_major__ << "." << __clang_minor__;
#elif defined(__GNUC__)
    std::cout << "gcc " << __GNUC__ << "." << __GNUC_MINOR__;
#else
    std::cout << "desconhecido";
#endif
    std::cout << "\n";

    std::cout << "  fast-math    ";
#ifdef __FAST_MATH__
    std::cout << "LIGADO (build de desempenho)";
#else
    std::cout << "desligado (build de precisao)";
#endif
    std::cout << "\n";

    std::cout << "  openmp       ";
#ifdef _OPENMP
    std::cout << "sim, " << omp_get_max_threads() << " threads";
#else
    std::cout << "nao (execucao serial)";
#endif
    std::cout << "\n";

    std::cout << "  semente " << BENCH_SEED
              << ", escala " << BENCH_SCALE
              << ", persistencia 0.5, lacunaridade 2.0\n";
}

} // namespace

int main(int argc, char** argv) {
    bool csv = false;
    int repeats = 15;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--csv") == 0) csv = true;
        else if (std::strcmp(argv[i], "--repeats") == 0 && i + 1 < argc) {
            repeats = std::max(3, std::atoi(argv[++i]));
        }
    }

    const PerlinCore core(BENCH_SEED);

    const Case cases[] = {
        { "256x256",  256, 256,  1 },
        { "256x256",  256, 256,  6 },
        { "256x256",  256, 256, 16 },
        { "800x600",  800, 600,  1 },
        { "800x600",  800, 600,  6 },
        { "800x600",  800, 600, 16 },
        { "1024x1024", 1024, 1024, 6 },
    };

    if (csv) {
        std::cout << "resolucao,pixels,oitavas,serial_ms,paralelo_ms,"
                  << "aceleracao,mpixels_por_s\n";
    } else {
        print_environment();
        std::cout << "\nmediana de " << repeats
                  << " execucoes, apos aquecimento\n\n";
        std::cout << "  resolucao   oitavas     serial   paralelo"
                  << "   ganho   Mpixel/s\n";
        std::cout << "  ---------------------------------------"
                  << "----------------------\n";
    }

    for (const Case& c : cases) {
        const Result serial = measure(core, c, false, repeats);
        const Result parallel = measure(core, c, true, repeats);

        const double pixels = static_cast<double>(c.width * c.height);
        const double speedup = serial.median_ms / parallel.median_ms;
        const double mpps = (pixels / 1.0e6) / (parallel.median_ms / 1000.0);

        if (csv) {
            std::cout << c.label << "," << static_cast<int64_t>(pixels) << ","
                      << c.octaves << ","
                      << std::fixed << std::setprecision(3)
                      << serial.median_ms << "," << parallel.median_ms << ","
                      << std::setprecision(2) << speedup << ","
                      << std::setprecision(1) << mpps << "\n";
        } else {
            std::cout << "  " << std::left << std::setw(12) << c.label
                      << std::right << std::setw(5) << c.octaves
                      << std::fixed << std::setprecision(2)
                      << std::setw(12) << serial.median_ms << " ms"
                      << std::setw(8) << parallel.median_ms << " ms"
                      << std::setw(7) << speedup << "x"
                      << std::setprecision(1) << std::setw(10) << mpps
                      << "\n";
        }
    }

    if (!csv) {
        std::cout << "\nO tempo citado no relatorio deve vir desta tabela,"
                  << " acompanhado da\nconfiguracao de build impressa acima"
                  << " -- fast-math ligado e desligado\nsao medicoes"
                  << " diferentes e nao podem ser misturadas.\n";
    }
    return 0;
}
