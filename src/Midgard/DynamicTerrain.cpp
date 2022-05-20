#include "Midgard/DynamicTerrain.hpp"

#include <RaZ/Entity.hpp>
#include <RaZ/Data/Mesh.hpp>
#include <RaZ/Render/MeshRenderer.hpp>
#include <RaZ/Utils/Logger.hpp>

namespace {

constexpr std::string_view tessCtrlSource = {
#include "terrain.tesc.embed"
};

constexpr std::string_view tessEvalSource = {
#include "terrain.tese.embed"
};

constexpr std::string_view noiseCompSource = {
#include "perlin_noise.comp.embed"
};

inline void checkParameters(float& minTessLevel) {
  if (minTessLevel <= 0.f) {
    Raz::Logger::warn("[DynamicTerrain] The minimal tessellation level can't be 0 or negative; remapping to +epsilon.");
    minTessLevel = std::numeric_limits<float>::epsilon();
  }
}

} // namespace

DynamicTerrain::DynamicTerrain(Raz::Entity& entity) : Terrain(entity) {
  Raz::Material& material = m_entity.getComponent<Raz::MeshRenderer>().getMaterials().front();

  Raz::RenderShaderProgram& terrainProgram = material.getProgram();
  terrainProgram.setTessellationControlShader(Raz::TessellationControlShader::loadFromSource(tessCtrlSource));
  terrainProgram.setTessellationEvaluationShader(Raz::TessellationEvaluationShader::loadFromSource(tessEvalSource));
  terrainProgram.link();

  m_noiseProgram.setShader(Raz::ComputeShader::loadFromSource(noiseCompSource));
  m_noiseMap = Raz::Texture::create(1024, 1024, Raz::ImageColorspace::GRAY, Raz::ImageDataType::FLOAT);

  material.setTexture(m_noiseMap, "uniNoiseMap");

  m_noiseMap->bind();
  Raz::Renderer::bindImageTexture(0, m_noiseMap->getIndex(), 0, false, 0, Raz::ImageAccess::WRITE, Raz::ImageInternalFormat::R16F);
  m_noiseMap->unbind();

  computeNoiseMap(0.01f);
}

DynamicTerrain::DynamicTerrain(Raz::Entity& entity, unsigned int width, unsigned int depth,
                               float heightFactor, float flatness, float minTessLevel) : DynamicTerrain(entity) {
  Raz::RenderShaderProgram& program = entity.getComponent<Raz::MeshRenderer>().getMaterials().front().getProgram();
  program.use();
  program.sendUniform("uniTerrainSize", Raz::Vec2u(width, depth));

  DynamicTerrain::generate(width, depth, heightFactor, flatness, minTessLevel);
}

void DynamicTerrain::setParameters(float minTessLevel, float heightFactor, float flatness) {
  Terrain::setParameters(heightFactor, flatness);

  ::checkParameters(minTessLevel);
  m_minTessLevel = minTessLevel;

  Raz::RenderShaderProgram& program = m_entity.getComponent<Raz::MeshRenderer>().getMaterials().front().getProgram();
  program.use();
  program.sendUniform("uniTessLevel", m_minTessLevel);
  program.sendUniform("uniHeightFactor", m_heightFactor);
  program.sendUniform("uniFlatness", m_flatness);
}

void DynamicTerrain::generate(unsigned int width, unsigned int depth, float heightFactor, float flatness, float minTessLevel) {
  auto& mesh = m_entity.getComponent<Raz::Mesh>();
  mesh.getSubmeshes().resize(1);

  constexpr int patchCount = 20;

  const float strideX = static_cast<float>(width) / patchCount;
  const float strideZ = static_cast<float>(depth) / patchCount;
  const float strideU = 1.f / patchCount;
  const float strideV = 1.f / patchCount;

  // Creating patches:
  //
  // 1------3
  // |      |     Z
  // |      |     ^
  // |      |     |
  // 0------2     ---> X

  std::vector<Raz::Vertex>& vertices = mesh.getSubmeshes().front().getVertices();
  vertices.resize(patchCount * patchCount * 4);

  for (int widthPatchIndex = 0; widthPatchIndex < patchCount; ++widthPatchIndex) {
    const float patchStartX = -static_cast<float>(width / 2) + strideX * static_cast<float>(widthPatchIndex);
    const float patchStartU = strideU * static_cast<float>(widthPatchIndex);
    const std::size_t finalWidthPatchIndex = widthPatchIndex * patchCount;

    for (int depthPatchIndex = 0; depthPatchIndex < patchCount; ++depthPatchIndex) {
      const float patchStartZ = -static_cast<float>(depth / 2) + strideZ * static_cast<float>(depthPatchIndex);
      const float patchStartV = strideV * static_cast<float>(depthPatchIndex);
      const std::size_t finalPatchIndex = (finalWidthPatchIndex + depthPatchIndex) * 4;

      vertices[finalPatchIndex] = Raz::Vertex{ Raz::Vec3f(patchStartX, 0.f, patchStartZ),
                                               Raz::Vec2f(patchStartU, patchStartV),
                                               Raz::Axis::Y,
                                               Raz::Axis::X };
      vertices[finalPatchIndex + 1] = Raz::Vertex{ Raz::Vec3f(patchStartX, 0.f, patchStartZ + strideZ),
                                                   Raz::Vec2f(patchStartU, patchStartV + strideV),
                                                   Raz::Axis::Y,
                                                   Raz::Axis::X };
      vertices[finalPatchIndex + 2] = Raz::Vertex{ Raz::Vec3f(patchStartX + strideX, 0.f, patchStartZ),
                                                   Raz::Vec2f(patchStartU + strideU, patchStartV),
                                                   Raz::Axis::Y,
                                                   Raz::Axis::X };
      vertices[finalPatchIndex + 3] = Raz::Vertex{ Raz::Vec3f(patchStartX + strideX, 0.f, patchStartZ + strideZ),
                                                   Raz::Vec2f(patchStartU + strideU, patchStartV + strideV),
                                                   Raz::Axis::Y,
                                                   Raz::Axis::X };
    }
  }

  m_entity.getComponent<Raz::MeshRenderer>().load(mesh, Raz::RenderMode::PATCH);
  Raz::Renderer::setPatchVertexCount(4); // Since the terrain is made of quads

  setParameters(minTessLevel, heightFactor, flatness);
}

const Raz::TexturePtr& DynamicTerrain::computeNoiseMap(float factor) {
  m_noiseProgram.use();
  m_noiseProgram.sendUniform("uniFactor", factor);
  m_noiseProgram.execute(1024, 1024);

  return m_noiseMap;
}
