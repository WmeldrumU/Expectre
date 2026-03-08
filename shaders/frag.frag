#version 450

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragColor;
layout(location = 2) in vec3 fragNorm;
layout(location = 3) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;


void main() {
    //outColor = vec4(fragTexCoord, 0.0, 1.0);
    // Ambient
    vec3 light_color = vec3(1.0, 1.0, 1.0);
    vec3 mesh_color = vec3 (0.4, 0.4, 0.43);
    //vec3 mesh_color = texture(texSampler, fragTexCoord).rgb; // Use texture if available

    float ambient_coef = 0.8;
    vec3 ambient = ambient_coef * light_color;

    // Diffuse
    // vec3 normal = normalize(vec3(0.0, 1.0, 1.0));
    vec3 normal = normalize(fragNorm);
    vec3 light_dir = normalize(vec3(10.0, 10.0, 0.0) - fragPos);
    float diffuse_coef = max(dot(normal, light_dir), 0.0);
    vec3 diffuse = diffuse_coef * light_color;

    // Specular

    //outColor = vec4(fragNorm, 1.0);
    outColor = vec4((ambient + diffuse) * mesh_color, 1.0);
    //outColor = texture(texSampler, fragTexCoord);
}