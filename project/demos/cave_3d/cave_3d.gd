extends Node3D

# Caverna 3D: aplicação que justifica os métodos em lote de voxels.
#   get_solid_voxel_transforms -> a rocha, desenhada num MultiMesh: um único
#     draw call para ~180 mil cubos;
#   get_solid_voxels           -> os tesouros. Um SEGUNDO ruído, com outra
#     semente, dá as posições do minério -- a lógica precisa de posições, não
#     de matrizes de renderização ;
#   get_fractal_noise_3d       -> consulta pontual: confere se cada candidato
#     está no chão de uma caverna. São de dezenas a poucos milhares de
#     consultas (medido); gerar outro lote de 262 mil voxels para isso seria
#     desperdício.

const GRID_SIZE := 64

# Mesma origem da demo 2D, em unidades de ruído: longe da coerência junto à
# origem e longe do estouro do int no fast_floor do núcleo.
const VOLUME_ORIGIN := 10.0

const ROCK_SEED := 42
const PERSISTENCE := 0.5
const LACUNARITY := 2.0

# Desvio de UMA oitava de ruído 3D: 0.2751 (400 mil amostras, semente 42).
# A fórmula sigma_1 * sqrt(soma a^2) / soma a vale em 3D com erro de -2.5%
# (4 oitavas, 20 sementes, medido em 13/09/2026). A média é ~0.
const SIGMA_1_3D := 0.2751

# Minério: semente derivada da rocha (XOR, para nunca estourar o int64 como
# uma soma poderia) e o dobro da frequência, para veios menores.
const ORE_SEED_SALT := 1000
const ORE_OCTAVES := 3
const ORE_SCALE_FACTOR := 2.0
const TREASURE_SIZE := 0.4

# Espera sem nova mudança antes de regenerar (debounce). Gerar 64^3 custa de
# 8 a 17 ms (4 a 8 oitavas, medido), mais o envio de ~9 MB de transformações
# para a GPU: regenerar a cada passo do slider travaria a interface.
const REGENERATE_DELAY := 0.15

const ROCK_SHADER := preload("res://demos/cave_3d/rock.gdshader")
const TREASURE_SHADER := preload("res://demos/cave_3d/treasure.gdshader")

@onready var rock: MultiMeshInstance3D = $%Rock
@onready var treasures: MultiMeshInstance3D = $%Treasures

@onready var threshold_slider: HSlider = $%ThresholdSlider
@onready var scale_slider: HSlider = $%ScaleSlider
@onready var octaves_slider: HSlider = $%OctavesSlider
@onready var rarity_slider: HSlider = $%RaritySlider
@onready var cut_slider: HSlider = $%CutSlider
@onready var new_seed_button: Button = $%NewSeedButton

@onready var threshold_label: Label = $%ThresholdLabel
@onready var scale_label: Label = $%ScaleLabel
@onready var octaves_label: Label = $%OctavesLabel
@onready var rarity_label: Label = $%RarityLabel
@onready var cut_label: Label = $%CutLabel
@onready var stats_label: Label = $%StatsLabel

var rock_noise: PerlinNoise
var ore_noise: PerlinNoise
var _rock_material: ShaderMaterial
var _treasure_material: ShaderMaterial
var _regenerate_timer: Timer


func _ready() -> void:
	rock_noise = PerlinNoise.new()
	rock_noise.seed = ROCK_SEED
	rock_noise.persistence = PERSISTENCE
	rock_noise.lacunarity = LACUNARITY
	rock_noise.octaves = int(octaves_slider.value)

	ore_noise = PerlinNoise.new()
	ore_noise.seed = ROCK_SEED ^ ORE_SEED_SALT
	ore_noise.persistence = PERSISTENCE
	ore_noise.lacunarity = LACUNARITY
	ore_noise.octaves = ORE_OCTAVES

	_setup_multimeshes()

	_regenerate_timer = Timer.new()
	_regenerate_timer.one_shot = true
	_regenerate_timer.wait_time = REGENERATE_DELAY
	_regenerate_timer.timeout.connect(_regenerate)
	add_child(_regenerate_timer)

	# Conectar só depois dos valores iniciais, pelo mesmo motivo do mapa 2D.
	rock_noise.changed.connect(_schedule_regenerate)

	octaves_slider.value_changed.connect(func(value: float) -> void: rock_noise.octaves = int(value))

	# O teclado é da câmera. Um botão clicado fica com o foco, e o Espaço
	# (parte de ui_accept) o aciona: subir com a câmera geraria outra caverna.
	# Sem foco, o botão continua funcionando com o mouse.
	new_seed_button.focus_mode = Control.FOCUS_NONE
	new_seed_button.pressed.connect(_on_new_seed)

	# Limiar, escala e raridade não são propriedades do ruído.
	for slider: HSlider in [threshold_slider, scale_slider, rarity_slider]:
		slider.value_changed.connect(_schedule_regenerate.unbind(1))

	# O corte é só um uniform: não regenera nada.
	cut_slider.value_changed.connect(_on_cut_changed)

	for slider: HSlider in [threshold_slider, scale_slider, octaves_slider,
			rarity_slider, cut_slider]:
		slider.value_changed.connect(_update_labels.unbind(1))

	_regenerate()
	_on_cut_changed(cut_slider.value)
	_update_labels()


func _setup_multimeshes() -> void:
	_rock_material = ShaderMaterial.new()
	_rock_material.shader = ROCK_SHADER
	_rock_material.set_shader_parameter("volume_height", float(GRID_SIZE))
	rock.multimesh = _create_multimesh(Vector3.ONE, _rock_material)

	_treasure_material = ShaderMaterial.new()
	_treasure_material.shader = TREASURE_SHADER
	treasures.multimesh = _create_multimesh(Vector3.ONE * TREASURE_SIZE, _treasure_material)


func _create_multimesh(cube_size: Vector3, material: Material) -> MultiMesh:
	var mesh := BoxMesh.new()
	mesh.size = cube_size
	mesh.material = material

	var multimesh := MultiMesh.new()
	# TRANSFORM_3D sem cores nem dados extras: 12 floats por instância, o
	# layout que get_solid_voxel_transforms devolve.
	multimesh.transform_format = MultiMesh.TRANSFORM_3D
	multimesh.mesh = mesh
	# Caixa envolvente fixa do volume inteiro, para o frustum culling nunca
	# esconder os cubos por uma AABB calculada errada.
	multimesh.custom_aabb = AABB(Vector3.ONE * -1.0, Vector3.ONE * (GRID_SIZE + 2))
	return multimesh


func _schedule_regenerate() -> void:
	# start() num timer que já está correndo recomeça a contagem: só regenera
	# depois de REGENERATE_DELAY segundos sem nenhuma mudança.
	_regenerate_timer.start()


func _on_new_seed() -> void:
	var new_seed := rock_noise.randomize_seed()
	ore_noise.seed = new_seed ^ ORE_SEED_SALT


func _on_cut_changed(value: float) -> void:
	_rock_material.set_shader_parameter("cut_height", value)
	_treasure_material.set_shader_parameter("cut_height", value)


func _regenerate() -> void:
	# O offset da API é em pixels (voxels) e somado antes da escala: dividir
	# a origem pela escala mantém o volume em VOLUME_ORIGIN unidades de ruído.
	var noise_scale := scale_slider.value
	var offset := VOLUME_ORIGIN / noise_scale
	var rock_threshold := _raw_threshold(threshold_slider.value, rock_noise.octaves)

	var started := Time.get_ticks_usec()
	var rock_buffer: PackedFloat32Array = rock_noise.get_solid_voxel_transforms(
		GRID_SIZE, GRID_SIZE, GRID_SIZE, noise_scale, rock_threshold,
		offset, offset, offset
	)
	var rock_count := rock_buffer.size() / 12
	_fill_multimesh(rock.multimesh, rock_buffer, rock_count)
	var rock_ms := (Time.get_ticks_usec() - started) / 1000.0

	started = Time.get_ticks_usec()
	var placement := _place_treasures(noise_scale, offset, rock_threshold)
	var treasure_ms := (Time.get_ticks_usec() - started) / 1000.0

	var total := GRID_SIZE * GRID_SIZE * GRID_SIZE
	stats_label.text = (
		"Rocha: %.1f%% sólida (%d cubos)\n" % [100.0 * rock_count / total, rock_count]
		+ "Tesouros: %d (de %d candidatos)\n" % [placement.x, placement.y]
		+ "Tempo: %.1f ms rocha + %.1f ms tesouros" % [rock_ms, treasure_ms]
	)


# Converte um limiar em desvios padrão para o valor bruto do ruído.
func _raw_threshold(z: float, octaves: int) -> float:
	var amplitude := 1.0
	var amplitude_sum := 0.0
	var amplitude_square_sum := 0.0
	for _octave in octaves:
		amplitude_sum += amplitude
		amplitude_square_sum += amplitude * amplitude
		amplitude *= PERSISTENCE
	return z * SIGMA_1_3D * sqrt(amplitude_square_sum) / amplitude_sum


# Devolve (tesouros colocados, candidatos avaliados).
func _place_treasures(noise_scale: float, offset: float, rock_threshold: float) -> Vector2i:
	var ore_scale := noise_scale * ORE_SCALE_FACTOR
	var ore_offset := VOLUME_ORIGIN / ore_scale
	var ore_threshold := _raw_threshold(rarity_slider.value, ORE_OCTAVES)

	# Lote: onde o minério é alto, na mesma grade de voxels da rocha.
	var candidates: PackedVector3Array = ore_noise.get_solid_voxels(
		GRID_SIZE, GRID_SIZE, GRID_SIZE, ore_scale, ore_threshold,
		ore_offset, ore_offset, ore_offset
	)

	# Pontual: um tesouro dentro da rocha seria invisível (os cubos o
	# escondem). Fica só o que está no ar e tem rocha logo abaixo -- no chão
	# de uma caverna. Este laço percorre candidatos esparsos, não a grade.
	var buffer := PackedFloat32Array()
	for cell: Vector3 in candidates:
		if cell.y < 1.0:
			continue
		if _is_rock(cell, noise_scale, offset, rock_threshold):
			continue
		if not _is_rock(cell + Vector3.DOWN, noise_scale, offset, rock_threshold):
			continue
		# Apoiado no topo do cubo de baixo, que vai de y - 1.5 a y - 0.5.
		var resting_y := cell.y - 0.5 + TREASURE_SIZE * 0.5
		buffer.append_array(PackedFloat32Array([
			1.0, 0.0, 0.0, cell.x,
			0.0, 1.0, 0.0, resting_y,
			0.0, 0.0, 1.0, cell.z,
		]))

	var count := buffer.size() / 12
	_fill_multimesh(treasures.multimesh, buffer, count)
	return Vector2i(count, candidates.size())


# Mesma expressão que get_solid_voxels usa em C++: (i + offset) * scale.
# Repeti-la na mesma ordem garante o mesmo double -- e a mesma decisão de
# "rocha ou ar" que o lote tomou.
func _is_rock(cell: Vector3, noise_scale: float, offset: float, rock_threshold: float) -> bool:
	return rock_noise.get_fractal_noise_3d(
		(cell.x + offset) * noise_scale,
		(cell.y + offset) * noise_scale,
		(cell.z + offset) * noise_scale
	) > rock_threshold


func _fill_multimesh(multimesh: MultiMesh, buffer: PackedFloat32Array, count: int) -> void:
	# instance_count precisa vir antes: mudar a contagem descarta o buffer.
	multimesh.instance_count = count
	if count > 0:
		multimesh.buffer = buffer


func _update_labels() -> void:
	threshold_label.text = "Limiar da rocha: %.2f σ" % threshold_slider.value
	scale_label.text = "Escala: %.3f" % scale_slider.value
	octaves_label.text = "Oitavas: %d" % int(octaves_slider.value)
	rarity_label.text = "Raridade do tesouro: %.2f σ" % rarity_slider.value
	if cut_slider.value >= GRID_SIZE:
		cut_label.text = "Corte transversal: desligado"
	else:
		cut_label.text = "Corte transversal: altura %d" % int(cut_slider.value)
