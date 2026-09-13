@tool
extends EditorScript

# Verifica a regra de emissão do sinal `changed` do PerlinNoise.
#
# Os testes em tests/ (C++) não alcançam esta camada: emit_changed depende
# da engine. Por isso a verificação é feita aqui, dentro da Godot.
#
# Como rodar: abra este arquivo no editor de scripts da Godot e use
# Arquivo > Executar (Ctrl+Shift+X). O resultado sai no painel Saída,
# junto com os avisos esperados para entrada inválida.

var _emissoes := 0


func _contar() -> void:
	_emissoes += 1


func _verificar(descricao: String, acao: Callable, esperado: int) -> bool:
	_emissoes = 0
	acao.call()
	var ok := _emissoes == esperado
	print("%s  %s  (esperado %d, obtido %d)" % [
		"ok   " if ok else "FALHA", descricao, esperado, _emissoes
	])
	return ok


func _run() -> void:
	var noise := PerlinNoise.new()
	noise.changed.connect(_contar)

	print("\n--- emissao do sinal changed ---")
	print("valores padrao: octaves 4, persistence 0.5, lacunarity 2.0,")
	print("fractal FBM, fade Quintic, seed 1337\n")

	var casos := [
		["octaves 4 -> 5", func(): noise.octaves = 5, 1],
		["octaves 5 de novo", func(): noise.octaves = 5, 0],
		["octaves 99 (vira 16, deve avisar)", func(): noise.octaves = 99, 1],
		["octaves 99 de novo (deve avisar)", func(): noise.octaves = 99, 0],
		["persistence com o valor atual", func(): noise.persistence = 0.5, 0],
		["persistence 0.5 -> 0.6", func(): noise.persistence = 0.6, 1],
		["lacunarity com o valor atual", func(): noise.lacunarity = 2.0, 0],
		["fractal_type com o valor atual", func(): noise.fractal_type = PerlinNoise.FRACTAL_FBM, 0],
		["fractal_type FBM -> Ridged", func(): noise.fractal_type = PerlinNoise.FRACTAL_RIDGED, 1],
		["fade_mode com o valor atual", func(): noise.fade_mode = PerlinNoise.FADE_QUINTIC, 0],
		["fade_mode Quintic -> Cubic", func(): noise.fade_mode = PerlinNoise.FADE_CUBIC, 1],
		["seed com o valor atual", func(): noise.seed = noise.seed, 0],
		["seed 1337 -> 42", func(): noise.seed = 42, 1],
		["randomize_seed() emite uma vez so", func(): noise.randomize_seed(), 1],
	]

	var falhas := 0
	for caso in casos:
		if not _verificar(caso[0], caso[1], caso[2]):
			falhas += 1

	print("\n%d de %d casos passaram" % [casos.size() - falhas, casos.size()])
