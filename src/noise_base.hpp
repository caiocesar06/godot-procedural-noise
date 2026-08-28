#pragma once

#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>

#include "fractal.hpp"

namespace godot {
    class NoiseBase : public Resource {
        GDCLASS(NoiseBase, Resource)

    public:
        enum FractalType {
            FRACTAL_FBM = 0,
            FRACTAL_RIDGED,
            FRACTAL_BILLOW
        };

    private:
        int32_t _octaves = 4;
        double _persistence = 0.5;
        double _lacunarity = 2.0;
        FractalType _fractal_type = FractalType::FRACTAL_FBM;

    protected:
        static void _bind_methods();

    public:
        NoiseBase() = default;
        virtual ~NoiseBase() = 0;

        void set_octaves(int32_t p_octaves);
        int32_t get_octaves() const;

        void set_persistence(double p_persistence);
        double get_persistence() const;

        void set_lacunarity(double p_lacunarity);
        double get_lacunarity() const;

        void set_fractal_type(int32_t p_type);
        int32_t get_fractal_type() const;

        // -------------------------------------------------- //
        // Contrato das subclasses: fornecer o ruído de UMA oitava.
        // Oitavas, persistência, lacunaridade e tipo de fractal são
        // responsabilidade desta classe -- é o que permite a um novo
        // algoritmo (Simplex, Worley) herdar fBm/Ridged/Billow sem
        // reimplementar nada.

        virtual void set_seed(int64_t p_seed) = 0;
        virtual int64_t get_seed() const = 0;

        virtual double get_noise_2d(
            double x,
            double y
        ) const = 0;

        virtual double get_noise_3d(
            double x,
            double y,
            double z
        ) const = 0;

        // -------------------------------------------------- //

        // Sorteia uma semente nova e a devolve. Aleatoriedade é
        // explícita: o construtor padrão é determinístico.
        int64_t randomize_seed();

        double get_fractal_noise_2d(
            double x,
            double y
        ) const;

        double get_fractal_noise_3d(
            double x,
            double y,
            double z
        ) const;

        // -------------------------------------------------- //
        // Geração em lote. Toda iteração pesada de pixel, vértice ou
        // voxel vive aqui: uma travessia de fronteira GDScript->C++ por
        // chamada, em vez de uma por elemento.

        PackedByteArray get_fbm_image_data(
            int64_t width,
            int64_t height,
            double scale,
            double offset_x,
            double offset_y
        ) const;

        PackedByteArray get_fbm_volume_data(
            int64_t width,
            int64_t height,
            int64_t depth,
            double scale,
            double offset_x,
            double offset_y,
            double offset_z
        ) const;

        // Valores brutos do ruído fractal, sem quantização para byte.
        PackedFloat32Array get_heightmap_data(
            int64_t width,
            int64_t height,
            double scale,
            double offset_x,
            double offset_y
        ) const;

        // Malha de terreno pronta para ArrayMesh.add_surface_from_arrays().
        // Vértices, normais, UVs e índices calculados em C++.
        Array get_terrain_mesh_arrays(
            int64_t grid_size,
            double scale,
            double amplitude,
            double offset_x,
            double offset_y
        ) const;

        // Centros dos voxels cujo ruído supera o limiar. Substitui o laço
        // de grid_size^3 iterações que era feito em GDScript.
        PackedVector3Array get_solid_voxels(
            int64_t width,
            int64_t height,
            int64_t depth,
            double scale,
            double threshold,
            double offset_x,
            double offset_y,
            double offset_z
        ) const;

        // Os mesmos voxels, já no layout de buffer que
        // MultiMesh.set_buffer() espera para TRANSFORM_3D (12 floats por
        // instância, matriz 3x4 em ordem de linha).
        //
        // Existe porque, sem ela, sobra em GDScript um laço sobre os
        // voxels sólidos chamando set_instance_transform() um a um -- o
        // que viola a mesma regra que get_solid_voxels veio corrigir.
        // O preço é esta classe conhecer um formato de buffer do Godot.
        PackedFloat32Array get_solid_voxel_transforms(
            int64_t width,
            int64_t height,
            int64_t depth,
            double scale,
            double threshold,
            double offset_x,
            double offset_y,
            double offset_z
        ) const;
    };
}

VARIANT_ENUM_CAST(godot::NoiseBase::FractalType);
