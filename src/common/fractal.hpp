#pragma once

#include <algorithm>
#include <cmath>

// Camada de composição fractal, agnóstica de engine.

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

    template <typename F>
    double calculate_fractal(
        FractalKind kind,
        int octaves,
        double persistence,
        double lacunarity,
        double weighted_strength,
        F evaluate_noise
    ) {
        double total = 0.0;
        double amplitude = 1.0;
        double frequency = 1.0;
        double max_value = 0.0;
        const double weight_k = std::clamp(weighted_strength, 0.0, 1.0);

        for (int i = 0; i < octaves; ++i) {
            const double signal = apply_fractal_shape(kind, evaluate_noise(frequency));
            total += signal * amplitude;

            max_value += amplitude;

            const double weight = std::clamp((signal + 1.0) * 0.5, 0.0, 1.0);
            const double factor = 1.0 + weight_k * (weight - 1.0);

            amplitude *= factor * persistence;
            frequency *= lacunarity;
        }

        return max_value > 0.0 ? total / max_value : 0.0;
    }
}
