layout(vertices = 4) out;

struct MeshInfo {
  vec3 vertPosition;
  vec2 vertTexcoords;
  mat3 vertTBNMatrix;
};

in MeshInfo vertMeshInfo[];

layout(std140) uniform uboCameraMatrices {
  mat4 viewMat;
  mat4 invViewMat;
  mat4 projectionMat;
  mat4 invProjectionMat;
  mat4 viewProjectionMat;
  vec3 cameraPos;
};

uniform float uniTessLevel = 12.0;

out MeshInfo tessMeshInfo[];

void main() {
  gl_out[gl_InvocationID].gl_Position         = gl_in[gl_InvocationID].gl_Position;
  tessMeshInfo[gl_InvocationID].vertPosition  = vertMeshInfo[gl_InvocationID].vertPosition;
  tessMeshInfo[gl_InvocationID].vertTexcoords = vertMeshInfo[gl_InvocationID].vertTexcoords;
  tessMeshInfo[gl_InvocationID].vertTBNMatrix = vertMeshInfo[gl_InvocationID].vertTBNMatrix;

  if (gl_InvocationID == 0) {
    vec3 patchCentroid    = (vertMeshInfo[0].vertPosition + vertMeshInfo[1].vertPosition + vertMeshInfo[2].vertPosition + vertMeshInfo[3].vertPosition) / 4.0;
    float tessLevelFactor = (1.0 / distance(cameraPos, patchCentroid)) * 512.0;

    // Outer levels get a higher factor in order to attempt filling gaps between patches
    // TODO: A better way must be found, like varying the level according the edges mid points
    //   See: https://www.khronos.org/opengl/wiki/Tessellation#Patch_interface_and_continuity
    gl_TessLevelOuter[0] = uniTessLevel * (tessLevelFactor * 4.0);
    gl_TessLevelOuter[1] = uniTessLevel * (tessLevelFactor * 4.0);
    gl_TessLevelOuter[2] = uniTessLevel * (tessLevelFactor * 4.0);
    gl_TessLevelOuter[3] = uniTessLevel * (tessLevelFactor * 4.0);

    gl_TessLevelInner[0] = uniTessLevel * tessLevelFactor;
    gl_TessLevelInner[1] = uniTessLevel * tessLevelFactor;
  }
}
