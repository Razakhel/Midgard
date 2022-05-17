#pragma once

#ifndef MIDGARD_TERRAIN_HPP
#define MIDGARD_TERRAIN_HPP

namespace Raz { class Entity; }

class Terrain {
public:
  explicit Terrain(Raz::Entity& entity);
  Terrain(const Terrain&) = delete;
  Terrain(Terrain&&) noexcept = default;

  void setHeightFactor(float heightFactor) { setParameters(heightFactor, m_flatness); }
  void setFlatness(float flatness) { setParameters(m_heightFactor, flatness); }
  virtual void setParameters(float heightFactor, float flatness);

  virtual void generate(unsigned int width, unsigned int depth, float heightFactor, float flatness) = 0;

  Terrain& operator=(const Terrain&) = delete;
  Terrain& operator=(Terrain&&) noexcept = delete;

  virtual ~Terrain() = default;

protected:
  static void checkParameters(float& heightFactor, float& flatness);

  Raz::Entity& m_entity;

  unsigned int m_width {};
  unsigned int m_depth {};
  float m_heightFactor {};
  float m_flatness {};
  float m_invFlatness {};
};

#endif // MIDGARD_TERRAIN_HPP
