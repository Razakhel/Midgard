#include "Midgard/Terrain.hpp"

#include <RaZ/Entity.hpp>
#include <RaZ/Data/Mesh.hpp>
#include <RaZ/Math/MathUtils.hpp>
#include <RaZ/Math/PerlinNoise.hpp>
#include <RaZ/Math/Transform.hpp>
#include <RaZ/Render/MeshRenderer.hpp>
#include <RaZ/Utils/Logger.hpp>
#include <RaZ/Utils/Threading.hpp>

namespace {

constexpr Raz::Vec3b waterColor(0, 0, 255);
constexpr Raz::Vec3b grassColor(62, 126, 0);
constexpr Raz::Vec3b groundColor(157, 110, 94);
constexpr Raz::Vec3b rockColor(127, 127, 127);
constexpr Raz::Vec3b snowColor(255, 255, 255);

inline void checkParameters(float& heightFactor, float& flatness) {
  if (heightFactor <= 0.f) {
    Raz::Logger::warn("[Terrain] Height factor can't be 0 or negative; remapping to +epsilon.");
    heightFactor = std::numeric_limits<float>::epsilon();
  }

  if (flatness <= 0.f) {
    Raz::Logger::warn("[Terrain] Flatness can't be 0 or negative; remapping to +epsilon.");
    flatness = std::numeric_limits<float>::epsilon();
  }
}

} // namespace

Terrain::Terrain(Raz::Entity& entity) : m_entity{ entity } {
  if (!m_entity.hasComponent<Raz::Transform>())
    m_entity.addComponent<Raz::Transform>();

  if (!m_entity.hasComponent<Raz::Mesh>())
    m_entity.addComponent<Raz::Mesh>();

  if (!m_entity.hasComponent<Raz::MeshRenderer>())
    m_entity.addComponent<Raz::MeshRenderer>();

  auto& meshRenderer = m_entity.getComponent<Raz::MeshRenderer>();

  if (meshRenderer.getSubmeshRenderers().empty())
    meshRenderer.addSubmeshRenderer();

  Raz::Material& material = meshRenderer.setMaterial(Raz::Material(Raz::MaterialType::COOK_TORRANCE));
  material.setAttribute(0.f, "uniMaterial.metallicFactor");
  material.setAttribute(0.f, "uniMaterial.roughnessFactor");
}

void Terrain::setParameters(float heightFactor, float flatness) {
  checkParameters(heightFactor, flatness);

  remapVertices(heightFactor, flatness);

  m_heightFactor = heightFactor;
  m_flatness     = flatness;
  m_invFlatness  = 1.f / flatness;
}

void Terrain::generate(unsigned int width, unsigned int depth, float heightFactor, float flatness) {
  m_width = width;
  m_depth = depth;

  checkParameters(heightFactor, flatness);
  m_heightFactor = heightFactor;
  m_flatness     = flatness;
  m_invFlatness  = 1.f / flatness;

  auto& mesh = m_entity.getComponent<Raz::Mesh>();
  mesh.getSubmeshes().resize(1);

  // Computing vertices

  std::vector<Raz::Vertex>& vertices = mesh.getSubmeshes().front().getVertices();
  vertices.resize(m_width * m_depth);

  Raz::Threading::parallelize(vertices, [this, &vertices] (const Raz::Threading::IndexRange& range) noexcept {
    for (std::size_t i = range.beginIndex; i < range.endIndex; ++i) {
      const auto xCoord = static_cast<float>(i % m_width);
      const auto yCoord = static_cast<float>(i / m_width);

      float noiseValue = Raz::PerlinNoise::get2D(xCoord / 100.f, yCoord / 100.f, 8, true);
      noiseValue       = std::pow(noiseValue, m_flatness);

      const Raz::Vec2f scaledCoords = (Raz::Vec2f(xCoord, yCoord) - static_cast<float>(m_width) * 0.5f) * 0.5f;

      Raz::Vertex& vertex = vertices[i];
      vertex.position     = Raz::Vec3f(scaledCoords.x(), noiseValue * m_heightFactor, scaledCoords.y());
      vertex.texcoords    = Raz::Vec2f(xCoord / static_cast<float>(m_width), yCoord / static_cast<float>(m_depth));
    }
  });

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

const Raz::Image& Terrain::computeColorMap() {
  m_colorMap = Raz::Image(m_width, m_depth, Raz::ImageColorspace::RGB);
  auto* imgData = static_cast<uint8_t*>(m_colorMap.getDataPtr());

  const std::vector<Raz::Vertex>& vertices = m_entity.getComponent<Raz::Mesh>().getSubmeshes().front().getVertices();

  Raz::Threading::parallelize(vertices, [this, &vertices, imgData] (const Raz::Threading::IndexRange& range) noexcept {
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

  Raz::Material& material = m_entity.getComponent<Raz::MeshRenderer>().getMaterials().front();
  material.setTexture(Raz::Texture::create(m_colorMap), "uniMaterial.baseColorMap");

  return m_colorMap;
}

const Raz::Image& Terrain::computeNormalMap() {
  m_normalMap = Raz::Image(m_width, m_depth, Raz::ImageColorspace::RGB);
  auto* imgData = static_cast<uint8_t*>(m_normalMap.getDataPtr());

  const std::vector<Raz::Vertex>& vertices = m_entity.getComponent<Raz::Mesh>().getSubmeshes().front().getVertices();

  Raz::Threading::parallelize(vertices, [&vertices, imgData] (const Raz::Threading::IndexRange& range) noexcept {
    for (std::size_t i = range.beginIndex; i < range.endIndex; ++i) {
      const Raz::Vec3f& normal = vertices[i].normal;

      const std::size_t dataStride = i * 3;
      imgData[dataStride]     = static_cast<uint8_t>(normal.x() * 255.f);
      imgData[dataStride + 1] = static_cast<uint8_t>(normal.y() * 255.f);
      imgData[dataStride + 2] = static_cast<uint8_t>(normal.z() * 255.f);
    }
  });

  return m_normalMap;
}

const Raz::Image& Terrain::computeSlopeMap() {
  m_slopeMap    = Raz::Image(m_width, m_depth, Raz::ImageColorspace::GRAY, Raz::ImageDataType::FLOAT);
  auto* imgData = static_cast<float*>(m_slopeMap.getDataPtr());

  const std::vector<Raz::Vertex>& vertices = m_entity.getComponent<Raz::Mesh>().getSubmeshes().front().getVertices();

  for (unsigned int j = 1; j < m_depth - 1; ++j) {
    const unsigned int depthStride = j * m_width;

    for (unsigned int i = 1; i < m_width - 1; ++i) {
      const float topHeight   = vertices[(j - 1) * m_width + i].position.y();
      const float leftHeight  = vertices[depthStride + i - 1].position.y();
      const float rightHeight = vertices[depthStride + i + 1].position.y();
      const float botHeight   = vertices[(j + 1) * m_width + i].position.y();

      const float length       = Raz::Vec2f(leftHeight - rightHeight, topHeight - botHeight).computeLength();
      imgData[depthStride + i] = length * 0.5f;
    }
  }

  return m_slopeMap;
}

void Terrain::computeNormals() {
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

void Terrain::remapVertices(float newHeightFactor, float newFlatness) {
  auto& mesh = m_entity.getComponent<Raz::Mesh>();

  std::vector<Raz::Vertex>& vertices = mesh.getSubmeshes().front().getVertices();
  Raz::Threading::parallelize(vertices, [this, newHeightFactor, newFlatness] (const Raz::Threading::IterRange<std::vector<Raz::Vertex>>& range) noexcept {
    for (Raz::Vertex& vertex : range) {
      const float baseHeight = std::pow(vertex.position.y() / m_heightFactor, m_invFlatness);
      vertex.position.y()    = std::pow(baseHeight, newFlatness) * newHeightFactor;
    }
  });

  computeNormals();
  m_entity.getComponent<Raz::MeshRenderer>().load(mesh);
}
