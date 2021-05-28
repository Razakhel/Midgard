#pragma once

#ifndef MIDGARD_TERRAIN_HPP
#define MIDGARD_TERRAIN_HPP

#include <RaZ/Utils/Image.hpp>

namespace Raz { class Mesh; }

class Terrain {
public:
  explicit Terrain(Raz::Mesh& mesh) : m_mesh{ mesh } {}
  Terrain(Raz::Mesh& mesh, unsigned int width, unsigned int height, float heightFactor) : Terrain(mesh) { generate(width, height, heightFactor); }

  void generate(unsigned int width, unsigned int height, float heightFactor);
  void computeTexture() const;
  const Raz::Image& computeSlopeMap();

private:
  Raz::Mesh& m_mesh;

  unsigned int m_width {};
  unsigned int m_depth {};
  float m_heightFactor {};

  Raz::Image m_slopeMap {};
};

#endif // MIDGARD_TERRAIN_HPP
