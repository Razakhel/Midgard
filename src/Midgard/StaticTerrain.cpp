#include "Midgard/StaticTerrain.hpp"

#include <RaZ/Entity.hpp>
#include <RaZ/Data/Mesh.hpp>
#include <RaZ/Math/MathUtils.hpp>
#include <RaZ/Math/PerlinNoise.hpp>
#include <RaZ/Render/MeshRenderer.hpp>
#include <RaZ/Utils/Threading.hpp>

namespace {

constexpr Raz::Vec3b waterColor(0, 0, 255);
constexpr Raz::Vec3b grassColor(62, 126, 0);
constexpr Raz::Vec3b groundColor(157, 110, 94);
constexpr Raz::Vec3b rockColor(127, 127, 127);
constexpr Raz::Vec3b snowColor(255, 255, 255);

} // namespace

StaticTerrain::StaticTerrain(Raz::Entity& entity, unsigned int width, unsigned int depth, float heightFactor, float flatness) : StaticTerrain(entity) {
  StaticTerrain::generate(width, depth, heightFactor, flatness);
}

void StaticTerrain::setParameters(float heightFactor, float flatness) {
  checkParameters(heightFactor, flatness);

  remapVertices(heightFactor, flatness);

  m_heightFactor = heightFactor;
  m_flatness     = flatness;
  m_invFlatness  = 1.f / flatness;
}

void StaticTerrain::generate(unsigned int width, unsigned int depth, float heightFactor, float flatness) {
  m_width = width;
  m_depth = depth;
  Terrain::setParameters(heightFactor, flatness);

  auto& mesh = m_entity.getComponent<Raz::Mesh>();
  mesh.getSubmeshes().resize(1);

  // Computing vertices

  std::vector<Raz::Vertex>& vertices = mesh.getSubmeshes().front().getVertices();
  vertices.resize(m_width * m_depth);

  Raz::Threading::parallelize(0, vertices.size(), [this, &vertices] (const Raz::Threading::IndexRange& range) noexcept {
    for (std::size_t i = range.beginIndex; i < range.endIndex; ++i) {
      const auto xCoord = static_cast<float>(i % m_width);
      const auto yCoord = static_cast<float>(i / m_width);

      // float noiseValue = Raz::PerlinNoise::compute2D(xCoord / 1000.f, yCoord / 1000.f, 8, true);
      // noiseValue       = Raz::PerlinNoise::compute2D(xCoord / 1000.f + noiseValue, yCoord / 1000.f + noiseValue, 8, true);
      // noiseValue       = Raz::PerlinNoise::compute2D(xCoord / 1000.f + noiseValue, yCoord / 1000.f + noiseValue, 8, true);

      float noiseValue = Raz::PerlinNoise::compute2D(xCoord / 100.f, yCoord / 100.f, 8, true);
      noiseValue       = std::pow(noiseValue, m_flatness);

      const Raz::Vec2f scaledCoords = (Raz::Vec2f(xCoord, yCoord) - static_cast<float>(m_width) * 0.5f) * 0.5f;

      Raz::Vertex& vertex = vertices[i];
      vertex.position     = Raz::Vec3f(scaledCoords.x(), noiseValue * m_heightFactor, scaledCoords.y());
      vertex.texcoords    = Raz::Vec2f(xCoord / static_cast<float>(m_width), yCoord / static_cast<float>(m_depth));
    }
  });

//  for (unsigned int j = 0; j < m_depth; ++j) {
//    const unsigned int depthIndex = j * m_depth;
//
//    for (unsigned int i = 0; i < m_width; ++i) {
//      Raz::Vec2f coords(static_cast<float>(i), static_cast<float>(j));
//
//      // float noiseValue = Raz::PerlinNoise::compute2D(coords.x() / 1000.f, coords.y() / 1000.f, 8, true);
//      // noiseValue       = Raz::PerlinNoise::compute2D(coords.x() / 1000.f + noiseValue, coords.y() / 1000.f + noiseValue, 8, true);
//      // noiseValue       = Raz::PerlinNoise::compute2D(coords.x() / 1000.f + noiseValue, coords.y() / 1000.f + noiseValue, 8, true);
//
//      float noiseValue = Raz::PerlinNoise::compute2D(coords.x() / 100.f, coords.y() / 100.f, 8, true);
//      noiseValue       = std::pow(noiseValue, flatness);
//
//      coords = (coords - static_cast<float>(m_width) / 2.f) / 2.f;
//
//      Raz::Vertex& vertex = vertices[depthIndex + i];
//      vertex.position     = Raz::Vec3f(coords.x(), noiseValue * heightFactor, coords.y());
//      vertex.texcoords    = Raz::Vec2f(static_cast<float>(i) / static_cast<float>(m_width), static_cast<float>(j) / static_cast<float>(m_depth));
//    }
//  }

  computeNormals();

  // Computing indices

  std::vector<unsigned int>& indices = mesh.getSubmeshes().front().getTriangleIndices();
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
      indices[finalIndex + 1] = (j + 1) * m_width + i;
      indices[finalIndex + 2] = j * m_width + i + 1;
      indices[finalIndex + 3] = j * m_width + i + 1;
      indices[finalIndex + 4] = (j + 1) * m_width + i;
      indices[finalIndex + 5] = (j + 1) * m_width + i + 1;
    }
  }

  m_entity.getComponent<Raz::MeshRenderer>().load(mesh);
}

const Raz::Image& StaticTerrain::computeColorMap() {
  m_colorMap = Raz::Image(m_width, m_depth, Raz::ImageColorspace::RGB);
  auto* imgData = static_cast<uint8_t*>(m_colorMap.getDataPtr());

  const std::vector<Raz::Vertex>& vertices = m_entity.getComponent<Raz::Mesh>().getSubmeshes().front().getVertices();

  Raz::Threading::parallelize(0, vertices.size(), [this, &vertices, imgData] (const Raz::Threading::IndexRange& range) noexcept {
    for (std::size_t i = range.beginIndex; i < range.endIndex; ++i) {
      const float noiseValue      = std::pow(vertices[i].position.y() / m_heightFactor, m_invFlatness);
      const Raz::Vec3b pixelValue = (noiseValue < 0.33f ? Raz::MathUtils::lerp(waterColor, grassColor, noiseValue * 3.f)
                                  : (noiseValue < 0.5f  ? Raz::MathUtils::lerp(grassColor, groundColor, (noiseValue - 0.33f) * 5.75f)
                                  : (noiseValue < 0.66f ? Raz::MathUtils::lerp(groundColor, rockColor, (noiseValue - 0.5f) * 6.f)
                                                        : Raz::MathUtils::lerp(rockColor, snowColor, (noiseValue - 0.66f) * 3.f))));

      const std::size_t dataStride = i * 3;
      imgData[dataStride]     = pixelValue.x();
      imgData[dataStride + 1] = pixelValue.y();
      imgData[dataStride + 2] = pixelValue.z();
    }
  });

  m_entity.getComponent<Raz::MeshRenderer>().getMaterials().front().getProgram().setTexture(Raz::Texture2D::create(m_colorMap),
                                                                                            Raz::MaterialTexture::BaseColor);

  return m_colorMap;
}

const Raz::Image& StaticTerrain::computeNormalMap() {
  m_normalMap = Raz::Image(m_width, m_depth, Raz::ImageColorspace::RGB);
  auto* imgData = static_cast<uint8_t*>(m_normalMap.getDataPtr());

  const std::vector<Raz::Vertex>& vertices = m_entity.getComponent<Raz::Mesh>().getSubmeshes().front().getVertices();

  Raz::Threading::parallelize(0, vertices.size(), [&vertices, imgData] (const Raz::Threading::IndexRange& range) noexcept {
    for (std::size_t i = range.beginIndex; i < range.endIndex; ++i) {
      const Raz::Vec3f& normal = vertices[i].normal;

      const std::size_t dataStride = i * 3;
      imgData[dataStride]     = static_cast<uint8_t>(std::max(0.f, normal.x()) * 255.f);
      imgData[dataStride + 1] = static_cast<uint8_t>(std::max(0.f, normal.y()) * 255.f);
      imgData[dataStride + 2] = static_cast<uint8_t>(std::max(0.f, normal.z()) * 255.f);
    }
  });

  return m_normalMap;
}

const Raz::Image& StaticTerrain::computeSlopeMap() {
  m_slopeMap    = Raz::Image(m_width, m_depth, Raz::ImageColorspace::RGB, Raz::ImageDataType::FLOAT);
  auto* imgData = static_cast<float*>(m_slopeMap.getDataPtr());

  const std::vector<Raz::Vertex>& vertices = m_entity.getComponent<Raz::Mesh>().getSubmeshes().front().getVertices();

  for (unsigned int j = 1; j < m_depth - 1; ++j) {
    const unsigned int depthStride = j * m_width * 3;

    for (unsigned int i = 1; i < m_width - 1; ++i) {
      const float topHeight   = vertices[(j - 1) * m_width + i].position.y();
      const float leftHeight  = vertices[j * m_width + i - 1].position.y();
      const float rightHeight = vertices[j * m_width + i + 1].position.y();
      const float botHeight   = vertices[(j + 1) * m_width + i].position.y();

      Raz::Vec2f slopeVec(leftHeight - rightHeight, topHeight - botHeight);
      const float slopeStrength = slopeVec.computeLength() * 0.5f;
      slopeVec = slopeVec.normalize();

      imgData[depthStride + i * 3    ] = slopeVec.x();
      imgData[depthStride + i * 3 + 1] = slopeVec.y();
      imgData[depthStride + i * 3 + 2] = slopeStrength;
    }
  }

  return m_slopeMap;
}

void StaticTerrain::computeNormals() {
  std::vector<Raz::Vertex>& vertices = m_entity.getComponent<Raz::Mesh>().getSubmeshes().front().getVertices();

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
//      Raz::Vertex& midVertex    = vertices[depthStride + i];
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

void StaticTerrain::remapVertices(float newHeightFactor, float newFlatness) {
  auto& mesh = m_entity.getComponent<Raz::Mesh>();

  std::vector<Raz::Vertex>& vertices = mesh.getSubmeshes().front().getVertices();
  Raz::Threading::parallelize(vertices, [this, newHeightFactor, newFlatness] (const auto& range) noexcept {
    for (Raz::Vertex& vertex : range) {
      const float baseHeight = std::pow(vertex.position.y() / m_heightFactor, m_invFlatness);
      vertex.position.y()    = std::pow(baseHeight, newFlatness) * newHeightFactor;
    }
  });

  computeNormals();
  m_entity.getComponent<Raz::MeshRenderer>().load(mesh);
}
