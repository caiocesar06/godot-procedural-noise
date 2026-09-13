# Godot Procedural Noise GDExtension

[![Build Status](https://img.shields.io/github/actions/workflow/status/caiocesar06/godot-perlin-noise/build_addon.yml?branch=master&style=flat-square)](https://github.com/caiocesar06/godot-perlin-noise/actions)

Biblioteca de geração procedural para Godot 4, escrita em C++17 e integrada
via GDExtension. O núcleo matemático não depende da engine e é testado
isoladamente.

**Estado atual:** apenas Perlin Noise está implementado. A arquitetura já
está preparada para receber outros algoritmos — a classe base `NoiseBase`
concentra oitavas, persistência, lacunaridade e tipo de fractal, de modo que
um novo ruído precisa fornecer somente `get_noise_2d` e `get_noise_3d`.
Simplex Noise é trabalho em andamento em outra frente e **ainda não faz
parte deste repositório**.

Este repositório é fruto do projeto de Iniciação Científica (PIC1422-2025)
do CEFET-MG e também serve como base para outros projetos de geração
procedural.

---

## Instalação (usuários)

Requisito: Godot Engine 4.4 ou superior ([download](https://godotengine.org/download)).

1. Baixe o `.zip` do seu sistema operacional na página de
   [Releases](../../releases). Se a versão que você quer ainda não tiver
   Release publicada, os binários de cada commit ficam em
   [Actions](../../actions), na aba *Artifacts*.
2. Extraia e copie a pasta `addons/` inteira para a raiz do seu projeto
   Godot (`res://`).
3. Feche e reabra o editor.

É só isso: esta é uma **GDExtension**, não um plugin de editor. Ela carrega
sozinha e não aparece em *Project Settings → Plugins*. Se as classes não
aparecerem no autocomplete depois de reabrir o editor, confira se os
arquivos de runtime (`libgcc_s_seh-1.dll`, `libwinpthread-1.dll`,
`libgomp-1.dll` no Windows) vieram junto na pasta `bin/`.

### Exemplo de uso

```gdscript
func _ready() -> void:
    var noise := PerlinNoise.new()

    noise.seed = 42          # determinístico; o padrão é 1337
    noise.octaves = 6
    noise.persistence = 0.5
    noise.lacunarity = 2.0
    noise.fractal_type = PerlinNoise.FRACTAL_FBM

    # Consulta pontual: use get_fractal_noise_2d para ter o efeito das
    # oitavas. get_noise_2d devolve uma única oitava e ignora octaves,
    # persistence e lacunarity.
    var elevacao := noise.get_fractal_noise_2d(10.5, 20.1)
    print("Elevação: ", elevacao)

    # Geração em lote: uma travessia GDScript -> C++ para a imagem toda,
    # paralelizada com OpenMP.
    var buffer := noise.get_fbm_image_data(256, 256, 0.01, 0.0, 0.0)
    var img := Image.create_from_data(256, 256, false, Image.FORMAT_RGBA8, buffer)
    $TextureRect.texture = ImageTexture.create_from_image(img)
```

### Regra de ouro

**Nunca itere pixels ou voxels em GDScript.** Uma tela de 256×256
consultada ponto a ponto são 65.536 travessias de fronteira por quadro; em
C++ é uma. Toda iteração pesada tem uma API em lote correspondente.

### API

Propriedades (todas na base, portanto comuns a qualquer ruído futuro):

| Propriedade | Tipo | Faixa | Padrão |
|---|---|---|---|
| `seed` | int | — | `1337` |
| `octaves` | int | 1–16 | `4` |
| `persistence` | float | 0.0–1.0 | `0.5` |
| `lacunarity` | float | 1.0–4.0 | `2.0` |
| `fractal_type` | enum | `FRACTAL_FBM`, `FRACTAL_RIDGED`, `FRACTAL_BILLOW` | `FRACTAL_FBM` |
| `fade_mode` | enum | `FADE_NONE`, `FADE_CUBIC`, `FADE_QUINTIC` | `FADE_QUINTIC` |

Consulta pontual:

| Método | Devolve |
|---|---|
| `get_noise_2d(x, y)` | uma oitava, sem composição fractal |
| `get_noise_3d(x, y, z)` | uma oitava, sem composição fractal |
| `get_fractal_noise_2d(x, y)` | ruído fractal completo |
| `get_fractal_noise_3d(x, y, z)` | ruído fractal completo |
| `randomize_seed()` | sorteia uma semente nova e a devolve |

Geração em lote (tudo em C++, paralelizado):

| Método | Devolve | Para |
|---|---|---|
| `get_fbm_image_data(w, h, scale, ox, oy)` | `PackedByteArray` RGBA8 | textura pronta para `Image.create_from_data` |
| `get_heightmap_data(w, h, scale, ox, oy)` | `PackedFloat32Array` | valores brutos, sem quantizar para byte |
| `get_terrain_mesh_arrays(grid, scale, amp, ox, oy)` | `Array` | direto em `ArrayMesh.add_surface_from_arrays` |
| `get_fbm_volume_data(w, h, d, scale, ox, oy, oz)` | `PackedByteArray` | densidade 3D |
| `get_solid_voxels(w, h, d, scale, thr, ox, oy, oz)` | `PackedVector3Array` | centros dos voxels acima do limiar |
| `get_solid_voxel_transforms(...)` | `PackedFloat32Array` | direto em `MultiMesh.set_buffer` |

> Nota: `get_fbm_image_data` e `get_fbm_volume_data` respeitam
> `fractal_type`, então o `fbm` no nome é herança histórica — elas geram
> também Ridged e Billow.

### Sobre a faixa de valores

O Perlin gradiente **não** produz valores em toda a faixa `[-1, 1]`, e 2D e
3D não têm o mesmo alcance. Medido sobre 400 mil amostras
(`tests/test_core.cpp`):

| Configuração | Mínimo | Máximo | Ocupação de `[0, 255]` |
|---|---|---|---|
| 2D, 1 oitava | −0,697 | 0,705 | 70% |
| 3D, 1 oitava | −0,956 | 0,986 | 97% |
| fBm 2D, 6 oitavas | −0,434 | 0,432 | 43% |

Como o fBm normaliza pela soma das amplitudes, somar oitavas concentra a
distribuição em torno de zero. Por isso imagens com muitas oitavas saem
acinzentadas: se você precisa de contraste máximo, normalize pelo range
observado em vez de assumir `[-1, 1]`.

---

## Desenvolvimento

### Requisitos

- CMake 3.16+, Ninja, Python 3 (para o submódulo `godot-cpp`)
- Compilador C++17 com OpenMP — GCC via MSYS2/MinGW UCRT64 no Windows

### Compilar

```bash
git clone --recursive https://github.com/caiocesar06/godot-perlin-noise.git
cd godot-perlin-noise
cmake -B build -G Ninja
cmake --build build
```

Os binários vão direto para `project/addons/perlin_noise/bin/`. Depois de
recompilar, **feche e reabra o editor** — o hot-reload de GDExtension não é
confiável.

Se você clonou sem `--recursive`:

```bash
git submodule update --init --recursive
```

### Testes

O núcleo matemático (`perlin_core.hpp`, `permutation_table.hpp`,
`fractal.hpp`) não inclui `godot-cpp`. Isso permite verificá-lo sem a engine
e sem sequer configurar o submódulo:

```bash
cmake -B build-core -G Ninja -DPERLIN_BUILD_EXTENSION=OFF
cmake --build build-core
ctest --test-dir build-core --output-on-failure
```

Os testes cobrem a validade da tabela de permutação, determinismo por
semente, as três funções de suavização, o valor nulo nos vértices da grade,
continuidade nas fronteiras de célula, a camada fractal e a faixa de valores
medida contra a cota teórica.

### Benchmark

```bash
./build-core/perlin_benchmark          # tabela legível
./build-core/perlin_benchmark --csv    # CSV
```

Ele imprime junto a configuração de build usada (compilador, OpenMP,
fast-math), porque medições com e sem `-ffast-math` não são comparáveis
entre si.

### Build de desempenho vs. build de precisão

`-O3` é o padrão em `Release`. O `-ffast-math` é opcional:

```bash
cmake -B build -G Ninja -DPERLIN_ENABLE_FAST_MATH=ON
```

Medido neste projeto, ele **não trouxe ganho de tempo** e custa
reprodutibilidade bit-a-bit: o compilador reassocia o somatório das oitavas
e os resultados deixam de bater exatamente entre pontos de inlining. A
recomendação é mantê-lo desligado, que é o padrão.

### Opções de CMake

| Opção | Padrão | Efeito |
|---|---|---|
| `PERLIN_BUILD_EXTENSION` | `ON` | compila a GDExtension (exige `godot-cpp`) |
| `PERLIN_BUILD_TESTS` | `ON` | compila os testes do núcleo |
| `PERLIN_BUILD_BENCHMARK` | `ON` | compila o harness de benchmark |
| `PERLIN_ENABLE_FAST_MATH` | `OFF` | ativa `-ffast-math` em `Release` |

---

## Créditos e Licença

- Pesquisador Principal: Caio César Nascimento Silva
- Orientação: Prof. Luis Alberto D'Afonseca
- Instituição: CEFET-MG — Departamento de Matemática (NG)

Desenvolvido no escopo do Edital DPPG Nº 94/2025 — PIBIC FAPEMIG.
