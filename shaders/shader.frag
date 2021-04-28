#version 450
#extension GL_ARB_separate_shader_objects : enable


layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec3 inViewVec;
layout(location = 4) in vec3 inLightVec;

layout(location = 0) out vec4 outColor;

layout(binding = 1) uniform sampler2D texSampler;

void main() {
    vec4 textureColor = texture(texSampler, fragTexCoord);
	
	
	vec4 ambient = vec4(0.25) * textureColor;
	vec3 N = normalize(inNormal);
	vec3 L = normalize(inLightVec);
	vec3 V = normalize(inViewVec);
	vec3 R = reflect(-L, N);
	vec4 diffuse = max(dot(N, L), 0.0) * textureColor;
	vec4 specular = pow(max(dot(R, V), 0.0), 32.0) * vec4(0.75);
	outColor = vec4(ambient + diffuse * 1.75 + specular);		

}