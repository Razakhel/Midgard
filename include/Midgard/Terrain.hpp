#pragma once

#ifndef MIDGARD_TERRAIN_HPP
#define MIDGARD_TERRAIN_HPP

#include <RaZ/Utils/Image.hpp>

namespace Raz { class Entity; }

class Terrain {
public:
  explicit Terrain(Raz::Entity& entity);
  Terrain(Raz::Entity& entity, unsigned int width, unsigned int height, float heightFactor, float flatness)
    : Terrain(entity) { generate(width, height, heightFactor, flatness); }
  Terrain(const Terrain&) = delete;
  Terrain(Terrain&&) noexcept = default;

  void setHeightFactor(float heightFactor) { setParameters(heightFactor, m_flatness); }
  void setFlatness(float flatness) { setParameters(m_heightFactor, flatness); }
  void setParameters(float heightFactor, float flatness);

  void generate(unsigned int width, unsigned int height, float heightFactor, float flatness);
  const Raz::Image& computeColorMap();
  const Raz::Image& computeNormalMap();
  const Raz::Image& computeSlopeMap();

  Terrain& operator=(const Terrain&) = delete;
  Terrain& operator=(Terrain&&) noexcept = delete;

private:
  void computeNormals();
  void remapVertices(float newHeightFactor, float newFlatness);

  Raz::Entity& m_entity;

  unsigned int m_width {};
  unsigned int m_depth {};
  float m_heightFactor {};
  float m_flatness {};
  float m_invFlatness {};

  Raz::Image m_colorMap {};
  Raz::Image m_normalMap {};
  Raz::Image m_slopeMap {};
};

#endif // MIDGARD_TERRAIN_HPP
