layout(quads, fractional_odd_spacing, ccw) in;

struct MeshInfo {
  vec3 vertPosition;
  vec2 vertTexcoords;
  mat3 vertTBNMatrix;
};

in MeshInfo tessMeshInfo[];

uniform sampler2D uniHeightmap;
uniform uvec2 uniTerrainSize;
uniform float uniFlatness     = 3.0;
uniform float uniHeightFactor = 30.0;

layout(std140) uniform uboCameraMatrices {
  mat4 viewMat;
  mat4 invViewMat;
  mat4 projectionMat;
  mat4 invProjectionMat;
  mat4 viewProjectionMat;
  vec3 cameraPos;
};

out MeshInfo vertMeshInfo;

vec3 computeNormalDifferences(vec2 vertUV) {
  float uStride = 1.0 / float(uniTerrainSize.x);
  float vStride = 1.0 / float(uniTerrainSize.y);

  float topHeight   = pow(texture(uniHeightmap, vertUV + vec2(     0.0, -vStride)).r, uniFlatness) * uniHeightFactor;
  float leftHeight  = pow(texture(uniHeightmap, vertUV + vec2(-uStride,      0.0)).r, uniFlatness) * uniHeightFactor;
  float rightHeight = pow(texture(uniHeightmap, vertUV + vec2( uStride,      0.0)).r, uniFlatness) * uniHeightFactor;
  float botHeight   = pow(texture(uniHeightmap, vertUV + vec2(     0.0,  vStride)).r, uniFlatness) * uniHeightFactor;

  // Replace y by 2.0-2.5 to get the same result as the cross method
  return normalize(vec3(leftHeight - rightHeight, 1.0, topHeight - botHeight));
}

vec3 computeNormalCross(vec3 vertPos, vec2 vertUV) {
  float uStride = 1.0 / float(uniTerrainSize.x);
  float vStride = 1.0 / float(uniTerrainSize.y);

  vec3 bottomLeftPos  = vertPos + vec3(-1.0, 0.0, -1.0);
  vec3 topLeftPos     = vertPos + vec3(-1.0, 0.0,  1.0);
  vec3 bottomRightPos = vertPos + vec3( 1.0, 0.0, -1.0);
  vec3 topRightPos    = vertPos + vec3( 1.0, 0.0,  1.0);

  float bottomLeftHeight  = texture(uniHeightmap, vertUV + vec2(-uStride, -vStride)).r;
  float topLeftHeight     = texture(uniHeightmap, vertUV + vec2(-uStride,  vStride)).r;
  float bottomRightHeight = texture(uniHeightmap, vertUV + vec2( uStride, -vStride)).r;
  float topRightHeight    = texture(uniHeightmap, vertUV + vec2( uStride,  vStride)).r;

  bottomLeftPos.y  += pow(bottomLeftHeight, uniFlatness) * uniHeightFactor;
  topLeftPos.y     += pow(topLeftHeight, uniFlatness) * uniHeightFactor;
  bottomRightPos.y += pow(bottomRightHeight, uniFlatness) * uniHeightFactor;
  topRightPos.y    += pow(topRightHeight, uniFlatness) * uniHeightFactor;

  vec3 bottomLeftDir  = bottomLeftPos - vertPos;
  vec3 topLeftDir     = topLeftPos - vertPos;
  vec3 bottomRightDir = bottomRightPos - vertPos;
  vec3 topRightDir    = topRightPos - vertPos;

  vec3 normal = cross(bottomLeftDir, topLeftDir);
  normal     += cross(topRightDir, bottomRightDir);
  normal     += cross(bottomRightDir, bottomLeftDir);
  normal     += cross(topLeftDir, topRightDir);

  return normalize(normal);
}

void main() {
  // Top & bottom are actually inverted when looking the terrain from above: [0] & [2] are the patch's top vertices, while [1] & [3] are the bottom's
  vec3 bottomLeftPos  = tessMeshInfo[0].vertPosition;
  vec3 topLeftPos     = tessMeshInfo[1].vertPosition;
  vec3 bottomRightPos = tessMeshInfo[2].vertPosition;
  vec3 topRightPos    = tessMeshInfo[3].vertPosition;

  // Interpolating bilinearly to recover the position on the patch
  vec3 vertPos0 = mix(tessMeshInfo[0].vertPosition, tessMeshInfo[1].vertPosition, gl_TessCoord.x);
  vec3 vertPos1 = mix(tessMeshInfo[2].vertPosition, tessMeshInfo[3].vertPosition, gl_TessCoord.x);
  vec3 vertPos  = mix(vertPos0, vertPos1, gl_TessCoord.y);

  // Interpolating bilinearly to recover the texcoords on the patch
  vec2 vertUV0 = mix(tessMeshInfo[0].vertTexcoords, tessMeshInfo[1].vertTexcoords, gl_TessCoord.x);
  vec2 vertUV1 = mix(tessMeshInfo[2].vertTexcoords, tessMeshInfo[3].vertTexcoords, gl_TessCoord.x);
  vec2 vertUV  = mix(vertUV0, vertUV1, gl_TessCoord.y);

  float midHeight = texture(uniHeightmap, vertUV).r;
  vertPos.y += pow(midHeight, uniFlatness) * uniHeightFactor;

  vec3 normal = computeNormalDifferences(vertUV);
  //vec3 normal = computeNormalCross(vertPos, vertUV);
  vec3 tangent = normal.zxy; // ?

  vertMeshInfo.vertPosition  = vertPos;
  vertMeshInfo.vertTexcoords = vertUV;
  vertMeshInfo.vertTBNMatrix = mat3(tangent, cross(normal, tangent), normal);

  gl_Position = viewProjectionMat * vec4(vertPos, 1.0);
}
