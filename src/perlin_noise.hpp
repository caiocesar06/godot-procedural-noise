#pragma once

#include "noise_base.hpp"
#include "perlin_core.hpp"

namespace godot {
    class PerlinNoise final : public NoiseBase {
        GDCLASS(PerlinNoise, NoiseBase)

    public:
        // Espelha FadeMode do núcleo. Existe separado porque o ClassDB
        // exige um enum membro da classe registrada para expor constantes
        // ao GDScript via BIND_ENUM_CONSTANT -- sem isso o Inspector
        // mostra o dropdown, mas PerlinNoise.FADE_QUINTIC não existe no
        // script. Os valores são fixados por static_assert no .cpp.
        enum FadeType {
            FADE_NONE = 0,
            FADE_CUBIC,
            FADE_QUINTIC
        };

    private:
        PerlinCore _core;

    protected:
        static void _bind_methods();

    public:
        PerlinNoise() = default;
        ~PerlinNoise() = default;

        void set_seed(int64_t p_seed) override;
        int64_t get_seed() const override;

        void set_fade_mode(int32_t p_mode);
        int32_t get_fade_mode() const;

        double get_noise_2d(double x, double y) const override;
        double get_noise_3d(double x, double y, double z) const override;
    };
}

VARIANT_ENUM_CAST(godot::PerlinNoise::FadeType);
