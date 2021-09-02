#include "Midgard/Terrain.hpp"

#include <RaZ/Math/MathUtils.hpp>
#include <RaZ/Math/PerlinNoise.hpp>
#include <RaZ/Render/Mesh.hpp>
#include <RaZ/Utils/Logger.hpp>

constexpr Raz::Vec3b waterColor(0, 0, 255);
constexpr Raz::Vec3b grassColor(62, 126, 0);
constexpr Raz::Vec3b groundColor(157, 110, 94);
constexpr Raz::Vec3b rockColor(127, 127, 127);
constexpr Raz::Vec3b snowColor(255, 255, 255);

namespace {

inline void checkParameters(float& heightFactor, float& flatness) {
  if (heightFactor <= 0.f)
    Raz::Logger::warn("Terrain height factor can't be 0 or negative; remapping to +epsilon.");

  if (flatness <= 0.f)
    Raz::Logger::warn("Terrain flatness can't be 0 or negative; remapping to +epsilon.");

  heightFactor = std::max(heightFactor, std::numeric_limits<float>::epsilon());
  flatness     = std::max(flatness, std::numeric_limits<float>::epsilon());
}

} // namespace

Terrain::Terrain(Raz::Mesh& mesh) : m_mesh{ mesh } {
  auto& material = static_cast<Raz::MaterialCookTorrance&>(*m_mesh.getMaterials().front());
  material.setMetallicFactor(0.f);
  material.setRoughnessFactor(0.f);
}

void Terrain::setParameters(float heightFactor, float flatness) {
  checkParameters(heightFactor, flatness);

  remapVertices(heightFactor, flatness);

  m_heightFactor = heightFactor;
  m_flatness     = flatness;
  m_invFlatness  = 1.f / flatness;
}

void Terrain::generate(unsigned int width, unsigned int depth, float heightFactor, float flatness) {
  m_width        = width;
  m_depth        = depth;

  checkParameters(heightFactor, flatness);
  m_heightFactor = heightFactor;
  m_flatness     = flatness;
  m_invFlatness  = 1.f / flatness;

  // Computing vertices

  std::vector<Raz::Vertex>& vertices = m_mesh.getSubmeshes().front().getVertices();
  vertices.resize(m_width * m_depth);

  for (unsigned int j = 0; j < m_depth; ++j) {
    const unsigned int depthIndex = j * m_depth;

    for (unsigned int i = 0; i < m_width; ++i) {
      Raz::Vec2f coords(static_cast<float>(i), static_cast<float>(j));

      float noiseValue = Raz::PerlinNoise::get2D(coords.x() / 100.f, coords.y() / 100.f, 8, true);
      noiseValue = std::pow(noiseValue, flatness);

      coords = (coords - static_cast<float>(m_width) / 2.f) / 2.f;

      Raz::Vertex& vertex = vertices[depthIndex + i];
      vertex.position  = Raz::Vec3f(coords.x(), noiseValue * heightFactor, coords.y());
      vertex.texcoords = Raz::Vec2f(static_cast<float>(i) / static_cast<float>(m_width), static_cast<float>(j) / static_cast<float>(m_depth));
    }
  }

  computeNormals();

  // Computing indices

  std::vector<unsigned int>& indices = m_mesh.getSubmeshes().front().getTriangleIndices();
  indices.resize(vertices.size() * 6);

  for (unsigned int j = 0; j < m_depth - 1; ++j) {
    const unsigned int depthIndex = j * m_depth;

    for (unsigned int i = 0; i < m_width - 1; ++i) {
      //     i     i + 1
      //     v       v
      //     --------- <- j * width
      //     |      /|
      //     |    /  |
      //     |  /    |
      //     |/______| <- (j + 1) * width
      //     ^       ^
      //  j + 1    j + 1 + i + 1

      const unsigned int finalIndex = (depthIndex + i) * 6;

      indices[finalIndex    ] = j * m_width + i;
      indices[finalIndex + 1] = j * m_width + i + 1;
      indices[finalIndex + 2] = (j + 1) * m_width + i;
      indices[finalIndex + 3] = (j + 1) * m_width + i;
      indices[finalIndex + 4] = j * m_width + i + 1;
      indices[finalIndex + 5] = (j + 1) * m_width + i + 1;
    }
  }

  m_mesh.load();
}

const Raz::Image& Terrain::computeColorMap() {
  m_colorMap = Raz::Image(m_width, m_depth, Raz::ImageColorspace::RGB);
  auto* imgData = static_cast<uint8_t*>(m_colorMap.getDataPtr());

  const std::vector<Raz::Vertex>& vertices = m_mesh.getSubmeshes().front().getVertices();

  for (unsigned int j = 0; j < m_depth; ++j) {
    const unsigned int depthStride = j * m_width;

    for (unsigned int i = 0; i < m_width; ++i) {
      const float noiseValue      = std::pow(vertices[depthStride + i].position.y() / m_heightFactor, m_invFlatness);
      const Raz::Vec3b pixelValue = (noiseValue < 0.33f ? Raz::MathUtils::lerp(waterColor, grassColor, noiseValue * 3.f)
                                  : (noiseValue < 0.5f  ? Raz::MathUtils::lerp(grassColor, groundColor, (noiseValue - 0.33f) * 5.75f)
                                  : (noiseValue < 0.66f ? Raz::MathUtils::lerp(groundColor, rockColor, (noiseValue - 0.5f) * 6.f)
                                                        : Raz::MathUtils::lerp(rockColor, snowColor, (noiseValue - 0.66f) * 3.f))));

      const unsigned int dataStride = depthStride * 3 + i * 3;
      imgData[dataStride]     = pixelValue.x();
      imgData[dataStride + 1] = pixelValue.y();
      imgData[dataStride + 2] = pixelValue.z();
    }
  }

  auto& material = static_cast<Raz::MaterialCookTorrance&>(*m_mesh.getMaterials().front());
  material.setBaseColorMap(Raz::Texture::create(m_colorMap, 0));

  return m_colorMap;
}

const Raz::Image& Terrain::computeNormalMap() {
  m_normalMap = Raz::Image(m_width, m_depth, Raz::ImageColorspace::RGB);
  auto* imgData = static_cast<uint8_t*>(m_normalMap.getDataPtr());

  const std::vector<Raz::Vertex>& vertices = m_mesh.getSubmeshes().front().getVertices();

  for (unsigned int j = 0; j < m_depth; ++j) {
    const unsigned int depthStride = j * m_width;

    for (unsigned int i = 0; i < m_width; ++i) {
      const Raz::Vec3f& normal = vertices[depthStride + i].normal;

      const unsigned int dataStride = depthStride * 3 + i * 3;
      imgData[dataStride]     = static_cast<uint8_t>(normal.x() * 255.f);
      imgData[dataStride + 1] = static_cast<uint8_t>(normal.y() * 255.f);
      imgData[dataStride + 2] = static_cast<uint8_t>(normal.z() * 255.f);
    }
  }

  return m_normalMap;
}

const Raz::Image& Terrain::computeSlopeMap() {
  m_slopeMap    = Raz::Image(m_width, m_depth, Raz::ImageColorspace::DEPTH);
  auto* imgData = static_cast<float*>(m_slopeMap.getDataPtr());

  const std::vector<Raz::Vertex>& vertices = m_mesh.getSubmeshes().front().getVertices();

  for (unsigned int j = 1; j < m_depth - 1; ++j) {
    const unsigned int depthStride = j * m_width;

    for (unsigned int i = 1; i < m_width - 1; ++i) {
      const float topHeight   = vertices[(j - 1) * m_width + i].position.y();
      const float leftHeight  = vertices[depthStride + i - 1].position.y();
      const float rightHeight = vertices[depthStride + i + 1].position.y();
      const float botHeight   = vertices[(j + 1) * m_width + i].position.y();

      const float length = Raz::Vec2f(leftHeight - rightHeight, topHeight - botHeight).computeLength();
      imgData[depthStride + i] = length * 0.5f;
    }
  }

  return m_slopeMap;
}

void Terrain::computeNormals() {
  std::vector<Raz::Vertex>& vertices = m_mesh.getSubmeshes().front().getVertices();

  for (unsigned int j = 1; j < m_depth - 1; ++j) {
    const unsigned int depthStride = j * m_width;

    for (unsigned int i = 1; i < m_width - 1; ++i) {
      //                      topHeight (j - 1)
      //                           x
      //                           |
      //                           |
      // leftHeight (i - 1) x------X------x rightHeight (i + 1)
      //                           |
      //                           |
      //                           x
      //                    botHeight (j + 1)

      // Using finite differences

      const float topHeight   = vertices[(j - 1) * m_width + i].position.y();
      const float leftHeight  = vertices[depthStride + i - 1].position.y();
      const float rightHeight = vertices[depthStride + i + 1].position.y();
      const float botHeight   = vertices[(j + 1) * m_width + i].position.y();

      Raz::Vertex& midVertex = vertices[depthStride + i];
      midVertex.normal  = Raz::Vec3f(leftHeight - rightHeight, 0.1f, topHeight - botHeight).normalize();
      midVertex.tangent = Raz::Vec3f(midVertex.normal.z(), midVertex.normal.x(), midVertex.normal.y());

      // Using cross products

//      const Raz::Vec3f& topPos   = vertices[(j - 1) * m_width + i].position;
//      const Raz::Vec3f& leftPos  = vertices[depthStride + i - 1].position;
//      const Raz::Vec3f& rightPos = vertices[depthStride + i + 1].position;
//      const Raz::Vec3f& botPos   = vertices[(j + 1) * m_width + i].position;
//
//      //         topDir
//      //           ^
//      // leftDir <-|-> rightDir
//      //           v
//      //         botDir
//
//      Raz::Vertex& midVertex = vertices[depthStride + i];
//      const Raz::Vec3f topDir   = topPos - midVertex.position;
//      const Raz::Vec3f leftDir  = leftPos - midVertex.position;
//      const Raz::Vec3f rightDir = rightPos - midVertex.position;
//      const Raz::Vec3f botDir   = botPos - midVertex.position;
//
//      midVertex.normal   = rightDir.cross(topDir);
//      midVertex.normal  += topDir.cross(leftDir);
//      midVertex.normal  += leftDir.cross(botDir);
//      midVertex.normal  += botDir.cross(rightDir);
//      midVertex.normal   = midVertex.normal.normalize();
//      midVertex.tangent  = Raz::Vec3f(midVertex.normal.z(), midVertex.normal.x(), midVertex.normal.y());
    }
  }
}

void Terrain::remapVertices(float newHeightFactor, float newFlatness) {
  for (Raz::Vertex& vertex : m_mesh.getSubmeshes().front().getVertices()) {
    const float baseHeight = std::pow(vertex.position.y() / m_heightFactor, m_invFlatness);
    vertex.position.y() = std::pow(baseHeight, newFlatness) * newHeightFactor;
  }

  computeNormals();
  m_mesh.load();
}
