#pragma once

#include <cmath>

// Camada de composição fractal, agnóstica de engine.
//
// Vive fora de NoiseBase de propósito: a composição de oitavas é
// matemática pura e precisa ser exercitável pelos testes e pelo
// benchmark sem carregar o godot-cpp. NoiseBase apenas delega para cá.
//
// Ver perlin_core.hpp para a mesma decisão aplicada ao ruído de base.

namespace godot {

    // Espelha NoiseBase::FractalType. A duplicação é a fronteira entre o
    // núcleo STL e o enum registrado no ClassDB; os valores são fixados
    // por static_assert em noise_base.cpp.
    enum class FractalKind { FBM = 0, RIDGED = 1, BILLOW = 2 };

    inline double apply_fractal_shape(FractalKind kind, double v) {
        switch (kind) {
        case FractalKind::RIDGED:
            return 1.0 - (2.0 * std::abs(v));
        case FractalKind::BILLOW:
            return (2.0 * std::abs(v)) - 1.0;
        case FractalKind::FBM:
        default:
            return v;
        }
    }

    // evaluate_noise(frequency) devolve o ruído de UMA oitava já escalado
    // pela frequência. O somatório é normalizado pela soma das amplitudes,
    // então o resultado herda o range do ruído de base -- que NÃO é
    // necessariamente [-1, 1]. Ver nota em perlin_core.hpp sobre o supremo
    // sqrt(N)/2 e a concentração por TLC.
    template <typename F>
    double calculate_fractal(
        FractalKind kind,
        int octaves,
        double persistence,
        double lacunarity,
        F evaluate_noise
    ) {
        double total = 0.0;
        double amplitude = 1.0;
        double frequency = 1.0;
        double max_value = 0.0;

        for (int i = 0; i < octaves; ++i) {
            total += apply_fractal_shape(kind, evaluate_noise(frequency))
                * amplitude;

            max_value += amplitude;
            amplitude *= persistence;
            frequency *= lacunarity;
        }

        return max_value > 0.0 ? total / max_value : 0.0;
    }
}
