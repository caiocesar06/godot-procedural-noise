// Testes do núcleo matemático, sem Godot.
//
// PerlinCore, PermutationTable e a camada fractal foram mantidos livres
// de godot-cpp exatamente para permitir isto: exercitar a matemática sem
// subir a engine. Este binário não linka godot-cpp.
//
// Compilar e rodar:
//   cmake -B build-tests -G Ninja -DPERLIN_BUILD_EXTENSION=OFF -DPERLIN_BUILD_TESTS=ON
//   cmake --build build-tests && ctest --test-dir build-tests --output-on-failure

#include "fractal.hpp"
#include "perlin_core.hpp"
#include "permutation_table.hpp"

#include <array>
#include <cmath>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>

using godot::calculate_fractal;
using godot::FadeMode;
using godot::FractalKind;
using godot::PerlinCore;
using godot::PermutationTable;

// --- Harness mínimo ------------------------------------------------- //

static int g_checks = 0;
static int g_failures = 0;
static std::string g_section;

static void section(const std::string& name) {
    g_section = name;
    std::cout << "\n[" << name << "]\n";
}

static void check(bool ok, const std::string& what, int line) {
    ++g_checks;
    if (ok) {
        std::cout << "  ok    " << what << "\n";
    } else {
        ++g_failures;
        std::cout << "  FALHA " << what
                  << "   (test_core.cpp:" << line << ")\n";
    }
}

static void check_near(
    double a, double b, double eps, const std::string& what, int line
) {
    const bool ok = std::fabs(a - b) <= eps;
    ++g_checks;
    if (ok) {
        std::cout << "  ok    " << what << "\n";
    } else {
        ++g_failures;
        std::cout << "  FALHA " << what << "   esperado " << b
                  << ", obtido " << a
                  << "   (test_core.cpp:" << line << ")\n";
    }
}

#define CHECK(cond, what)          check((cond), (what), __LINE__)
#define CHECK_NEAR(a, b, eps, what) check_near((a), (b), (eps), (what), __LINE__)

// --- Estatística descritiva ----------------------------------------- //

struct Stats {
    double min = 0.0;
    double max = 0.0;
    double mean = 0.0;
    double stddev = 0.0;
};

static Stats summarize(const std::vector<double>& v) {
    Stats s;
    s.min = v[0];
    s.max = v[0];
    double sum = 0.0;
    for (double x : v) {
        if (x < s.min) s.min = x;
        if (x > s.max) s.max = x;
        sum += x;
    }
    s.mean = sum / static_cast<double>(v.size());

    double acc = 0.0;
    for (double x : v) acc += (x - s.mean) * (x - s.mean);
    s.stddev = std::sqrt(acc / static_cast<double>(v.size()));
    return s;
}

// Consequencia pratica do range: quanto da escala [0,255] o
// (val + 1) * 127,5 de get_fbm_image_data realmente ocupa.
static void report_gray_usage(const std::string& label, const Stats& s) {
    const double low = (s.min + 1.0) * 127.5;
    const double high = (s.max + 1.0) * 127.5;
    std::cout << "  " << std::left << std::setw(28) << label
              << std::right << std::fixed << std::setprecision(0)
              << " ocupa [" << std::setw(3) << low
              << ", " << std::setw(3) << high << "] de [0, 255] = "
              << std::setw(3) << (100.0 * (high - low) / 255.0) << "%\n";
}

static void report_stats(const std::string& label, const Stats& s) {
    std::cout << "  " << std::left << std::setw(28) << label
              << std::right << std::fixed << std::setprecision(5)
              << " min " << std::setw(9) << s.min
              << "  max " << std::setw(9) << s.max
              << "  media " << std::setw(9) << s.mean
              << "  desvio " << std::setw(9) << s.stddev << "\n";
}

// --- Testes ---------------------------------------------------------- //

static void test_permutation_table() {
    section("Tabela de permutacao");

    PermutationTable table(42);

    std::array<int, 256> seen{};
    for (int i = 0; i < 256; ++i) {
        const int v = table.hash(i);
        if (v >= 0 && v < 256) ++seen[static_cast<size_t>(v)];
    }
    bool is_permutation = true;
    for (int count : seen) {
        if (count != 1) is_permutation = false;
    }
    CHECK(is_permutation, "os 256 primeiros valores sao uma permutacao de 0..255");

    bool mirrored = true;
    for (int i = 0; i < 256; ++i) {
        if (table.hash(i) != table.hash(i + 256)) mirrored = false;
    }
    CHECK(mirrored, "a segunda metade duplica a primeira");

    // O default deixou de usar std::random_device: PerlinNoise.new() tem
    // de ser reprodutivel entre execucoes.
    PermutationTable a;
    PermutationTable b;
    CHECK(
        a.get_seed() == PermutationTable::DEFAULT_SEED,
        "construtor padrao usa DEFAULT_SEED"
    );
    bool same = a.get_seed() == b.get_seed();
    for (int i = 0; i < 512; ++i) {
        if (a.hash(i) != b.hash(i)) same = false;
    }
    CHECK(same, "duas instancias padrao sao identicas (determinismo)");

    PermutationTable c(1);
    PermutationTable d(2);
    bool differ = false;
    for (int i = 0; i < 512; ++i) {
        if (c.hash(i) != d.hash(i)) differ = true;
    }
    CHECK(differ, "sementes diferentes produzem tabelas diferentes");
}

static void test_fade() {
    section("Funcoes de suavizacao");

    const FadeMode modes[] = { FadeMode::NONE, FadeMode::CUBIC, FadeMode::QUINTIC };
    const char* names[] = { "linear", "cubica", "quintica" };

    for (int m = 0; m < 3; ++m) {
        PerlinCore core(42);
        core.set_fade_mode(modes[m]);

        CHECK_NEAR(core.fade(0.0f), 0.0, 1e-6,
            std::string("fade ") + names[m] + ": f(0) = 0");
        CHECK_NEAR(core.fade(1.0f), 1.0, 1e-6,
            std::string("fade ") + names[m] + ": f(1) = 1");
        CHECK_NEAR(core.fade(0.5f), 0.5, 1e-6,
            std::string("fade ") + names[m] + ": f(1/2) = 1/2");

        bool monotonic = true;
        float previous = core.fade(0.0f);
        for (int i = 1; i <= 200; ++i) {
            const float t = static_cast<float>(i) / 200.0f;
            const float current = core.fade(t);
            if (current < previous - 1e-6f) monotonic = false;
            previous = current;
        }
        CHECK(monotonic, std::string("fade ") + names[m] + ": monotona em [0,1]");
    }
}

static void test_lattice_zeros() {
    section("Ruido nos pontos da grade");

    PerlinCore core(42);

    bool all_zero_2d = true;
    for (int i = -8; i <= 8; ++i) {
        for (int j = -8; j <= 8; ++j) {
            if (std::fabs(core.calculate_2d(i, j)) > 1e-6) all_zero_2d = false;
        }
    }
    CHECK(all_zero_2d, "2D vale exatamente 0 nos vertices inteiros da grade");

    bool all_zero_3d = true;
    for (int i = -4; i <= 4; ++i) {
        for (int j = -4; j <= 4; ++j) {
            for (int k = -4; k <= 4; ++k) {
                if (std::fabs(core.calculate_3d(i, j, k)) > 1e-6) all_zero_3d = false;
            }
        }
    }
    CHECK(all_zero_3d, "3D vale exatamente 0 nos vertices inteiros da grade");
}

static void test_continuity() {
    section("Continuidade nas bordas de celula");

    PerlinCore core(42);

    double worst_2d = 0.0;
    for (int cell = -5; cell <= 5; ++cell) {
        for (int s = 0; s < 16; ++s) {
            const double y = 0.13 + s * 0.37;
            const double left = core.calculate_2d(cell - 1e-5, y);
            const double right = core.calculate_2d(cell + 1e-5, y);
            worst_2d = std::max(worst_2d, std::fabs(left - right));
        }
    }
    CHECK(worst_2d < 1e-3,
        "2D nao salta ao cruzar a fronteira de celula (max "
            + std::to_string(worst_2d) + ")");

    double worst_3d = 0.0;
    for (int cell = -3; cell <= 3; ++cell) {
        for (int s = 0; s < 8; ++s) {
            const double y = 0.21 + s * 0.43;
            const double z = 0.07 + s * 0.29;
            const double a = core.calculate_3d(cell - 1e-5, y, z);
            const double b = core.calculate_3d(cell + 1e-5, y, z);
            worst_3d = std::max(worst_3d, std::fabs(a - b));
        }
    }
    CHECK(worst_3d < 1e-3,
        "3D nao salta ao cruzar a fronteira de celula (max "
            + std::to_string(worst_3d) + ")");
}

static void test_range_and_distribution() {
    section("Range medido contra o supremo teorico");

    PerlinCore core(42);
    std::mt19937_64 engine(12345);
    std::uniform_real_distribution<double> coord(-500.0, 500.0);

    constexpr int SAMPLES = 400000;
    std::vector<double> values_2d;
    std::vector<double> values_3d;
    values_2d.reserve(SAMPLES);
    values_3d.reserve(SAMPLES);

    for (int i = 0; i < SAMPLES; ++i) {
        values_2d.push_back(core.calculate_2d(coord(engine), coord(engine)));
        values_3d.push_back(
            core.calculate_3d(coord(engine), coord(engine), coord(engine))
        );
    }

    const Stats s2 = summarize(values_2d);
    const Stats s3 = summarize(values_3d);

    const double eps = 1e-4;
    CHECK(
        std::max(std::fabs(s2.min), std::fabs(s2.max))
            <= PerlinCore::SUPREMUM_2D + eps,
        "2D respeita o supremo sqrt(2)/2 ~ 0,70711"
    );
    CHECK(
        std::max(std::fabs(s3.min), std::fabs(s3.max))
            <= PerlinCore::SUPREMUM_3D + eps,
        "3D respeita o supremo sqrt(6)/2 ~ 1,22474"
    );

    // sqrt(N)/2 e um limite superior FROUXO: sai do produto escalar
    // maximo, ignorando que os pesos de interpolacao nao podem estar
    // todos no maximo ao mesmo tempo. Em 2D a cota e praticamente
    // justa (0,7047 medido contra 0,7071); em 3D esta longe disso
    // (0,986 medido contra 1,2247). Medido, nao presumido.
    CHECK(
        std::fabs(s2.max) > 0.69,
        "2D chega perto de sqrt(2)/2: a cota e justa"
    );
    CHECK(
        std::max(std::fabs(s3.min), std::fabs(s3.max)) > 0.90
            && std::max(std::fabs(s3.min), std::fabs(s3.max)) < 1.0,
        "3D para em ~0,99 e nao alcanca sqrt(6)/2: a cota e frouxa"
    );

    std::cout << "\n  -- dados para o relatorio (" << SAMPLES
              << " amostras, semente 42) --\n";
    report_stats("2D, 1 oitava", s2);
    report_stats("3D, 1 oitava", s3);
    report_gray_usage("2D, 1 oitava", s2);
    report_gray_usage("3D, 1 oitava", s3);

    // Concentracao por TLC: quanto mais oitavas, mais estreita a
    // distribuicao em torno de zero.
    for (int octaves : { 1, 2, 4, 6, 16 }) {
        std::vector<double> fbm;
        fbm.reserve(60000);
        std::mt19937_64 local(999);
        std::uniform_real_distribution<double> c(-500.0, 500.0);
        for (int i = 0; i < 60000; ++i) {
            const double x = c(local);
            const double y = c(local);
            fbm.push_back(calculate_fractal(
                FractalKind::FBM, octaves, 0.5, 2.0,
                [&](double freq) { return core.calculate_2d(x * freq, y * freq); }
            ));
        }
        const Stats fs = summarize(fbm);
        report_stats(
            "fBm 2D, " + std::to_string(octaves) + " oitava(s)", fs
        );
        report_gray_usage(
            "fBm 2D, " + std::to_string(octaves) + " oitava(s)", fs
        );
    }
}

static void test_gradient_bias() {
    section("Vies do hash % 12 no gradiente 3D");

    // O hash devolve [0,255] e 256 nao e multiplo de 12: os quatro
    // primeiros gradientes saem 22 vezes e os outros oito, 21.
    std::array<int, 12> buckets{};
    for (int h = 0; h < 256; ++h) ++buckets[static_cast<size_t>(h % 12)];

    int max_count = buckets[0];
    int min_count = buckets[0];
    for (int c : buckets) {
        max_count = std::max(max_count, c);
        min_count = std::min(min_count, c);
    }

    CHECK(max_count == 22 && min_count == 21,
        "o vies e exatamente 22 contra 21 ocorrencias");

    const double excess =
        100.0 * (max_count - min_count) / static_cast<double>(min_count);
    std::cout << "  info  desequilibrio de "
              << std::fixed << std::setprecision(2) << excess
              << "% entre o gradiente mais e o menos frequente\n";
    std::cout << "  info  Perlin (2002) usa h & 15 com a tabela estendida"
              << " a 16 duplicando 4 arestas\n";
}

static void test_fractal_layer() {
    section("Camada fractal");

    PerlinCore core(42);
    const double x = 3.7;
    const double y = -12.3;

    // Tolerancia de 1e-6, nao de 1e-12.
    //
    // A API publica troca double, mas PerlinCore::calculate_2d calcula
    // em float internamente -- o epsilon do float e ~1,2e-7, entao
    // qualquer tolerancia abaixo disso cobra do teste uma precisao que o
    // tipo nao tem. Sob -ffast-math a diferenca aparece de fato, porque o
    // compilador contrai multiply-add de formas diferentes em cada ponto
    // de inlining. Essa e a fronteira interna double/float que o cap4 do
    // relatorio cita como politica ainda nao documentada; aqui ela fica
    // medida.
    const double single = calculate_fractal(
        FractalKind::FBM, 1, 0.5, 2.0,
        [&](double freq) { return core.calculate_2d(x * freq, y * freq); }
    );
    CHECK_NEAR(single, core.calculate_2d(x, y), 1e-6,
        "1 oitava em fBm devolve o ruido de base inalterado");

    const double zero_octaves = calculate_fractal(
        FractalKind::FBM, 0, 0.5, 2.0,
        [&](double) { return 1.0; }
    );
    CHECK_NEAR(zero_octaves, 0.0, 1e-12,
        "0 oitavas devolve 0, sem divisao por zero");

    const double zero_persistence = calculate_fractal(
        FractalKind::FBM, 8, 0.0, 2.0,
        [&](double freq) { return core.calculate_2d(x * freq, y * freq); }
    );
    CHECK_NEAR(zero_persistence, core.calculate_2d(x, y), 1e-6,
        "persistencia 0 colapsa para a primeira oitava, sem NaN");

    CHECK_NEAR(godot::apply_fractal_shape(FractalKind::RIDGED, 0.0), 1.0, 1e-12,
        "Ridged leva 0 ao topo da crista");
    CHECK_NEAR(godot::apply_fractal_shape(FractalKind::BILLOW, 0.0), -1.0, 1e-12,
        "Billow leva 0 ao fundo");
    CHECK_NEAR(godot::apply_fractal_shape(FractalKind::FBM, 0.31), 0.31, 1e-12,
        "fBm nao altera a amostra");
}

static void test_determinism() {
    section("Determinismo do ruido");

    PerlinCore a(2026);
    PerlinCore b(2026);

    bool identical = true;
    for (int i = 0; i < 5000; ++i) {
        const double x = i * 0.017;
        const double y = i * 0.031;
        if (a.calculate_2d(x, y) != b.calculate_2d(x, y)) identical = false;
        if (a.calculate_3d(x, y, x - y) != b.calculate_3d(x, y, x - y)) identical = false;
    }
    CHECK(identical, "mesma semente produz exatamente os mesmos valores");

    PerlinCore c(2027);
    bool differs = false;
    for (int i = 0; i < 5000; ++i) {
        const double x = i * 0.017;
        const double y = i * 0.031;
        if (a.calculate_2d(x, y) != c.calculate_2d(x, y)) differs = true;
    }
    CHECK(differs, "sementes diferentes produzem campos diferentes");

    // fade_mode altera o resultado -- houve um bug historico em que
    // fade() ignorava o modo e sempre usava a quintica.
    PerlinCore linear(42);
    linear.set_fade_mode(FadeMode::NONE);
    PerlinCore quintic(42);
    quintic.set_fade_mode(FadeMode::QUINTIC);
    CHECK(
        linear.calculate_2d(0.37, 0.62) != quintic.calculate_2d(0.37, 0.62),
        "trocar fade_mode muda o valor calculado"
    );
}

int main() {
    std::cout << "Testes do nucleo matematico (sem godot-cpp)\n";

    test_permutation_table();
    test_fade();
    test_lattice_zeros();
    test_continuity();
    test_determinism();
    test_fractal_layer();
    test_gradient_bias();
    test_range_and_distribution();

    std::cout << "\n----------------------------------------------\n";
    std::cout << g_checks - g_failures << " de " << g_checks
              << " verificacoes passaram\n";

    if (g_failures > 0) {
        std::cout << g_failures << " FALHA(S)\n";
        return 1;
    }
    std::cout << "tudo certo\n";
    return 0;
}
