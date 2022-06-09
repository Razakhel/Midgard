#include "Midgard/Terrain.hpp"

#include <RaZ/Entity.hpp>
#include <RaZ/Data/Mesh.hpp>
#include <RaZ/Math/Transform.hpp>
#include <RaZ/Render/MeshRenderer.hpp>
#include <RaZ/Utils/Logger.hpp>

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

  meshRenderer.setMaterial(Raz::Material(Raz::MaterialType::COOK_TORRANCE)).getProgram().setAttribute(0.f, Raz::MaterialAttribute::Roughness);
}

void Terrain::setParameters(float heightFactor, float flatness) {
  checkParameters(heightFactor, flatness);

  m_heightFactor = heightFactor;
  m_flatness     = flatness;
  m_invFlatness  = 1.f / flatness;
}

void Terrain::checkParameters(float& heightFactor, float& flatness) {
  if (heightFactor <= 0.f) {
    Raz::Logger::warn("[Terrain] The height factor can't be 0 or negative; remapping to +epsilon.");
    heightFactor = std::numeric_limits<float>::epsilon();
  }

  if (flatness <= 0.f) {
    Raz::Logger::warn("[Terrain] The flatness can't be 0 or negative; remapping to +epsilon.");
    flatness = std::numeric_limits<float>::epsilon();
  }
}
