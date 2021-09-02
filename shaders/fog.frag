#version 330 core

struct Buffers {
  sampler2D depth;
  sampler2D color;
};

in vec2 fragTexcoords;

layout(std140) uniform uboCameraMatrices {
  mat4 viewMat;
  mat4 invViewMat;
  mat4 projectionMat;
  mat4 invProjectionMat;
  mat4 viewProjectionMat;
  vec3 cameraPos;
};

uniform Buffers uniSceneBuffers;
uniform vec3 uniSunDir;
uniform float uniFogDensity = 0.1;

layout(location = 0) out vec4 fragColor;

vec3 computeViewPosFromDepth(float depth) {
  vec4 projPos = vec4(vec3(fragTexcoords, depth) * 2.0 - 1.0, 1.0);
  vec4 viewPos = invProjectionMat * projPos;

  return viewPos.xyz / viewPos.w;
}

// Basic fog implementation from https://www.iquilezles.org/www/articles/fog/fog.htm
vec3 computeFog(vec3 color, float distance, vec3 viewDir) {
  float fogDensity = uniFogDensity / 20.0;
  float fogAmount  = 1.0 - exp(-distance * fogDensity);

  float sunAmount = max(-dot(viewDir, mat3(viewMat) * uniSunDir), 0.0);
  vec3 fogColor   = mix(vec3(0.5, 0.6, 0.7), // Sky/fog color (blue)
                        vec3(1.0, 0.9, 0.7), // Sun color (yellow)
                        pow(sunAmount, 8.0));

  return mix(color, fogColor, fogAmount);
}

void main() {
  float depth = texture(uniSceneBuffers.depth, fragTexcoords).r;
  vec3 color  = texture(uniSceneBuffers.color, fragTexcoords).rgb;

  vec3 viewPos     = computeViewPosFromDepth(depth);
  float viewDist   = length(viewPos);
  vec3 foggedColor = computeFog(color, viewDist, viewPos / viewDist);

  fragColor = vec4(foggedColor, 1.0);
}
