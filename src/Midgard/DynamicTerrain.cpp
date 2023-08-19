#include "Midgard/DynamicTerrain.hpp"

#include <RaZ/Entity.hpp>
#include <RaZ/Data/Mesh.hpp>
#include <RaZ/Render/MeshRenderer.hpp>
#include <RaZ/Render/Renderer.hpp>
#include <RaZ/Utils/Logger.hpp>

namespace {

constexpr std::string_view tessCtrlSource = {
#include "terrain.tesc.embed"
};

constexpr std::string_view tessEvalSource = {
#include "terrain.tese.embed"
};

constexpr std::string_view noiseCompSource = {
#include "perlin_noise_2d.comp.embed"
};

constexpr std::string_view colorCompSource = {
#include "terrain_color.comp.embed"
};

inline void checkParameters(float& minTessLevel) {
  if (minTessLevel <= 0.f) {
    Raz::Logger::warn("[DynamicTerrain] The minimal tessellation level can't be 0 or negative; remapping to +epsilon.");
    minTessLevel = std::numeric_limits<float>::epsilon();
  }
}

} // namespace

DynamicTerrain::DynamicTerrain(Raz::Entity& entity) : Terrain(entity) {
  Raz::RenderShaderProgram& terrainProgram = m_entity.getComponent<Raz::MeshRenderer>().getMaterials().front().getProgram();
  terrainProgram.setTessellationControlShader(Raz::TessellationControlShader::loadFromSource(tessCtrlSource));
  terrainProgram.setTessellationEvaluationShader(Raz::TessellationEvaluationShader::loadFromSource(tessEvalSource));
  terrainProgram.link();

  m_noiseMap = Raz::Texture2D::create(1024, 1024, Raz::TextureColorspace::GRAY, Raz::TextureDataType::FLOAT16);
  m_colorMap = Raz::Texture2D::create(1024, 1024, Raz::TextureColorspace::RGBA, Raz::TextureDataType::BYTE);

#if !defined(USE_OPENGL_ES)
  if (Raz::Renderer::checkVersion(4, 3)) {
    Raz::Renderer::setLabel(Raz::RenderObjectType::TEXTURE, m_noiseMap->getIndex(), "Noise map");
    Raz::Renderer::setLabel(Raz::RenderObjectType::TEXTURE, m_colorMap->getIndex(), "Color map");
  }
#endif

  m_noiseProgram.setShader(Raz::ComputeShader::loadFromSource(noiseCompSource));
  m_noiseProgram.setAttribute(8, "uniOctaveCount");
  m_noiseProgram.sendAttributes();
  m_noiseProgram.use();
  Raz::Renderer::bindImageTexture(0, m_noiseMap->getIndex(), 0, false, 0, Raz::ImageAccess::WRITE, Raz::ImageInternalFormat::R16F);

  m_colorProgram.setShader(Raz::ComputeShader::loadFromSource(colorCompSource));
  m_colorProgram.use();
  Raz::Renderer::bindImageTexture(0, m_noiseMap->getIndex(), 0, false, 0, Raz::ImageAccess::READ, Raz::ImageInternalFormat::R16F);
  Raz::Renderer::bindImageTexture(1, m_colorMap->getIndex(), 0, false, 0, Raz::ImageAccess::WRITE, Raz::ImageInternalFormat::RGBA8);

  terrainProgram.setTexture(m_noiseMap, "uniHeightmap");
  terrainProgram.setTexture(m_colorMap, Raz::MaterialTexture::BaseColor);

  computeNoiseMap(0.01f);
  computeColorMap();
}

DynamicTerrain::DynamicTerrain(Raz::Entity& entity, unsigned int width, unsigned int depth,
                               float heightFactor, float flatness, float minTessLevel) : DynamicTerrain(entity) {
  Raz::RenderShaderProgram& terrainProgram = entity.getComponent<Raz::MeshRenderer>().getMaterials().front().getProgram();
  terrainProgram.setAttribute(Raz::Vec2u(width, depth), "uniTerrainSize");
  terrainProgram.sendAttributes();

  DynamicTerrain::generate(width, depth, heightFactor, flatness, minTessLevel);
}

void DynamicTerrain::setParameters(float minTessLevel, float heightFactor, float flatness) {
  Terrain::setParameters(heightFactor, flatness);

  ::checkParameters(minTessLevel);
  m_minTessLevel = minTessLevel;

  Raz::RenderShaderProgram& terrainProgram = m_entity.getComponent<Raz::MeshRenderer>().getMaterials().front().getProgram();
  terrainProgram.setAttribute(m_minTessLevel, "uniTessLevel");
  terrainProgram.setAttribute(m_heightFactor, "uniHeightFactor");
  terrainProgram.setAttribute(m_flatness, "uniFlatness");
  terrainProgram.sendAttributes();
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

  for (std::size_t widthPatchIndex = 0; widthPatchIndex < patchCount; ++widthPatchIndex) {
    const float patchStartX = -static_cast<float>(width) * 0.5f + strideX * static_cast<float>(widthPatchIndex);
    const float patchStartU = strideU * static_cast<float>(widthPatchIndex);
    const std::size_t finalWidthPatchIndex = widthPatchIndex * patchCount;

    for (std::size_t depthPatchIndex = 0; depthPatchIndex < patchCount; ++depthPatchIndex) {
      const float patchStartZ = -static_cast<float>(depth) * 0.5f + strideZ * static_cast<float>(depthPatchIndex);
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

const Raz::Texture2D& DynamicTerrain::computeNoiseMap(float factor) {
  m_noiseProgram.setAttribute(factor, "uniNoiseFactor");
  m_noiseProgram.sendAttributes();
  m_noiseProgram.execute(1024, 1024);

  return *m_noiseMap;
}

const Raz::Texture2D& DynamicTerrain::computeColorMap() {
  m_colorProgram.execute(1024, 1024);

  return *m_colorMap;
}