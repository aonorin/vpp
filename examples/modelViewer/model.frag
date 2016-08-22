#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(location = 0) in vec3 ipos;
layout(location = 1) in vec3 inormal;
layout(location = 2) in vec2 iuv;

layout(location = 0) out vec4 ocol;

layout(set = 0, binding = 1) uniform UboScene
{
	vec3 viewPos;
	vec3 lightDir;
	vec3 lightCol;
} scene;

layout(set = 0, binding = 2) uniform UboMaterial
{
	vec4 ambient;
	vec4 diffuse;
	vec4 specular;
	float shininess;
	bool texture;
} material;

layout(set = 0, binding = 3) uniform sampler2D diffuseMap;

void main()
{
	// if(material.texture)
	// {
	// 	ocol = texture(diffuseMap, iuv);
	// }

	vec3 ambientCol = 0.2 * scene.lightCol * material.diffuse.rgb;

	vec3 norm = inormal;
	// vec3 lightDir = normalize(scene.lightPos - ipos);
	vec3 lightDir = scene.lightDir;

	float diffuseFac = max(dot(norm, lightDir), 0.0);
	vec3 diffuseCol = 0.8 * diffuseFac * material.diffuse.rgb;

	vec3 viewDir = normalize(scene.viewPos - ipos);
	vec3 reflectDir = reflect(-lightDir, norm);
	float specFac = pow(max(dot(viewDir, reflectDir), 0.0), 16);
	vec3 specularCol = 0.2 * specFac * material.diffuse.rgb;

	vec3 result = ambientCol + specularCol + diffuseCol;
	ocol = vec4(result, 1.0);

	// ocol = vec4(1.0, 1.0, 1.0, 1.0);
}