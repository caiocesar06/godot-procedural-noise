#pragma once

#include <array>
#include <cstdint>
#include <numeric>
#include <random>

namespace godot {

    class PermutationTable {
        public:
            // Semente padrão determinística.
            //
            // Antes daqui o construtor sem argumento chamava
            // std::random_device, o que tornava PerlinNoise.new() não
            // reprodutível: cada execução do jogo gerava um terreno
            // diferente sem que o usuário tivesse pedido isso. As figuras
            // do relatório dependem de semente fixa, e o FastNoiseLite --
            // a referência de comparação da IC -- também adota um valor
            // fixo (1337). Aleatoriedade agora é explícita, via
            // PerlinNoise.randomize_seed().
            static constexpr int64_t DEFAULT_SEED = 1337;

        private:
            int64_t _seed = DEFAULT_SEED;
            std::array<int, 512> _table{};

        public:
            PermutationTable() {
                reseed(DEFAULT_SEED);
            }

            explicit PermutationTable(int64_t p_seed) {
                reseed(p_seed);
            }

            void reseed(int64_t p_seed) {
                _seed = p_seed;

                std::array<int, 256> temp;
                std::iota(temp.begin(), temp.end(), 0);

                std::mt19937_64 engine(
                    static_cast<std::uint64_t>(_seed)
                );

                for (std::size_t i = temp.size() - 1; i > 0; --i) {
                    std::uniform_int_distribution<std::size_t>
                        distribution(0, i);

                    const std::size_t j = distribution(engine);
                    std::swap(temp[i], temp[j]);
                }

                for (std::size_t i = 0; i < temp.size(); ++i) {
                    _table[i] = temp[i];
                    _table[i + 256] = temp[i];
                }
            }

            int64_t get_seed() const { return _seed; }

            inline int hash(int x) const {
                return _table[static_cast<std::size_t>(x) & 511u];
            }
    };
}
