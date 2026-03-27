#version 450
#extension GL_EXT_nonuniform_qualifier : require // Required for variable indexing

layout(binding = 1) uniform sampler2D texSamplers[]; // All textures live here

layout(push_constant) uniform PC {int texture_id} pc;

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragColor;
layout(location = 2) in vec3 fragNorm;
layout(location = 3) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

void main() {
    // Material color from brick texture
    vec3 meshColor = texture(texSamplers[nonuniformEXT(pc.texture_id)], fragTexCoord).rgb;

    // Light properties
    vec3 lightColor = vec3(1.0, 1.0, 1.0);
    vec3 lightPos   = vec3(10.0, 10.0, 5.0);

    // Camera at origin (view space) — approximate eye position in world space.
    // For a proper solution, pass the camera position via a uniform.
    vec3 viewPos = vec3(0.0, 1.0, 2.0);

    vec3 N = normalize(fragNorm);
    vec3 L = normalize(lightPos - fragPos);
    vec3 V = normalize(viewPos - fragPos);
    vec3 H = normalize(L + V);  // Blinn-Phong half-vector

    // Ambient
    float ambientStrength = 0.15;
    vec3 ambient = ambientStrength * lightColor;

    // Diffuse (Lambertian)
    float diff = max(dot(N, L), 0.0);
    vec3 diffuse = diff * lightColor;

    // Specular (Blinn-Phong)
    float shininess = 32.0;
    float spec = pow(max(dot(N, H), 0.0), shininess);
    float specularStrength = 0.5;
    vec3 specular = specularStrength * spec * lightColor;

    vec3 result = (ambient + diffuse + specular) * meshColor;
    
    outColor = vec4(result, 1.0);
}