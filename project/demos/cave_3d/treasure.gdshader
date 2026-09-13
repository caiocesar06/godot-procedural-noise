shader_type spatial;

// unshaded: o tesouro não depende da luz da cena, então brilha por igual
// mesmo no fundo escuro de uma caverna. O glow do WorldEnvironment soma o
// halo em volta.
render_mode unshaded;

uniform vec4 treasure_color : source_color = vec4(1.0, 0.78, 0.25, 1.0);
uniform float glow_energy = 1.6;
uniform float pulse_speed = 3.0;
uniform float cut_height = 64.0;

varying float instance_y;

void vertex() {
	instance_y = MODEL_MATRIX[3].y;
}

void fragment() {
	if (instance_y > cut_height) {
		discard;
	}
	// Pulsação defasada pela altura, para os tesouros não piscarem juntos.
	float pulse = 0.75 + 0.25 * sin(TIME * pulse_speed + instance_y);
	ALBEDO = treasure_color.rgb * glow_energy * pulse;
}
