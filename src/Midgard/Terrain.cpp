#include "Midgard/Terrain.hpp"

#include <RaZ/Math/MathUtils.hpp>
#include <RaZ/Math/PerlinNoise.hpp>
#include <RaZ/Render/Mesh.hpp>

// Debug colors
constexpr Raz::Vec3b gradientDown(97, 130, 234);
constexpr Raz::Vec3b gradientMid(221, 220, 219);
constexpr Raz::Vec3b gradientHigh(220, 94, 75);

// Actual colors
constexpr Raz::Vec3b waterColor(0, 0, 255);
constexpr Raz::Vec3b grassColor(62, 126, 0);
constexpr Raz::Vec3b groundColor(157, 110, 94);
constexpr Raz::Vec3b rockColor(127, 127, 127);
constexpr Raz::Vec3b snowColor(255, 255, 255);

void Terrain::generate(unsigned int width, unsigned int height) {
  m_width  = width;
  m_height = height;

  Raz::Submesh& terrainSubmesh = m_mesh.getSubmeshes().front();

  // Computing vertices

  terrainSubmesh.getVertices().clear();
  terrainSubmesh.getVertices().reserve(m_width * m_height);

  for (unsigned int j = 0; j < m_height; ++j) {
    for (unsigned int i = 0; i < m_width; ++i) {
      Raz::Vec2f coords(static_cast<float>(i), static_cast<float>(j));
      const float noiseValue = Raz::PerlinNoise::get2D(coords.x() / 100.f, coords.y() / 100.f, 8, true);
      coords = (coords - static_cast<float>(m_width) / 2.f) / 2.f;

      terrainSubmesh.getVertices().emplace_back(Raz::Vertex{ Raz::Vec3f(coords.x(), noiseValue * 30.f, coords.y()),
                                                             Raz::Vec2f(static_cast<float>(i) / static_cast<float>(m_width),
                                                                        static_cast<float>(j) / static_cast<float>(m_height))});
    }
  }

  // Computing normals

  for (unsigned int j = 1; j < m_height - 1; ++j) {
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

      const float topHeight   = terrainSubmesh.getVertices()[(j - 1) * m_width + i].position.y();
      const float leftHeight  = terrainSubmesh.getVertices()[j * m_width + i - 1].position.y();
      const float rightHeight = terrainSubmesh.getVertices()[j * m_width + i + 1].position.y();
      const float botHeight   = terrainSubmesh.getVertices()[(j + 1) * m_width + i].position.y();

      Raz::Vertex& midVertex = terrainSubmesh.getVertices()[j * m_width + i];
      // TODO: check the Y coordinate: (maxDimension / coordDivider) * (m_heightFactor / coordDivider)?
      midVertex.normal  = Raz::Vec3f(leftHeight - rightHeight, 1.5f, topHeight - botHeight).normalize();
      midVertex.tangent = Raz::Vec3f(midVertex.normal.z(), midVertex.normal.x(), midVertex.normal.y());

      // Using cross products

//      const Raz::Vec3f& topPos   = terrainSubmesh.getVertices()[(j - 1) * m_width + i].position;
//      const Raz::Vec3f& leftPos  = terrainSubmesh.getVertices()[j * m_width + i - 1].position;
//      const Raz::Vec3f& rightPos = terrainSubmesh.getVertices()[j * m_width + i + 1].position;
//      const Raz::Vec3f& botPos   = terrainSubmesh.getVertices()[(j + 1) * m_width + i].position;
//
//      //         topDir
//      //           ^
//      // leftDir <-|-> rightDir
//      //           v
//      //         botDir
//
//      Raz::Vertex& midVertex = terrainSubmesh.getVertices()[j * m_width + i];
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

  // Computing indices

  terrainSubmesh.getTriangleIndices().clear();
  terrainSubmesh.getTriangleIndices().reserve(m_width * m_height * 6);

  for (unsigned int j = 0; j < m_height - 1; ++j) {
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

      terrainSubmesh.getTriangleIndices().emplace_back(j * m_width + i);
      terrainSubmesh.getTriangleIndices().emplace_back(j * m_width + i + 1);
      terrainSubmesh.getTriangleIndices().emplace_back((j + 1) * m_width + i);
      terrainSubmesh.getTriangleIndices().emplace_back((j + 1) * m_width + i);
      terrainSubmesh.getTriangleIndices().emplace_back(j * m_width + i + 1);
      terrainSubmesh.getTriangleIndices().emplace_back((j + 1) * m_width + i + 1);
    }
  }

  //terrainMesh.load();
}

void Terrain::computeTexture() const {
  Raz::Image img(m_width, m_height);
  auto* imgData = static_cast<uint8_t*>(img.getDataPtr());

  for (unsigned int j = 0; j < m_height; ++j) {
    for (unsigned int i = 0; i < m_width; ++i) {
      const float noiseValue = Raz::PerlinNoise::get2D(static_cast<float>(i) / 100.f, static_cast<float>(j) / 100.f, 8, true);
      const Raz::Vec3b pixelValue = (noiseValue < 0.33f ? Raz::MathUtils::lerp(waterColor, grassColor, noiseValue * 3.f)
                                  : (noiseValue < 0.5f  ? Raz::MathUtils::lerp(grassColor, groundColor, (noiseValue - 0.33f) * 5.75f)
                                  : (noiseValue < 0.66f ? Raz::MathUtils::lerp(groundColor, rockColor, (noiseValue - 0.5f) * 6.f)
                                                        : Raz::MathUtils::lerp(rockColor, snowColor, (noiseValue - 0.66f) * 3.f))));

//      const Raz::Vec3b pixelValue = (noiseValue < 0.5f ? Raz::MathUtils::lerp(gradientDown, gradientMid, noiseValue * 2.f)
//                                                       : Raz::MathUtils::lerp(gradientMid, gradientHigh, noiseValue * 2.f - 1.f));

      imgData[j * 3 * m_width + i * 3]     = pixelValue.x();
      imgData[j * 3 * m_width + i * 3 + 1] = pixelValue.y();
      imgData[j * 3 * m_width + i * 3 + 2] = pixelValue.z();
    }
  }

  img.save("terrainTest.png");
  m_mesh.getMaterials().front()->setBaseColorMap(Raz::Texture::create(std::move(img), 0));
}
