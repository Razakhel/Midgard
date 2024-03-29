#pragma once

#ifndef MIDGARD_DYNAMICTERRAIN_HPP
#define MIDGARD_DYNAMICTERRAIN_HPP

#include "Midgard/Terrain.hpp"

#include <RaZ/Render/ShaderProgram.hpp>
#include <RaZ/Render/Texture.hpp>

class DynamicTerrain final : public Terrain {
public:
  explicit DynamicTerrain(Raz::Entity& entity);
  DynamicTerrain(Raz::Entity& entity, unsigned int width, unsigned int depth, float heightFactor, float flatness, float minTessLevel = 12.f);

  const Raz::Texture2D& getNoiseMap() const noexcept { return *m_noiseMap; }
  const Raz::Texture2D& getColorMap() const noexcept { return *m_colorMap; }
  const Raz::Texture2D& getSlopeMap() const noexcept { return *m_slopeMap; }

  void setMinTessellationLevel(float minTessLevel) { setParameters(minTessLevel, m_heightFactor, m_flatness); }
  void setParameters(float heightFactor, float flatness) override { setParameters(m_minTessLevel, heightFactor, flatness); }
  void setParameters(float minTessLevel, float heightFactor, float flatness);

  /// Generates a dynamic terrain using tessellation shaders.
  /// \param width Width of the terrain.
  /// \param depth Depth of the terrain.
  /// \param heightFactor Height factor to apply to vertices.
  /// \param flatness Flatness of the terrain.
  void generate(unsigned int width, unsigned int depth, float heightFactor, float flatness) override { generate(width, depth, heightFactor, flatness, 12.f); }
  /// Generates a dynamic terrain using tessellation shaders.
  /// \param width Width of the terrain.
  /// \param depth Depth of the terrain.
  /// \param heightFactor Height factor to apply to vertices.
  /// \param flatness Flatness of the terrain.
  /// \param minTessLevel Minimal tessellation level to render the terrain with.
  void generate(unsigned int width, unsigned int depth, float heightFactor, float flatness, float minTessLevel);
  const Raz::Texture2D& computeNoiseMap(float factor);
  const Raz::Texture2D& computeColorMap();
  const Raz::Texture2D& computeSlopeMap();

private:
  float m_minTessLevel {};

  Raz::ComputeShaderProgram m_noiseProgram {};
  Raz::ComputeShaderProgram m_colorProgram {};
  Raz::ComputeShaderProgram m_slopeProgram {};

  Raz::Texture2DPtr m_noiseMap {};
  Raz::Texture2DPtr m_colorMap {};
  Raz::Texture2DPtr m_slopeMap {};
};

#endif // MIDGARD_DYNAMICTERRAIN_HPP
