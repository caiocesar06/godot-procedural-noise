# Godot Procedural Noise GDExtension

[![Build Status](https://img.shields.io/github/actions/workflow/status/caiocesar06/godot-perlin-noise/build_addon.yml?branch=master&style=flat-square)](https://github.com/caiocesar06/godot-perlin-noise/actions)

Biblioteca de geração procedural para Godot 4 desenvolvida em C++ com GDExtension.

Atualmente, o projeto inclui:

- Perlin Noise
- Simplex Noise

O objetivo é evoluir para uma suíte de múltiplos algoritmos de ruído, com foco em alta performance para terrenos, texturas e simulações.

Este repositório é fruto do projeto de Iniciação Científica (PIC1422-2025) do CEFET-MG e também serve como base para outros projetos de geração procedural.

---

## 📦 Instalação do Addon (Usuários)

Se você quer apenas usar o addon no seu jogo (sem compilar C++), siga por Actions.

### Requisitos para uso

- Godot Engine 4.x

Downloads oficiais:

- Godot: https://godotengine.org/download

### Passo a passo (via GitHub Releases)

1. Acesse a página de Releases do projeto: [Actions](../../actions)
2. Abra a versão estável mais recente.
3. Na seção Assets, baixe o arquivo .zip correspondente ao seu sistema operacional (por exemplo: Windows x86_64 ou Linux x86_64).
4. Extraia o .zip e localize a pasta addons/.
5. Copie a pasta addons/ inteira para a raiz do seu projeto Godot (res://).
6. No Godot, abra Project > Project Settings > Plugins e habilite o plugin, caso ele apareça desativado.

Com isso, as classes do addon ficam disponíveis no GDScript.

### Exemplo de uso (GDScript)

```gdscript
func _ready():
    var noise = PerlinNoise.new()

    noise.set_octaves(6)
    noise.set_persistence(0.5)
    noise.set_lacunarity(2.0)

    var elevation = noise.get_noise_2D(10.5, 20.1)
    print("Elevation: ", elevation)
```

---

## 🛠️ Desenvolvimento e Compilação (Contribuidores)

Use esta seção se você quer alterar o código C++ ou adicionar novos algoritmos de ruído.

### Requisitos para build

- Godot Engine 4.x (para testar o projeto)
- CMake 3.16 ou superior
- Ninja (recomendado)
- Python 3.x (necessário para build do submódulo godot-cpp)
- Compilador C++ com suporte a OpenMP

Downloads oficiais:

- CMake: https://cmake.org/download/
- Ninja: https://ninja-build.org/
- Python: https://www.python.org/downloads/
- MinGW-w64 (Windows/GCC): https://www.mingw-w64.org/

### 1. Clone com submódulos

```bash
git clone --recursive https://github.com/caiocesar06/godot-perlin-noise.git
cd godot-perlin-noise
```

Se você já clonou sem --recursive:

```bash
git submodule update --init --recursive
```

### 2. Configure e compile

```bash
cmake -B build -G "Ninja"
cmake --build build --config Release
```

Os binários são posicionados na estrutura do addon, em project/addons/perlin_noise/bin/.

### 3. Rode no Godot

Abra a pasta project/ deste repositório no Godot para testar cenas e ferramentas.

---

## 🏛️ Créditos e Licença

- Pesquisador Principal: Caio César Nascimento Silva
- Orientação: Prof. Luis Alberto D'Afonseca
- Instituição: CEFET-MG - Departamento de Matemática (NG)

Desenvolvido no escopo do Edital DPPG No 94/2025 - PIBIC FAPEMIG.
