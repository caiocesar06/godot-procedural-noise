shader_type spatial;

// Corte transversal: cubos com o centro acima de cut_height somem. O corte
// é por cubo inteiro, e não por fragmento, para a fatia sair em degraus de
// voxel -- cortar no meio do cubo mostraria o interior oco da BoxMesh.
uniform float cut_height = 64.0;

// Degradê de cor pela altura: dá noção de profundidade dentro da caverna.
uniform vec4 deep_color : source_color = vec4(0.20, 0.18, 0.17, 1.0);
uniform vec4 top_color : source_color = vec4(0.62, 0.57, 0.50, 1.0);
uniform float volume_height = 64.0;

varying float instance_y;

void vertex() {
	// Num MultiMesh, MODEL_MATRIX já inclui a transformação da instância:
	// a coluna 3 é a posição do centro do cubo.
	instance_y = MODEL_MATRIX[3].y;
}

void fragment() {
	if (instance_y > cut_height) {
		discard;
	}
	float t = clamp(instance_y / volume_height, 0.0, 1.0);
	ALBEDO = mix(deep_color.rgb, top_color.rgb, t);
	ROUGHNESS = 0.9;
}
