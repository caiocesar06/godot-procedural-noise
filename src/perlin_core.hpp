#pragma once

#include <algorithm>
#include <array>
#include <cstdint>

#include "permutation_table.hpp"

namespace godot {

    enum class FadeMode { NONE, CUBIC, QUINTIC };

    // Núcleo matemático puro do ruído Perlin. Sem dependência de Godot:
    // pode ser testado isoladamente (unit tests) sem GDExtension.
    //
    // A tabela de permutação é possuída por composição (_hash), não por
    // herança. Ver discussão no chat: nem todo algoritmo de ruído futuro
    // (ex.: Worley) necessariamente usa esse mecanismo, então não faz
    // sentido promovê-lo a uma classe base comum.
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

        static inline int fast_floor(float x) {
            int xi = static_cast<int>(x);
            return x < xi ? xi - 1 : xi;
        }

        inline float fade(float t) const {
            if (_fade_mode == FadeMode::NONE)
                return t;
            if (_fade_mode == FadeMode::CUBIC)
                return t * t * (3.0f - 2.0f * t);

            return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
        }

        static inline float lerp(float t, float a, float b) {
            return a + t * (b - a);
        }

        inline int hash(int x) const {
            return _hash.hash(x);
        }


        // --- Gradientes --- //
        //
        // Cota superior do ruido gradiente de Perlin em N dimensoes com
        // gradientes de norma g: g * sqrt(N)/2. As duas tabelas abaixo
        // nao tem a mesma norma, entao 2D e 3D NAO compartilham o mesmo
        // range:
        //
        //   grad_2d  8 vetores de norma 1        -> cota sqrt(2)/2 ~ 0,707
        //   grad_3d  12 arestas de norma sqrt(2) -> cota sqrt(6)/2 ~ 1,225
        //
        // A cota e FROUXA: sai do produto escalar maximo e ignora que os
        // pesos de interpolacao nao podem estar todos no maximo ao mesmo
        // tempo. Medido sobre 400 mil amostras (tests/test_core.cpp):
        //
        //   2D  max 0,7047  -- a cota e praticamente justa
        //   3D  max 0,9863  -- longe da cota; nao passa de 1, nao satura
        //
        // Consequencia pratica: get_fbm_image_data e get_fbm_volume_data
        // aplicam o mesmo (val + 1.0) * 127.5 nos dois casos, e o 2D ocupa
        // ~70% da escala de cinza contra ~97% do 3D. Pior: o fBm
        // normalizado pela soma das amplitudes concentra a distribuicao
        // por TLC -- com 6 oitavas o 2D cai para 43% da escala. E dai que
        // vem o aspecto lavado das imagens, nao de clipping.

        static constexpr double SUPREMUM_2D = 0.70710678118654752;
        static constexpr double SUPREMUM_3D = 1.22474487139158905;

        static inline float grad_2d(int hash, float x, float y) {
            static constexpr float G2D[8][2] = {
                { 1.0f, 0.0f }, {-1.0f, 0.0f },
                { 0.0f, 1.0f }, { 0.0f, -1.0f },
                { 0.707106f, 0.707106f }, {-0.707106f, 0.707106f },
                { 0.707106f, -0.707106f }, {-0.707106f, -0.707106f }
            };
            int h = hash & 7;
            return G2D[h][0] * x + G2D[h][1] * y;
        }

        static inline float grad_3d(int hash, float x, float y, float z) {
            static constexpr float G3D[12][3] = {
                { 1.0f,  1.0f,  0.0f}, {-1.0f,  1.0f,  0.0f},
                { 1.0f, -1.0f,  0.0f}, {-1.0f, -1.0f,  0.0f},
                { 1.0f,  0.0f,  1.0f}, {-1.0f,  0.0f,  1.0f},
                { 1.0f,  0.0f, -1.0f}, {-1.0f,  0.0f, -1.0f},
                { 0.0f,  1.0f,  1.0f}, { 0.0f, -1.0f,  1.0f},
                { 0.0f,  1.0f, -1.0f}, { 0.0f, -1.0f, -1.0f}
            };
            int h = hash % 12;
            return G3D[h][0] * x + G3D[h][1] * y + G3D[h][2] * z;
        }

        // ---------------------------------

        double calculate_2d(double x, double y) const {
            float fx = static_cast<float>(x);
            float fy = static_cast<float>(y);

            int xi = fast_floor(fx);
            int yi = fast_floor(fy);
            float xf = fx - xi;
            float yf = fy - yi;

            float u = fade(xf);
            float v = fade(yf);

            int i = xi & 255;
            int j = yi & 255;

            int aa = hash(hash(i) + j);
            int ab = hash(hash(i) + j + 1);
            int ba = hash(hash(i + 1) + j);
            int bb = hash(hash(i + 1) + j + 1);

            float x1 = lerp(u, grad_2d(aa, xf, yf), grad_2d(ba, xf - 1.0f, yf));
            float x2 = lerp(u, grad_2d(ab, xf, yf - 1.0f), grad_2d(bb, xf - 1.0f, yf - 1.0f));

            return static_cast<double>(lerp(v, x1, x2));
        }

        double calculate_3d(double x, double y, double z) const {
            float fx = static_cast<float>(x);
            float fy = static_cast<float>(y);
            float fz = static_cast<float>(z);

            int xi = fast_floor(fx);
            int yi = fast_floor(fy);
            int zi = fast_floor(fz);

            float xf = fx - xi;
            float yf = fy - yi;
            float zf = fz - zi;

            float u = fade(xf);
            float v = fade(yf);
            float w = fade(zf);

            int i = xi & 255;
            int j = yi & 255;
            int k = zi & 255;

            int a = hash(i) + j;
            int aa = hash(a) + k;
            int ab = hash(a + 1) + k;
            int b = hash(i + 1) + j;
            int ba = hash(b) + k;
            int bb = hash(b + 1) + k;

            float x1 = lerp(
                u,
                grad_3d(hash(aa), xf, yf, zf),
                grad_3d(hash(ba), xf - 1.0f, yf, zf)
            );
            float x2 = lerp(
                u,
                grad_3d(hash(ab), xf, yf - 1.0f, zf),
                grad_3d(hash(bb), xf - 1.0f, yf - 1.0f, zf)
            );
            float y1 = lerp(v, x1, x2);

            float x3 = lerp(
                u,
                grad_3d(hash(aa + 1), xf, yf, zf - 1.0f),
                grad_3d(hash(ba + 1), xf - 1.0f, yf, zf - 1.0f)
            );
            float x4 = lerp(
                u,
                grad_3d(hash(ab + 1), xf, yf - 1.0f, zf - 1.0f),
                grad_3d(hash(bb + 1), xf - 1.0f, yf - 1.0f, zf - 1.0f)
            );
            float y2 = lerp(v, x3, x4);

            return static_cast<double>(lerp(w, y1, y2));
        }
    };
}
