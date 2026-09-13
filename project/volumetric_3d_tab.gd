extends HBoxContainer

# --- UI REFERENCES ---
@onready var mesh_instance: MeshInstance3D = $"ViewportContainer/3DEnvironment/TerrainMesh"
@onready var generate_button: Button = $SettingsPanel/SettingsVBox/GenerateTerrainButton
@onready var camera: Camera3D = $"ViewportContainer/3DEnvironment/Camera3D"

# Sliders
@onready var octaves_slider: HSlider = $SettingsPanel/SettingsVBox/OctavesSlider
@onready var persistence_slider: HSlider = $SettingsPanel/SettingsVBox/PersistenceSlider
@onready var lacunarity_slider: HSlider = $SettingsPanel/SettingsVBox/LacunaritySlider
@onready var amplitude_slider: HSlider = $SettingsPanel/SettingsVBox/AmplitudeSlider

# Labels
@onready var octaves_label: Label = $SettingsPanel/SettingsVBox/OctavesLabel
@onready var persistence_label: Label = $SettingsPanel/SettingsVBox/PersistenceLabel
@onready var lacunarity_label: Label = $SettingsPanel/SettingsVBox/LacunarityLabel
@onready var amplitude_label: Label = $SettingsPanel/SettingsVBox/AmplitudeLabel

# --- MEMORY STATE ---
var perlin: PerlinNoise
var grid_size: int = 100
var scale_factor: float = 0.05

# --- ORBIT CAMERA STATE ---
var camera_target: Vector3 = Vector3(0, 0, 0)
var camera_distance: float = 80.0
var camera_yaw: float = PI / 4.0
var camera_pitch: float = -PI / 6.0
var orbit_sensitivity: float = 0.005
var pan_sensitivity: float = 0.05
var zoom_speed: float = 5.0

func _ready() -> void:
	perlin = PerlinNoise.new()
	perlin.set_seed(42)
	perlin.set_fractal_type(PerlinNoise.FRACTAL_BILLOW)

	generate_button.pressed.connect(_on_generate_button_pressed)

	octaves_slider.value_changed.connect(_on_parameters_changed)
	persistence_slider.value_changed.connect(_on_parameters_changed)
	lacunarity_slider.value_changed.connect(_on_parameters_changed)
	amplitude_slider.value_changed.connect(_on_parameters_changed)

	_update_ui_texts()
	_generate_3d_terrain()

	_update_camera_transform()


# --- INPUT HANDLING (Event-Driven) ---

func _input(event: InputEvent) -> void:
	# 1. ORBIT (Girar a malha): Botão Esquerdo + Arrastar
	if event is InputEventMouseMotion and Input.is_mouse_button_pressed(MOUSE_BUTTON_LEFT):
		camera_yaw -= event.relative.x * orbit_sensitivity
		camera_pitch -= event.relative.y * orbit_sensitivity

		camera_pitch = clamp(camera_pitch, -PI / 2.0 + 0.01, PI / 2.0 - 0.01)
		_update_camera_transform()

	elif event is InputEventMouseMotion and Input.is_mouse_button_pressed(MOUSE_BUTTON_RIGHT):
		var right_dir = camera.transform.basis.x
		var up_dir = camera.transform.basis.y

		camera_target -= right_dir * event.relative.x * pan_sensitivity
		camera_target += up_dir * event.relative.y * pan_sensitivity
		_update_camera_transform()

	elif event is InputEventMouseButton:
		if event.button_index == MOUSE_BUTTON_WHEEL_UP:
			camera_distance = max(5.0, camera_distance - zoom_speed)
			_update_camera_transform()
		elif event.button_index == MOUSE_BUTTON_WHEEL_DOWN:
			camera_distance = min(200.0, camera_distance + zoom_speed)
			_update_camera_transform()

func _update_camera_transform() -> void:
	var offset = Vector3(0, 0, camera_distance)
	offset = offset.rotated(Vector3.RIGHT, camera_pitch)
	offset = offset.rotated(Vector3.UP, camera_yaw)

	camera.position = camera_target + offset
	camera.look_at(camera_target)

# --- TERRAIN GENERATION LOGIC ---

func _on_generate_button_pressed() -> void:
	perlin.randomize_seed()
	_generate_3d_terrain()

func _update_ui_texts() -> void:
	octaves_label.text = "Octaves: %d" % octaves_slider.value
	persistence_label.text = "Persistence: %.2f" % persistence_slider.value
	lacunarity_label.text = "Lacunarity: %.2f" % lacunarity_slider.value
	amplitude_label.text = "Amplitude: %.1f" % amplitude_slider.value

func _on_parameters_changed(_ignored_value: float) -> void:
	_update_ui_texts()
	_generate_3d_terrain()

func _generate_3d_terrain() -> void:
	perlin.set_octaves(int(octaves_slider.value))
	perlin.set_persistence(persistence_slider.value)
	perlin.set_lacunarity(lacunarity_slider.value)

	# Vértices, normais, UVs e índices vêm prontos do C++. Antes eram
	# 10.000 chamadas de get_fractal_noise_2d atravessando a fronteira
	# GDScript -> C++ para montar a malha com SurfaceTool; agora é uma.
	# As normais saem por diferenças centrais sobre o campo de altura,
	# em vez de generate_normals() sobre a malha já montada.
	var surface: Array = perlin.get_terrain_mesh_arrays(
		grid_size,
		scale_factor,
		amplitude_slider.value,
		0.0, 0.0
	)

	var array_mesh := ArrayMesh.new()
	array_mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, surface)

	var terrain_material = StandardMaterial3D.new()
	terrain_material.albedo_color = Color(0.25, 0.4, 0.2)
	terrain_material.roughness = 0.8
	array_mesh.surface_set_material(0, terrain_material)

	mesh_instance.mesh = array_mesh
	mesh_instance.position = Vector3(-grid_size / 2.0, 0, -grid_size / 2.0)
