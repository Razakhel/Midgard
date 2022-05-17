#pragma once

#ifndef MIDGARD_STATICTERRAIN_HPP
#define MIDGARD_STATICTERRAIN_HPP

#include "Midgard/Terrain.hpp"

#include <RaZ/Data/Image.hpp>

class StaticTerrain : public Terrain {
public:
  explicit StaticTerrain(Raz::Entity& entity) : Terrain(entity) {}
  StaticTerrain(Raz::Entity& entity, unsigned int width, unsigned int depth, float heightFactor, float flatness);

  void setParameters(float heightFactor, float flatness) override;

  /// Generates a terrain as a static mesh.
  /// \param width Width of the terrain.
  /// \param depth Depth of the terrain.
  /// \param heightFactor Height factor to apply to vertices.
  /// \param flatness Flatness of the terrain.
  void generate(unsigned int width, unsigned int depth, float heightFactor, float flatness) override;
  const Raz::Image& computeColorMap();
  const Raz::Image& computeNormalMap();
  const Raz::Image& computeSlopeMap();

private:
  void computeNormals();
  void remapVertices(float newHeightFactor, float newFlatness);

  Raz::Image m_colorMap {};
  Raz::Image m_normalMap {};
  Raz::Image m_slopeMap {};
};

#endif // MIDGARD_STATICTERRAIN_HPP
