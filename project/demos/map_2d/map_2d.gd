extends HBoxContainer

# Mapa 2D: aplicação que justifica get_heightmap_data (alturas em float,
# sem quantizar para byte). A cor de cada pixel sai de
# z = (altura - media) / desvio, calculado no shader; este script só gera
# as alturas e envia os parâmetros da normalização.
#
# O toggle de precisão alterna com get_fbm_image_data (8 bits). A diferença
# quase não aparece nas faixas de cor, mas salta aos olhos no relevo
# sombreado: uma derivada amplifica o erro de quantização. Medido em
# 13/09/2026: na escala 0.01, 21% dos pares de vizinhos caem no mesmo byte;
# na escala 0.002, 67%.

const MAP_SIZE := 256

# Origem do mapa em UNIDADES DE RUÍDO. O offset da API é em pixels e é
# somado antes da escala -- por isso a conversão em _regenerate().
# Medido em 13/09/2026 (300 sementes, 6 oitavas):
#   - na origem, com lacunaridade 1.25, o desvio fica 13.6% acima do
#     previsto: as oitavas compartilham o gradiente do vértice (0, 0);
#   - a partir de 10, o erro cai para ~2%;
#   - origens grandes (1000) estouram o int do fast_floor do núcleo com
#     lacunaridade e oitavas moderadas, e o mapa inteiro vira NaN.
const MAP_ORIGIN := 10.0

const INITIAL_SEED := 42

# Média e desvio de UMA oitava por tipo de fractal, na ordem do enum
# FractalType. Medidos sobre 200 mil amostras, semente 42 (13/09/2026).
# Billow é exatamente o negativo de Ridged: 2|v| - 1 = -(1 - 2|v|).
const FRACTAL_STATS := [
	{"mean": 0.0, "std_dev": 0.2209},     # FBM
	{"mean": 0.638, "std_dev": 0.2531},   # Ridged
	{"mean": -0.638, "std_dev": 0.2531},  # Billow
]

@onready var octaves_slider: HSlider = $%OctavesSlider
@onready var persistence_slider: HSlider = $%PersistenceSlider
@onready var lacunarity_slider: HSlider = $%LacunaritySlider
@onready var scale_slider: HSlider = $%ScaleSlider

@onready var fractal_type_option: OptionButton = $%FractalTypeOption
@onready var new_seed_button: Button = $%NewSeedButton

@onready var water_threshold_slider: HSlider = $%WaterThresholdSlider
@onready var sand_threshold_slider: HSlider = $%SandThresholdSlider
@onready var grass_threshold_slider: HSlider = $%GrassThresholdSlider
@onready var rock_threshold_slider: HSlider = $%RockThresholdSlider

@onready var octaves_label: Label = $%OctavesLabel
@onready var persistence_label: Label = $%PersistenceLabel
@onready var lacunarity_label: Label = $%LacunarityLabel
@onready var scale_label: Label = $%ScaleLabel
@onready var water_label: Label = $%WaterLabel
@onready var sand_label: Label = $%SandLabel
@onready var grass_label: Label = $%GrassLabel
@onready var rock_label: Label = $%RockLabel

@onready var normalize_toggle: CheckButton = $%NormalizeToggle
@onready var float_toggle: CheckButton = $%FloatToggle
@onready var shading_toggle: CheckButton = $%ShadingToggle
@onready var map: TextureRect = $%Map

var noise: PerlinNoise
var _scale: float
var _map_texture: ImageTexture
var _material: ShaderMaterial


func _ready() -> void:
	_material = map.material as ShaderMaterial

	noise = PerlinNoise.new()
	noise.seed = INITIAL_SEED
	noise.octaves = int(octaves_slider.value)
	noise.persistence = persistence_slider.value
	noise.lacunarity = lacunarity_slider.value
	noise.fractal_type = fractal_type_option.selected
	_scale = scale_slider.value

	# Conectar só depois de copiar os valores iniciais: cada atribuição
	# acima emite changed, e o mapa seria regenerado cinco vezes à toa.
	noise.changed.connect(_regenerate)

	octaves_slider.value_changed.connect(func(value: float) -> void: noise.octaves = int(value))
	persistence_slider.value_changed.connect(func(value: float) -> void: noise.persistence = value)
	lacunarity_slider.value_changed.connect(func(value: float) -> void: noise.lacunarity = value)
	fractal_type_option.item_selected.connect(func(index: int) -> void: noise.fractal_type = index)
	new_seed_button.pressed.connect(noise.randomize_seed)

	# A escala não é propriedade do ruído: não passa pelo sinal changed.
	scale_slider.value_changed.connect(_on_scale_changed)

	# Limiares, normalização e relevo só mudam uniforms: nada é regenerado.
	for slider: HSlider in [water_threshold_slider, sand_threshold_slider,
			grass_threshold_slider, rock_threshold_slider]:
		slider.value_changed.connect(_update_thresholds.unbind(1))
	normalize_toggle.toggled.connect(_update_normalization.unbind(1))
	shading_toggle.toggled.connect(_on_shading_toggled)

	# Trocar a precisão troca a API e o formato da imagem: esse regenera.
	float_toggle.toggled.connect(_on_float_toggled)

	_regenerate()
	_update_thresholds()
	_on_shading_toggled(shading_toggle.button_pressed)


func _on_scale_changed(value: float) -> void:
	_scale = value
	_regenerate()


func _on_shading_toggled(enabled: bool) -> void:
	_material.set_shader_parameter("shading_enabled", enabled)


func _on_float_toggled(_enabled: bool) -> void:
	# RF e RGBA8 são formatos diferentes, e ImageTexture.update() exige o
	# mesmo formato. Zerar a textura faz _regenerate() criar uma nova.
	_map_texture = null
	_regenerate()


func _regenerate() -> void:
	# A API calcula (x + offset) * scale. Dividir a origem pela escala
	# mantém o canto do mapa em MAP_ORIGIN unidades de ruído, qualquer que
	# seja o zoom.
	var offset := MAP_ORIGIN / _scale
	var use_float := float_toggle.button_pressed

	var image: Image
	if use_float:
		# Alturas brutas, sem quantizar: é o que o relevo sombreado precisa.
		var heights: PackedFloat32Array = noise.get_heightmap_data(
			MAP_SIZE, MAP_SIZE, _scale, offset, offset
		)
		image = Image.create_from_data(
			MAP_SIZE, MAP_SIZE, false, Image.FORMAT_RF, heights.to_byte_array()
		)
	else:
		# 8 bits: byte = trunc((v + 1) * 127.5). O shader decodifica de
		# volta para v (ver decode() em map_2d.gdshader).
		var bytes: PackedByteArray = noise.get_fbm_image_data(
			MAP_SIZE, MAP_SIZE, _scale, offset, offset
		)
		image = Image.create_from_data(
			MAP_SIZE, MAP_SIZE, false, Image.FORMAT_RGBA8, bytes
		)

	# update() reaproveita a textura já alocada na GPU; só funciona com o
	# mesmo tamanho e formato (ver _on_float_toggled).
	if _map_texture == null:
		_map_texture = ImageTexture.create_from_image(image)
		map.texture = _map_texture
	else:
		_map_texture.update(image)

	_material.set_shader_parameter("decode_8bit", not use_float)
	_material.set_shader_parameter("sample_spacing", _scale)
	_update_normalization()
	_update_labels()


func _update_normalization() -> void:
	var stats: Dictionary = FRACTAL_STATS[noise.fractal_type]

	# Laço sobre no máximo 16 oitavas, não sobre pixels.
	var amplitude := 1.0
	var amplitude_sum := 0.0
	var amplitude_square_sum := 0.0
	for _octave in noise.octaves:
		amplitude_sum += amplitude
		amplitude_square_sum += amplitude * amplitude
		amplitude *= noise.persistence

	var std_dev: float = stats["std_dev"]
	if normalize_toggle.button_pressed:
		# sigma_F = sigma_1 * sqrt(soma a^2) / soma a: desvio de uma soma
		# ponderada de oitavas descorrelacionadas, normalizada pela soma das
		# amplitudes. Desligado, fica sigma_1 -- como se a distribuição
		# nunca estreitasse, e a neve some quando as oitavas aumentam.
		std_dev *= sqrt(amplitude_square_sum) / amplitude_sum

	_material.set_shader_parameter("mean", stats["mean"])
	_material.set_shader_parameter("std_dev", std_dev)


func _update_thresholds() -> void:
	_material.set_shader_parameter("thresholds", Vector4(
		water_threshold_slider.value,
		sand_threshold_slider.value,
		grass_threshold_slider.value,
		rock_threshold_slider.value
	))
	water_label.text = "Água: até %.2f σ" % water_threshold_slider.value
	sand_label.text = "Areia: até %.2f σ" % sand_threshold_slider.value
	grass_label.text = "Floresta: até %.2f σ" % grass_threshold_slider.value
	rock_label.text = "Montanha: até %.2f σ (acima: neve)" % rock_threshold_slider.value


func _update_labels() -> void:
	octaves_label.text = "Oitavas: %d" % noise.octaves
	persistence_label.text = "Persistência: %.2f" % noise.persistence
	lacunarity_label.text = "Lacunaridade: %.2f" % noise.lacunarity
	scale_label.text = "Escala: %.3f" % _scale
