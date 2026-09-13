extends Camera3D

# Câmera de voo livre.
#   botão direito (segurar) -> olhar com o mouse
#   W A S D                 -> mover
#   Q / E                   -> descer / subir
#   Shift                   -> acelerar
#   roda do mouse           -> ajustar a velocidade
# Com o botão direito solto, o mouse fica livre para a interface.

const BOOST := 3.0
const MIN_SPEED := 2.0
const MAX_SPEED := 120.0

# Para onde a câmera olha ao abrir a cena: o centro do volume 64^3.
@export var look_target := Vector3(32.0, 32.0, 32.0)
@export var speed := 20.0
@export var mouse_sensitivity := 0.003

var _yaw := 0.0
var _pitch := 0.0


func _ready() -> void:
	look_at(look_target)
	_pitch = rotation.x
	_yaw = rotation.y


# _unhandled_input só recebe o que a interface não consumiu: clicar num
# slider não mexe a câmera.
func _unhandled_input(event: InputEvent) -> void:
	var button := event as InputEventMouseButton
	if button:
		if button.button_index == MOUSE_BUTTON_RIGHT:
			Input.mouse_mode = (
				Input.MOUSE_MODE_CAPTURED if button.pressed else Input.MOUSE_MODE_VISIBLE
			)
		elif button.pressed and button.button_index == MOUSE_BUTTON_WHEEL_UP:
			speed = minf(speed * 1.2, MAX_SPEED)
		elif button.pressed and button.button_index == MOUSE_BUTTON_WHEEL_DOWN:
			speed = maxf(speed / 1.2, MIN_SPEED)
		return

	var motion := event as InputEventMouseMotion
	if motion and Input.mouse_mode == Input.MOUSE_MODE_CAPTURED:
		_yaw -= motion.relative.x * mouse_sensitivity
		# Limitar o pitch evita virar de cabeça para baixo ao passar do zênite.
		_pitch = clampf(
			_pitch - motion.relative.y * mouse_sensitivity,
			-PI / 2.0 + 0.01, PI / 2.0 - 0.01
		)
		rotation = Vector3(_pitch, _yaw, 0.0)


func _process(delta: float) -> void:
	# Teclas físicas: a posição do W é a mesma em QWERTY e ABNT, e não é
	# preciso configurar o Input Map do projeto.
	var direction := Vector3.ZERO
	if Input.is_physical_key_pressed(KEY_W):
		direction -= basis.z
	if Input.is_physical_key_pressed(KEY_S):
		direction += basis.z
	if Input.is_physical_key_pressed(KEY_A):
		direction -= basis.x
	if Input.is_physical_key_pressed(KEY_D):
		direction += basis.x
	if Input.is_physical_key_pressed(KEY_E):
		direction += Vector3.UP
	if Input.is_physical_key_pressed(KEY_Q):
		direction -= Vector3.UP

	if direction == Vector3.ZERO:
		return
	var current_speed := speed * (BOOST if Input.is_physical_key_pressed(KEY_SHIFT) else 1.0)
	position += direction.normalized() * current_speed * delta
