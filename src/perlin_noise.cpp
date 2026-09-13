#include "perlin_noise.hpp"
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

    static_assert(
        static_cast<int>(PerlinNoise::FADE_NONE)
            == static_cast<int>(FadeMode::NONE),
        "FadeType e FadeMode divergiram"
    );
    static_assert(
        static_cast<int>(PerlinNoise::FADE_CUBIC)
            == static_cast<int>(FadeMode::CUBIC),
        "FadeType e FadeMode divergiram"
    );
    static_assert(
        static_cast<int>(PerlinNoise::FADE_QUINTIC)
            == static_cast<int>(FadeMode::QUINTIC),
        "FadeType e FadeMode divergiram"
    );

    void PerlinNoise::_bind_methods() {
        // set_seed/get_seed são registrados em NoiseBase: a semente é
        // contrato de todo ruído, não especificidade do Perlin.

        ClassDB::bind_method(
            D_METHOD("set_fade_mode", "mode"),
            &PerlinNoise::set_fade_mode
        );
        ClassDB::bind_method(
            D_METHOD("get_fade_mode"),
            &PerlinNoise::get_fade_mode
        );

        ClassDB::bind_method(
            D_METHOD("get_noise_2d", "x", "y"),
            &PerlinNoise::get_noise_2d
        );
        ClassDB::bind_method(
            D_METHOD("get_noise_3d", "x", "y", "z"),
            &PerlinNoise::get_noise_3d
        );

        BIND_ENUM_CONSTANT(FADE_NONE);
        BIND_ENUM_CONSTANT(FADE_CUBIC);
        BIND_ENUM_CONSTANT(FADE_QUINTIC);

        ADD_PROPERTY(
            PropertyInfo(Variant::INT, "fade_mode", PROPERTY_HINT_ENUM, "None,Cubic,Quintic"),
            "set_fade_mode",
            "get_fade_mode"
        );
    }

    void PerlinNoise::set_seed(int64_t p_seed) {
        _core.set_seed(p_seed);
    }
    int64_t PerlinNoise::get_seed() const { return _core.get_seed(); }

    void PerlinNoise::set_fade_mode(int32_t p_mode) {
        if (p_mode < 0 || p_mode > 2) {
            WARN_PRINT("PerlinNoise: 'fade_mode' invalido. Usando Quintic por padrao.");
            p_mode = FADE_QUINTIC;
        }
        _core.set_fade_mode(static_cast<FadeMode>(p_mode));
    }
    int32_t PerlinNoise::get_fade_mode() const { return static_cast<int32_t>(_core.get_fade_mode()); }

    double PerlinNoise::get_noise_2d(double x, double y) const {
        return _core.calculate_2d(x, y);
    }

    double PerlinNoise::get_noise_3d(double x, double y, double z) const {
        return _core.calculate_3d(x, y, z);
    }
}
