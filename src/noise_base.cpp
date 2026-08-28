#include "noise_base.hpp"

#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

namespace godot {

    // A fronteira entre o enum registrado no ClassDB e o enum STL do
    // núcleo só é segura enquanto os valores coincidirem.
    static_assert(
        static_cast<int>(NoiseBase::FRACTAL_FBM)
            == static_cast<int>(FractalKind::FBM),
        "FractalType e FractalKind divergiram"
    );
    static_assert(
        static_cast<int>(NoiseBase::FRACTAL_RIDGED)
            == static_cast<int>(FractalKind::RIDGED),
        "FractalType e FractalKind divergiram"
    );
    static_assert(
        static_cast<int>(NoiseBase::FRACTAL_BILLOW)
            == static_cast<int>(FractalKind::BILLOW),
        "FractalType e FractalKind divergiram"
    );

    constexpr int64_t MAX_BUFFER_BYTES = 512LL * 1024LL * 1024LL;

    NoiseBase::~NoiseBase() = default;

    void NoiseBase::_bind_methods() {
        ClassDB::bind_method(
            D_METHOD("set_octaves", "octaves"),
            &NoiseBase::set_octaves
        );
        ClassDB::bind_method(
            D_METHOD("get_octaves"),
            &NoiseBase::get_octaves
        );
        ClassDB::bind_method(
            D_METHOD("set_persistence", "persistence"),
            &NoiseBase::set_persistence
        );
        ClassDB::bind_method(
            D_METHOD("get_persistence"),
            &NoiseBase::get_persistence
        );
        ClassDB::bind_method(
            D_METHOD("set_lacunarity", "lacunarity"),
            &NoiseBase::set_lacunarity
        );
        ClassDB::bind_method(
            D_METHOD("get_lacunarity"),
            &NoiseBase::get_lacunarity
        );
        ClassDB::bind_method(
            D_METHOD("set_fractal_type", "type"),
            &NoiseBase::set_fractal_type
        );
        ClassDB::bind_method(
            D_METHOD("get_fractal_type"),
            &NoiseBase::get_fractal_type
        );

        // Semente vive na base: todo algoritmo de ruído precisa de uma, e
        // registrar aqui evita que cada subclasse futura duplique isso.
        ClassDB::bind_method(
            D_METHOD("set_seed", "seed"),
            &NoiseBase::set_seed
        );
        ClassDB::bind_method(
            D_METHOD("get_seed"),
            &NoiseBase::get_seed
        );
        ClassDB::bind_method(
            D_METHOD("randomize_seed"),
            &NoiseBase::randomize_seed
        );

        ClassDB::bind_method(
            D_METHOD("get_fractal_noise_2d", "x", "y"),
            &NoiseBase::get_fractal_noise_2d
        );
        ClassDB::bind_method(
            D_METHOD("get_fractal_noise_3d", "x", "y", "z"),
            &NoiseBase::get_fractal_noise_3d
        );
        ClassDB::bind_method(
            D_METHOD(
                "get_fbm_image_data",
                "width",
                "height",
                "scale",
                "offset_x",
                "offset_y"
            ),
            &NoiseBase::get_fbm_image_data
        );
        ClassDB::bind_method(
            D_METHOD(
                "get_fbm_volume_data",
                "width",
                "height",
                "depth",
                "scale",
                "offset_x",
                "offset_y",
                "offset_z"
            ),
            &NoiseBase::get_fbm_volume_data
        );
        ClassDB::bind_method(
            D_METHOD(
                "get_heightmap_data",
                "width",
                "height",
                "scale",
                "offset_x",
                "offset_y"
            ),
            &NoiseBase::get_heightmap_data
        );
        ClassDB::bind_method(
            D_METHOD(
                "get_terrain_mesh_arrays",
                "grid_size",
                "scale",
                "amplitude",
                "offset_x",
                "offset_y"
            ),
            &NoiseBase::get_terrain_mesh_arrays
        );
        ClassDB::bind_method(
            D_METHOD(
                "get_solid_voxels",
                "width",
                "height",
                "depth",
                "scale",
                "threshold",
                "offset_x",
                "offset_y",
                "offset_z"
            ),
            &NoiseBase::get_solid_voxels
        );
        ClassDB::bind_method(
            D_METHOD(
                "get_solid_voxel_transforms",
                "width",
                "height",
                "depth",
                "scale",
                "threshold",
                "offset_x",
                "offset_y",
                "offset_z"
            ),
            &NoiseBase::get_solid_voxel_transforms
        );

        BIND_ENUM_CONSTANT(FRACTAL_FBM);
        BIND_ENUM_CONSTANT(FRACTAL_RIDGED);
        BIND_ENUM_CONSTANT(FRACTAL_BILLOW);

        ADD_PROPERTY(
            PropertyInfo(Variant::INT, "seed"),
            "set_seed",
            "get_seed"
        );
        ADD_PROPERTY(
            PropertyInfo(Variant::INT, "octaves", PROPERTY_HINT_RANGE, "1,16,1"),
            "set_octaves",
            "get_octaves"
        );
        ADD_PROPERTY(
            PropertyInfo(Variant::FLOAT, "persistence", PROPERTY_HINT_RANGE, "0.0,1.0,0.05"),
            "set_persistence",
            "get_persistence"
        );
        ADD_PROPERTY(
            PropertyInfo(Variant::FLOAT, "lacunarity", PROPERTY_HINT_RANGE, "1.0,4.0,0.1"),
            "set_lacunarity",
            "get_lacunarity"
        );
        ADD_PROPERTY(
            PropertyInfo(Variant::INT, "fractal_type", PROPERTY_HINT_ENUM, "FBM,Ridged,Billow"),
            "set_fractal_type",
            "get_fractal_type"
        );
    }

    void NoiseBase::set_octaves(int32_t p_octaves) {
        if (p_octaves < 1 || p_octaves > 16)
            WARN_PRINT("NoiseBase: 'octaves' deve estar entre 1 e 16. Usando o limite mais proximo.");
        _octaves = std::clamp(p_octaves, 1, 16);
    }
    int32_t NoiseBase::get_octaves() const { return _octaves; }

    void NoiseBase::set_persistence(double p_persistence) {
        if (p_persistence < 0.0 || p_persistence > 1.0)
            WARN_PRINT("NoiseBase: 'persistence' deve estar entre 0.0 e 1.0. Usando o limite mais proximo.");
        _persistence = std::clamp(p_persistence, 0.0, 1.0);
    }
    double NoiseBase::get_persistence() const { return _persistence; }

    void NoiseBase::set_lacunarity(double p_lacunarity) {
        if (p_lacunarity < 1.0 || p_lacunarity > 4.0)
            WARN_PRINT("NoiseBase: 'lacunarity' deve estar entre 1.0 e 4.0. Usando o limite mais proximo.");
        _lacunarity = std::clamp(p_lacunarity, 1.0, 4.0);
    }
    double NoiseBase::get_lacunarity() const { return _lacunarity; }

    void NoiseBase::set_fractal_type(int32_t p_type) {
        if (p_type < 0 || p_type > 2) {
            WARN_PRINT("NoiseBase: 'fractal_type' invalido. Usando FBM por padrao.");
            p_type = 0;
        }
        _fractal_type = static_cast<FractalType>(p_type);
    }
    int32_t NoiseBase::get_fractal_type() const { return static_cast<int32_t>(_fractal_type); }

    int64_t NoiseBase::randomize_seed() {
        std::random_device device;
        const int64_t new_seed = static_cast<int64_t>(
            (static_cast<uint64_t>(device()) << 32) | device()
        );
        set_seed(new_seed);
        return new_seed;
    }

    double NoiseBase::get_fractal_noise_2d(double x, double y) const {
        return calculate_fractal(
            static_cast<FractalKind>(_fractal_type),
            _octaves,
            _persistence,
            _lacunarity,
            [this, x, y](double freq) {
                return get_noise_2d(x * freq, y * freq);
            }
        );
    }

    double NoiseBase::get_fractal_noise_3d(double x, double y, double z) const {
        return calculate_fractal(
            static_cast<FractalKind>(_fractal_type),
            _octaves,
            _persistence,
            _lacunarity,
            [this, x, y, z](double freq) {
                return get_noise_3d(x * freq, y * freq, z * freq);
            }
        );
    }

    PackedByteArray NoiseBase::get_fbm_image_data(
        int64_t width, int64_t height, double scale,
        double offset_x, double offset_y
    ) const {
        ERR_FAIL_COND_V_MSG(
            width <= 0 || height <= 0,
            PackedByteArray(),
            "Width e Height devem ser > 0."
        );
        ERR_FAIL_COND_V_MSG(
            width > (MAX_BUFFER_BYTES / 4) / height,
            PackedByteArray(),
            "Dimensoes excedem 512MB de RAM."
        );

        PackedByteArray buffer;
        buffer.resize(width * height * 4);
        uint8_t* ptr = buffer.ptrw();

#ifdef _OPENMP
#pragma omp parallel for
#endif
        for (int64_t y = 0; y < height; ++y) {
            for (int64_t x = 0; x < width; ++x) {
                double val = get_fractal_noise_2d(
                    (x + offset_x) * scale,
                    (y + offset_y) * scale
                );
                int64_t color = std::clamp(
                    static_cast<int64_t>((val + 1.0) * 127.5),
                    int64_t(0),
                    int64_t(255)
                );
                int64_t idx = (y * width + x) * 4;
                ptr[idx + 0] = ptr[idx + 1] = ptr[idx + 2] = static_cast<uint8_t>(color);
                ptr[idx + 3] = 255;
            }
        }
        return buffer;
    }

    PackedByteArray NoiseBase::get_fbm_volume_data(
        int64_t width, int64_t height, int64_t depth, double scale,
        double offset_x, double offset_y, double offset_z
    ) const {
        ERR_FAIL_COND_V_MSG(
            width <= 0 || height <= 0 || depth <= 0,
            PackedByteArray(),
            "Dimensoes devem ser > 0."
        );
        ERR_FAIL_COND_V_MSG(
            width > (MAX_BUFFER_BYTES / height) / depth,
            PackedByteArray(),
            "Volume 3D excede 512MB de RAM."
        );

        PackedByteArray buffer;
        buffer.resize(width * height * depth);
        uint8_t* ptr = buffer.ptrw();

#ifdef _OPENMP
#pragma omp parallel for collapse(2)
#endif
        for (int64_t z = 0; z < depth; ++z) {
            for (int64_t y = 0; y < height; ++y) {
                for (int64_t x = 0; x < width; ++x) {
                    double val = get_fractal_noise_3d(
                        (x + offset_x) * scale,
                        (y + offset_y) * scale,
                        (z + offset_z) * scale
                    );
                    int64_t density = std::clamp(
                        static_cast<int64_t>((val + 1.0) * 127.5),
                        int64_t(0),
                        int64_t(255)
                    );
                    int64_t idx = z * (width * height) + y * width + x;
                    ptr[idx] = static_cast<uint8_t>(density);
                }
            }
        }
        return buffer;
    }

    PackedFloat32Array NoiseBase::get_heightmap_data(
        int64_t width, int64_t height, double scale,
        double offset_x, double offset_y
    ) const {
        ERR_FAIL_COND_V_MSG(
            width <= 0 || height <= 0,
            PackedFloat32Array(),
            "Width e Height devem ser > 0."
        );
        ERR_FAIL_COND_V_MSG(
            width > (MAX_BUFFER_BYTES / 4) / height,
            PackedFloat32Array(),
            "Dimensoes excedem 512MB de RAM."
        );

        PackedFloat32Array buffer;
        buffer.resize(width * height);
        float* ptr = buffer.ptrw();

#ifdef _OPENMP
#pragma omp parallel for
#endif
        for (int64_t y = 0; y < height; ++y) {
            for (int64_t x = 0; x < width; ++x) {
                ptr[y * width + x] = static_cast<float>(
                    get_fractal_noise_2d(
                        (x + offset_x) * scale,
                        (y + offset_y) * scale
                    )
                );
            }
        }
        return buffer;
    }

    Array NoiseBase::get_terrain_mesh_arrays(
        int64_t grid_size, double scale, double amplitude,
        double offset_x, double offset_y
    ) const {
        ERR_FAIL_COND_V_MSG(
            grid_size < 2,
            Array(),
            "grid_size deve ser >= 2."
        );
        ERR_FAIL_COND_V_MSG(
            grid_size > 4096,
            Array(),
            "grid_size acima de 4096 excede o orcamento de memoria."
        );

        const int64_t side = grid_size;
        const int64_t vertex_count = side * side;
        const int64_t quad_count = (side - 1) * (side - 1);

        std::vector<double> heights(static_cast<size_t>(vertex_count));

#ifdef _OPENMP
#pragma omp parallel for
#endif
        for (int64_t z = 0; z < side; ++z) {
            for (int64_t x = 0; x < side; ++x) {
                heights[static_cast<size_t>(z * side + x)] =
                    get_fractal_noise_2d(
                        (x + offset_x) * scale,
                        (z + offset_y) * scale
                    ) * amplitude;
            }
        }

        PackedVector3Array vertices;
        PackedVector3Array normals;
        PackedVector2Array uvs;
        vertices.resize(vertex_count);
        normals.resize(vertex_count);
        uvs.resize(vertex_count);

        Vector3* vertex_ptr = vertices.ptrw();
        Vector3* normal_ptr = normals.ptrw();
        Vector2* uv_ptr = uvs.ptrw();

        const double inv_side = 1.0 / static_cast<double>(side);

        // Normal analítica do campo de altura por diferenças centrais.
        // Nas bordas o índice é fixado no limite, degenerando para
        // diferença lateral.
#ifdef _OPENMP
#pragma omp parallel for
#endif
        for (int64_t z = 0; z < side; ++z) {
            for (int64_t x = 0; x < side; ++x) {
                const size_t i = static_cast<size_t>(z * side + x);
                const double h = heights[i];

                const int64_t xl = std::max<int64_t>(x - 1, 0);
                const int64_t xr = std::min<int64_t>(x + 1, side - 1);
                const int64_t zu = std::max<int64_t>(z - 1, 0);
                const int64_t zd = std::min<int64_t>(z + 1, side - 1);

                const double dhdx =
                    (heights[static_cast<size_t>(z * side + xr)]
                        - heights[static_cast<size_t>(z * side + xl)])
                    / static_cast<double>(xr - xl);
                const double dhdz =
                    (heights[static_cast<size_t>(zd * side + x)]
                        - heights[static_cast<size_t>(zu * side + x)])
                    / static_cast<double>(zd - zu);

                const Vector3 n(
                    static_cast<float>(-dhdx),
                    1.0f,
                    static_cast<float>(-dhdz)
                );

                vertex_ptr[i] = Vector3(
                    static_cast<float>(x),
                    static_cast<float>(h),
                    static_cast<float>(z)
                );
                normal_ptr[i] = n.normalized();
                uv_ptr[i] = Vector2(
                    static_cast<float>(x * inv_side),
                    static_cast<float>(z * inv_side)
                );
            }
        }

        PackedInt32Array indices;
        indices.resize(quad_count * 6);
        int32_t* index_ptr = indices.ptrw();

        for (int64_t z = 0; z < side - 1; ++z) {
            for (int64_t x = 0; x < side - 1; ++x) {
                const int32_t i = static_cast<int32_t>(x + z * side);
                const int64_t base = (z * (side - 1) + x) * 6;

                index_ptr[base + 0] = i;
                index_ptr[base + 1] = i + 1;
                index_ptr[base + 2] = i + static_cast<int32_t>(side);
                index_ptr[base + 3] = i + 1;
                index_ptr[base + 4] = i + static_cast<int32_t>(side) + 1;
                index_ptr[base + 5] = i + static_cast<int32_t>(side);
            }
        }

        Array surface;
        surface.resize(Mesh::ARRAY_MAX);
        surface[Mesh::ARRAY_VERTEX] = vertices;
        surface[Mesh::ARRAY_NORMAL] = normals;
        surface[Mesh::ARRAY_TEX_UV] = uvs;
        surface[Mesh::ARRAY_INDEX] = indices;
        return surface;
    }

    PackedVector3Array NoiseBase::get_solid_voxels(
        int64_t width, int64_t height, int64_t depth, double scale,
        double threshold, double offset_x, double offset_y, double offset_z
    ) const {
        ERR_FAIL_COND_V_MSG(
            width <= 0 || height <= 0 || depth <= 0,
            PackedVector3Array(),
            "Dimensoes devem ser > 0."
        );
        ERR_FAIL_COND_V_MSG(
            width > (MAX_BUFFER_BYTES / 4 / height) / depth,
            PackedVector3Array(),
            "Volume 3D excede 512MB de RAM."
        );

        const int64_t total = width * height * depth;
        std::vector<float> density(static_cast<size_t>(total));

        // A avaliação do ruído é o custo real e vai em paralelo; a coleta
        // dos sólidos é uma varredura linear serial, barata em comparação
        // e livre da condição de corrida de um append compartilhado.
#ifdef _OPENMP
#pragma omp parallel for collapse(2)
#endif
        for (int64_t z = 0; z < depth; ++z) {
            for (int64_t y = 0; y < height; ++y) {
                for (int64_t x = 0; x < width; ++x) {
                    density[static_cast<size_t>(
                        z * (width * height) + y * width + x
                    )] = static_cast<float>(
                        get_fractal_noise_3d(
                            (x + offset_x) * scale,
                            (y + offset_y) * scale,
                            (z + offset_z) * scale
                        )
                    );
                }
            }
        }

        const float limit = static_cast<float>(threshold);

        int64_t solid_count = 0;
        for (int64_t i = 0; i < total; ++i) {
            if (density[static_cast<size_t>(i)] > limit) ++solid_count;
        }

        PackedVector3Array positions;
        positions.resize(solid_count);
        Vector3* position_ptr = positions.ptrw();

        int64_t written = 0;
        for (int64_t z = 0; z < depth; ++z) {
            for (int64_t y = 0; y < height; ++y) {
                for (int64_t x = 0; x < width; ++x) {
                    const size_t i = static_cast<size_t>(
                        z * (width * height) + y * width + x
                    );
                    if (density[i] > limit) {
                        position_ptr[written++] = Vector3(
                            static_cast<float>(x),
                            static_cast<float>(y),
                            static_cast<float>(z)
                        );
                    }
                }
            }
        }

        return positions;
    }

    PackedFloat32Array NoiseBase::get_solid_voxel_transforms(
        int64_t width, int64_t height, int64_t depth, double scale,
        double threshold, double offset_x, double offset_y, double offset_z
    ) const {
        const PackedVector3Array positions = get_solid_voxels(
            width, height, depth, scale, threshold,
            offset_x, offset_y, offset_z
        );

        const int64_t count = positions.size();

        // MultiMesh em TRANSFORM_3D consome 12 floats por instância: a
        // matriz 3x4 em ordem de linha, cada linha terminando na
        // componente correspondente da origem. Base identidade, portanto
        // só a translação varia.
        PackedFloat32Array buffer;
        buffer.resize(count * 12);
        float* ptr = buffer.ptrw();

        const Vector3* source = positions.ptr();

        for (int64_t i = 0; i < count; ++i) {
            float* row = ptr + i * 12;
            const Vector3& p = source[i];

            row[0] = 1.0f; row[1] = 0.0f; row[2]  = 0.0f; row[3]  = p.x;
            row[4] = 0.0f; row[5] = 1.0f; row[6]  = 0.0f; row[7]  = p.y;
            row[8] = 0.0f; row[9] = 0.0f; row[10] = 1.0f; row[11] = p.z;
        }

        return buffer;
    }
}
