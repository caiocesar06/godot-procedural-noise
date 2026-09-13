extends HBoxContainer

# --- UI REFERENCES ---
@onready var multi_mesh_instance: MultiMeshInstance3D = $"ViewportContainer/3DEnvironment/MultiMeshInstance3D"
@onready var camera: Camera3D = $"ViewportContainer/3DEnvironment/Camera3D"
@onready var generate_button: Button = $SettingsPanel/SettingsVBox/GenerateButton

@onready var threshold_slider: HSlider = $SettingsPanel/SettingsVBox/ThresholdSlider
@onready var threshold_label: Label = $SettingsPanel/SettingsVBox/ThresholdLabel
@onready var scale_slider: HSlider = $SettingsPanel/SettingsVBox/ScaleSlider
@onready var scale_label: Label = $SettingsPanel/SettingsVBox/ScaleLabel
@onready var octaves_slider: HSlider = $SettingsPanel/SettingsVBox/OctavesSlider
@onready var octaves_label: Label = $SettingsPanel/SettingsVBox/OctavesLabel

# --- MEMORY STATE ---
var perlin: PerlinNoise
var grid_size: int = 32

# --- CAMERA STATE ---
var camera_target: Vector3 = Vector3(grid_size / 2.0, grid_size / 2.0, grid_size / 2.0)
var camera_distance: float = 60.0
var camera_yaw: float = PI / 4.0
var camera_pitch: float = -PI / 6.0
var orbit_sensitivity: float = 0.005
var pan_sensitivity: float = 0.05
var zoom_speed: float = 5.0

func _ready() -> void:
	perlin = PerlinNoise.new()
	perlin.set_seed(42)

	generate_button.pressed.connect(_on_generate_button_pressed)
	threshold_slider.value_changed.connect(_on_parameters_changed)
	scale_slider.value_changed.connect(_on_parameters_changed)
	octaves_slider.value_changed.connect(_on_parameters_changed)

	_update_ui_texts()
	_update_camera_transform()
	generate_volume()

# --- INPUT HANDLING (CÂMERA ORBITAL) ---
func _input(event: InputEvent) -> void:
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
			camera_distance = min(150.0, camera_distance + zoom_speed)
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
	generate_volume()

func _update_ui_texts() -> void:
	threshold_label.text = "Threshold (Densidade): %d" % threshold_slider.value
	scale_label.text = "Escala do Ruido: %.2f" % scale_slider.value
	octaves_label.text = "Octaves: %d" % octaves_slider.value

func _on_parameters_changed(_ignored_value: float) -> void:
	_update_ui_texts()
	generate_volume()

func generate_volume() -> void:
	perlin.set_octaves(int(octaves_slider.value))
	perlin.set_persistence(0.5)

	var scale_factor: float = scale_slider.value

	# O slider continua em 0..255 (densidade em byte, como o usuário já
	# conhece); a API nativa trabalha no valor bruto do ruído. A conversão
	# é a inversa do (val + 1) * 127.5 usado em get_fbm_volume_data.
	var threshold: float = (threshold_slider.value / 127.5) - 1.0

	# Uma única travessia GDScript -> C++. Antes eram grid_size^3 = 32.768
	# iterações em GDScript só para descobrir quais voxels eram sólidos,
	# mais uma chamada de set_instance_transform por bloco.
	var transforms: PackedFloat32Array = perlin.get_solid_voxel_transforms(
		grid_size, grid_size, grid_size,
		scale_factor, threshold,
		0.0, 0.0, 0.0
	)

	var box_mesh = BoxMesh.new()
	box_mesh.size = Vector3(1, 1, 1)

	var material = StandardMaterial3D.new()
	material.albedo_color = Color(0.6, 0.6, 0.6)
	box_mesh.surface_set_material(0, material)

	var multi_mesh = MultiMesh.new()
	multi_mesh.transform_format = MultiMesh.TRANSFORM_3D
	multi_mesh.mesh = box_mesh
	multi_mesh.instance_count = transforms.size() / 12

	if multi_mesh.instance_count > 0:
		multi_mesh.set_buffer(transforms)

	multi_mesh_instance.multimesh = multi_mesh
