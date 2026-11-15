#pragma once

#include <cstdint>
#include <cmath>

#include "permutation_table.hpp"

namespace godot {

    enum class FadeMode { NONE, CUBIC, QUINTIC };

    // Núcleo matemático puro do ruído Perlin.
    // Pode ser testado isoladamente (unit tests) sem GDExtension.
    class PerlinCore {
    private:
        PermutationTable _hash;
        FadeMode _fade_mode = FadeMode::QUINTIC;

    public:
        PerlinCore() = default;

        explicit PerlinCore(int64_t p_seed) : _hash(p_seed) {}

        void set_seed(int64_t p_seed) { _hash.reseed(p_seed); }
        int64_t get_seed() const { return _hash.get_seed(); }

        void set_fade_mode(FadeMode mode) { _fade_mode = mode; }
        FadeMode get_fade_mode() const { return _fade_mode; }


        // --- Funções Matemáticas Utilitárias --- //

        inline double fade(double t) const {
            if (_fade_mode == FadeMode::NONE)
                return t;
            if (_fade_mode == FadeMode::CUBIC)
                return t * t * (3.0 - 2.0 * t);

            return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
        }

        static inline double lerp(double t, double a, double b) {
            return a + t * (b - a);
        }

        inline int64_t hash(int64_t x) const {
            return _hash.hash(x);
        }


        // --- Gradientes --- //

        static constexpr double SUPREMUM_2D = 0.70710678118654752;
        static constexpr double SUPREMUM_3D = 1.22474487139158905;

        static inline double grad_2d(int64_t hash, double x, double y) {
            static constexpr double G2D[8][2] = {
                { 1.0, 0.0 }, {-1.0, 0.0 },
                { 0.0, 1.0 }, { 0.0, -1.0 },
                { SUPREMUM_2D, SUPREMUM_2D }, {-SUPREMUM_2D, SUPREMUM_2D },     // vetores diagonais
                { SUPREMUM_2D, -SUPREMUM_2D }, {-SUPREMUM_2D, -SUPREMUM_2D }    // de normas iguais a um
            };
            const int64_t h = hash & 7;
            return G2D[h][0] * x + G2D[h][1] * y;
        }

        static inline double grad_3d(int64_t hash, double x, double y, double z) {
            static constexpr double G3D[16][3] = {
                { 1.0,  1.0,  0.0}, { 1.0,  1.0,  0.0}, // duplica
                {-1.0,  1.0,  0.0}, {-1.0,  1.0,  0.0}, // duplica
                { 0.0, -1.0,  1.0}, { 0.0, -1.0,  1.0}, // duplica
                { 0.0, -1.0, -1.0}, { 0.0, -1.0, -1.0}, // duplica
                { 1.0, -1.0,  0.0}, {-1.0, -1.0,  0.0},
                { 1.0,  0.0,  1.0}, {-1.0,  0.0,  1.0},
                { 1.0,  0.0, -1.0}, {-1.0,  0.0, -1.0},
                { 0.0,  1.0,  1.0}, { 0.0,  1.0, -1.0}
            };
            const int64_t h = hash & 15;
            return G3D[h][0] * x + G3D[h][1] * y + G3D[h][2] * z;
        }

        // ---------------------------------

        double calculate_2d(double x, double y) const {
            const double x_floor = std::floor(x);
            const double y_floor = std::floor(y);

            const double xf = x - x_floor;
            const double yf = y - y_floor;

            const double u = fade(xf);
            const double v = fade(yf);

            const int64_t i = static_cast<int64_t>(x_floor) & 255;
            const int64_t j = static_cast<int64_t>(y_floor) & 255;

            const int64_t aa = hash(hash(i) + j);
            const int64_t ab = hash(hash(i) + j + 1);
            const int64_t ba = hash(hash(i + 1) + j);
            const int64_t bb = hash(hash(i + 1) + j + 1);

            const double x1 = lerp(
                u,
                grad_2d(aa, xf      , yf      ),
                grad_2d(ba, xf - 1.0, yf      )
            );

            const double x2 = lerp(
                u,
                grad_2d(ab, xf      , yf - 1.0),
                grad_2d(bb, xf - 1.0, yf - 1.0)
            );

            return lerp(v, x1, x2);
        }

        double calculate_3d(double x, double y, double z) const {
            const double x_floor = std::floor(x);
            const double y_floor = std::floor(y);
            const double z_floor = std::floor(z);

            const double xf = x - x_floor;
            const double yf = y - y_floor;
            const double zf = z - z_floor;

            const double u = fade(xf);
            const double v = fade(yf);
            const double w = fade(zf);

            const int64_t i = static_cast<int64_t>(x_floor) & 255;
            const int64_t j = static_cast<int64_t>(y_floor) & 255;
            const int64_t k = static_cast<int64_t>(z_floor) & 255;

            const int64_t a = hash(i) + j;
            const int64_t aa = hash(a) + k;
            const int64_t ab = hash(a + 1) + k;
            const int64_t b = hash(i + 1) + j;
            const int64_t ba = hash(b) + k;
            const int64_t bb = hash(b + 1) + k;

            const double x1 = lerp(
                u,
                grad_3d(hash(aa    ), xf      , yf      , zf      ),
                grad_3d(hash(ba    ), xf - 1.0, yf      , zf      )
            );
            const double x2 = lerp(
                u,
                grad_3d(hash(ab    ), xf      , yf - 1.0, zf      ),
                grad_3d(hash(bb    ), xf - 1.0, yf - 1.0, zf      )
            );
            const double y1 = lerp(v, x1, x2);

            const double x3 = lerp(
                u,
                grad_3d(hash(aa + 1), xf      , yf      , zf - 1.0),
                grad_3d(hash(ba + 1), xf - 1.0, yf      , zf - 1.0)
            );
            const double x4 = lerp(
                u,
                grad_3d(hash(ab + 1), xf      , yf - 1.0, zf - 1.0),
                grad_3d(hash(bb + 1), xf - 1.0, yf - 1.0, zf - 1.0)
            );
            const double y2 = lerp(v, x3, x4);

            return lerp(w, y1, y2);
        }
    };
}
