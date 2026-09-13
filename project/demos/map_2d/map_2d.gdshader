shader_type canvas_item;

// Normalização enviada pelo script: z = (altura - mean) / std_dev.
// std_dev começa em 1.0, e não em 0.0, para que um uniform esquecido não
// vire divisão por zero (a tela inteira de uma cor só).
uniform float mean = 0.0;
uniform float std_dev = 1.0;

// Limite superior de cada faixa, em desvios padrão: água, areia, floresta,
// montanha. Acima do último limiar, neve.
uniform vec4 thresholds = vec4(0.0, 0.2, 1.0, 1.6);

uniform vec4 water_color : source_color = vec4(0.14, 0.33, 0.60, 1.0);
uniform vec4 sand_color : source_color = vec4(0.87, 0.80, 0.58, 1.0);
uniform vec4 grass_color : source_color = vec4(0.27, 0.52, 0.24, 1.0);
uniform vec4 rock_color : source_color = vec4(0.46, 0.41, 0.37, 1.0);
uniform vec4 snow_color : source_color = vec4(0.95, 0.96, 0.98, 1.0);

// Modo 8 bits: a textura veio de get_fbm_image_data (RGBA8) em vez de
// get_heightmap_data (RF).
uniform bool decode_8bit = false;

uniform bool shading_enabled = false;
// Distância entre texels vizinhos em unidades de ruído (a escala do mapa).
// Dividir a diferença por ela dá a inclinação de verdade: sem isso, o
// relevo sumiria ao aproximar e estouraria ao afastar (a diferença bruta
// entre vizinhos varia 17x entre as escalas 0.002 e 0.05).
uniform float sample_spacing = 0.01;
// Exagero vertical. Medido com os parâmetros padrão: a inclinação média
// fica em ~4.5 sigma por unidade de ruído, e 0.15 a leva para ~0.6.
uniform float height_exaggeration = 0.15;
// Direção de onde vem a luz: canto superior esquerdo, ACIMA do plano.
// O z precisa ser positivo: com z negativo a luz vem de baixo do terreno,
// o produto escalar com as normais (que apontam para cima) fica negativo,
// e tudo cai na luz ambiente.
uniform vec3 light_direction = vec3(-1.0, -1.0, 2.0);

// Converte o valor lido da textura de volta para a altura do ruído.
//   float: a textura já guarda a altura.
//   8 bits: o C++ grava byte = trunc((v + 1) * 127.5) e o shader lê
//   r = byte / 255, então v = r * 255 / 127.5 - 1. Como houve TRUNCAMENTO,
//   o byte b representa todo o intervalo [b, b + 1); somar 0.5 devolve o
//   centro dele. Sem isso, 2r - 1 desloca o mapa inteiro em -0.03 sigma
//   (medido).
float decode(float r) {
	if (!decode_8bit) {
		return r;
	}
	return (r * 255.0 + 0.5) / 127.5 - 1.0;
}

// Funções não enxergam TEXTURE nem UV (são built-ins do fragment), então
// a leitura da textura fica no fragment e só o valor lido chega aqui.
float to_z(float r) {
	return (decode(r) - mean) / std_dev;
}

void fragment() {
	float z = to_z(texture(TEXTURE, UV).r);

	// Camadas: cada faixa cobre a anterior a partir do seu limiar. Se o
	// usuário inverter dois limiares, a faixa do meio simplesmente some --
	// o mapa não quebra.
	vec4 color = water_color;
	color = mix(color, sand_color, step(thresholds.x, z));
	color = mix(color, grass_color, step(thresholds.y, z));
	color = mix(color, rock_color, step(thresholds.z, z));
	color = mix(color, snow_color, step(thresholds.w, z));

	// A água fica plana: sombrear o mar desenharia relevo onde não há.
	if (shading_enabled && z >= thresholds.x) {
		// Diferenças centrais entre os quatro vizinhos -- a mesma ideia das
		// normais de get_terrain_mesh_arrays, só que por pixel, na GPU.
		// Em 8 bits, vizinhos no mesmo byte dão diferença exatamente zero:
		// o terreno parece plano e depois salta um nível. Daí os degraus.
		vec2 px = TEXTURE_PIXEL_SIZE;
		float z_left = to_z(texture(TEXTURE, UV - vec2(px.x, 0.0)).r);
		float z_right = to_z(texture(TEXTURE, UV + vec2(px.x, 0.0)).r);
		float z_up = to_z(texture(TEXTURE, UV - vec2(0.0, px.y)).r);
		float z_down = to_z(texture(TEXTURE, UV + vec2(0.0, px.y)).r);

		float k = height_exaggeration / (2.0 * sample_spacing);
		vec3 normal = normalize(vec3(
			-(z_right - z_left) * k,
			-(z_down - z_up) * k,
			1.0
		));
		float light = clamp(dot(normal, normalize(light_direction)), 0.0, 1.0);

		// Luz ambiente + difusa: sem a parcela ambiente, as encostas
		// contra a luz ficariam totalmente pretas.
		color.rgb *= 0.35 + 0.65 * light;
	}

	COLOR = vec4(color.rgb, 1.0);
}
