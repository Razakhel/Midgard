#include <RaZ/Application.hpp>
#include <RaZ/Math/PerlinNoise.hpp>
#include <RaZ/Math/Transform.hpp>
#include <RaZ/Render/Light.hpp>
#include <RaZ/Render/RenderSystem.hpp>

using namespace std::literals;
using namespace Raz::Literals;

constexpr int terrainWidth  = 512;
constexpr int terrainHeight = 512;

int main() {
  ////////////////////
  // Initialization //
  ////////////////////

  Raz::Application app;
  Raz::World& world = app.addWorld(3);

  ///////////////
  // Rendering //
  ///////////////

  auto& renderSystem = world.addSystem<Raz::RenderSystem>(1280, 720, "Midgard", Raz::WindowSetting::DEFAULT, 2);

  Raz::RenderPass& geometryPass = renderSystem.getGeometryPass();
  geometryPass.getProgram().setShaders(Raz::VertexShader(RAZ_ROOT + "shaders/common.vert"s),
                                       Raz::FragmentShader(RAZ_ROOT + "shaders/cook-torrance.frag"s));

  Raz::Window& window = renderSystem.getWindow();

#if !defined(USE_OPENGL_ES)
  // Allow wireframe toggling
  bool isWireframe = false;
  window.addKeyCallback(Raz::Keyboard::Z, [&isWireframe] (float /* deltaTime */) {
    isWireframe = !isWireframe;
    Raz::Renderer::setPolygonMode(Raz::FaceOrientation::FRONT_BACK, (isWireframe ? Raz::PolygonMode::LINE : Raz::PolygonMode::FILL));
  }, Raz::Input::ONCE);
#endif

  // Allowing to quit the application with the Escape key
  window.addKeyCallback(Raz::Keyboard::ESCAPE, [&app] (float /* deltaTime */) { app.quit(); });

  ///////////////////
  // Camera entity //
  ///////////////////

  Raz::Entity& camera = world.addEntity();
  auto& cameraComp    = camera.addComponent<Raz::Camera>(window.getWidth(), window.getHeight(), 45_deg, 0.1f, 1000.f);
  auto& cameraTrans   = camera.addComponent<Raz::Transform>(Raz::Vec3f(0.f, 30.f, -120.f));

  /////////////
  // Terrain //
  /////////////

  Raz::Entity& terrain = world.addEntity();
  auto& meshComp       = terrain.addComponent<Raz::Mesh>();
  terrain.addComponent<Raz::Transform>();

  Raz::Submesh& terrainSubmesh = meshComp.getSubmeshes().front();
  terrainSubmesh.getVertices().reserve(terrainWidth * terrainHeight);

  for (unsigned int j = 0; j < terrainHeight; ++j) {
    for (unsigned int i = 0; i < terrainWidth; ++i) {
      Raz::Vec2f coords(static_cast<float>(i), static_cast<float>(j));
      const float noiseValue = Raz::PerlinNoise::get2D(coords.x() / 100.f, coords.y() / 100.f, 8, true);
      coords = (coords - terrainWidth / 2.f) / 2.f;

      terrainSubmesh.getVertices().emplace_back(Raz::Vertex{ Raz::Vec3f(coords.x(), noiseValue * 30.f, coords.y()),
                                                             Raz::Vec2f(static_cast<float>(i) / terrainWidth, static_cast<float>(j) / terrainHeight)});
    }
  }

  // Computing normals
  for (unsigned int j = 1; j < terrainHeight - 1; ++j) {
    for (unsigned int i = 1; i < terrainWidth - 1; ++i) {
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

      const float topHeight   = terrainSubmesh.getVertices()[(j - 1) * terrainWidth + i].position.y();
      const float leftHeight  = terrainSubmesh.getVertices()[j * terrainWidth + i - 1].position.y();
      const float rightHeight = terrainSubmesh.getVertices()[j * terrainWidth + i + 1].position.y();
      const float botHeight   = terrainSubmesh.getVertices()[(j + 1) * terrainWidth + i].position.y();

      Raz::Vertex& midVertex = terrainSubmesh.getVertices()[j * terrainWidth + i];
      // TODO: check the Y coordinate: (maxDimension / coordDivider) * (heightFactor / coordDivider)?
      midVertex.normal  = Raz::Vec3f(leftHeight - rightHeight, 1.5f, topHeight - botHeight).normalize();
      midVertex.tangent = Raz::Vec3f(midVertex.normal.z(), midVertex.normal.x(), midVertex.normal.y());

      // Using cross products

//      const Raz::Vec3f& topPos   = terrainSubmesh.getVertices()[(j - 1) * terrainWidth + i].position;
//      const Raz::Vec3f& leftPos  = terrainSubmesh.getVertices()[j * terrainWidth + i - 1].position;
//      const Raz::Vec3f& rightPos = terrainSubmesh.getVertices()[j * terrainWidth + i + 1].position;
//      const Raz::Vec3f& botPos   = terrainSubmesh.getVertices()[(j + 1) * terrainWidth + i].position;
//
//      //         topDir
//      //           ^
//      // leftDir <-|-> rightDir
//      //           v
//      //         botDir
//
//      Raz::Vertex& midVertex = terrainSubmesh.getVertices()[j * terrainWidth + i];
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

  terrainSubmesh.getTriangleIndices().reserve(terrainWidth * terrainHeight * 6);

  for (unsigned int j = 0; j < terrainHeight - 1; ++j) {
    for (unsigned int i = 0; i < terrainWidth - 1; ++i) {
      //     i     i + 1
      //     v       v
      //     --------- <- j * terrainWidth
      //     |      /|
      //     |    /  |
      //     |  /    |
      //     |/______| <- (j + 1) * terrainWidth
      //     ^       ^
      //  j + 1    j + 1 + i + 1

      terrainSubmesh.getTriangleIndices().emplace_back(j * terrainWidth + i);
      terrainSubmesh.getTriangleIndices().emplace_back(j * terrainWidth + i + 1);
      terrainSubmesh.getTriangleIndices().emplace_back((j + 1) * terrainWidth + i);
      terrainSubmesh.getTriangleIndices().emplace_back((j + 1) * terrainWidth + i);
      terrainSubmesh.getTriangleIndices().emplace_back(j * terrainWidth + i + 1);
      terrainSubmesh.getTriangleIndices().emplace_back((j + 1) * terrainWidth + i + 1);
    }
  }

  //meshComp.load();

  /////////
  // Sun //
  /////////

  Raz::Entity& light = world.addEntity();
  light.addComponent<Raz::Light>(Raz::LightType::DIRECTIONAL,             // Type
                                 Raz::Vec3f(0.f, -1.f, -1.f).normalize(), // Direction
                                 1.f,                                     // Energy
                                 Raz::Vec3f(1.f));                        // Color (RGB)
  light.addComponent<Raz::Transform>();

  /////////////////////
  // Camera controls //
  /////////////////////

  float cameraSpeed = 1.f;
  window.addKeyCallback(Raz::Keyboard::LEFT_SHIFT,
                        [&cameraSpeed] (float /* deltaTime */) noexcept { cameraSpeed = 2.f; },
                        Raz::Input::ONCE,
                        [&cameraSpeed] () noexcept { cameraSpeed = 1.f; });
  window.addKeyCallback(Raz::Keyboard::SPACE, [&cameraTrans, &cameraSpeed] (float deltaTime) {
    cameraTrans.move(0.f, (10.f * deltaTime) * cameraSpeed, 0.f);
  });
  window.addKeyCallback(Raz::Keyboard::V, [&cameraTrans, &cameraSpeed] (float deltaTime) {
    cameraTrans.move(0.f, (-10.f * deltaTime) * cameraSpeed, 0.f);
  });
  window.addKeyCallback(Raz::Keyboard::W, [&cameraTrans, &cameraComp, &cameraSpeed] (float deltaTime) {
    const float moveVal = (10.f * deltaTime) * cameraSpeed;

    cameraTrans.move(0.f, 0.f, moveVal);
    cameraComp.setOrthoBoundX(cameraComp.getOrthoBoundX() - moveVal);
    cameraComp.setOrthoBoundY(cameraComp.getOrthoBoundY() - moveVal);
  });
  window.addKeyCallback(Raz::Keyboard::S, [&cameraTrans, &cameraComp, &cameraSpeed] (float deltaTime) {
    const float moveVal = (-10.f * deltaTime) * cameraSpeed;

    cameraTrans.move(0.f, 0.f, moveVal);
    cameraComp.setOrthoBoundX(cameraComp.getOrthoBoundX() - moveVal);
    cameraComp.setOrthoBoundY(cameraComp.getOrthoBoundY() - moveVal);
  });
  window.addKeyCallback(Raz::Keyboard::A, [&cameraTrans, &cameraSpeed] (float deltaTime) {
    cameraTrans.move((-10.f * deltaTime) * cameraSpeed, 0.f, 0.f);
  });
  window.addKeyCallback(Raz::Keyboard::D, [&cameraTrans, &cameraSpeed] (float deltaTime) {
    cameraTrans.move((10.f * deltaTime) * cameraSpeed, 0.f, 0.f);
  });

  window.addMouseScrollCallback([&cameraComp] (double /* xOffset */, double yOffset) {
    const float newFov = Raz::Degreesf(cameraComp.getFieldOfView()).value + static_cast<float>(-yOffset) * 2.f;
    cameraComp.setFieldOfView(Raz::Degreesf(std::clamp(newFov, 15.f, 90.f)));
  });

  bool cameraLocked = true; // To allow moving the camera using the mouse

  window.addMouseButtonCallback(Raz::Mouse::RIGHT_CLICK, [&cameraLocked, &window] (float) {
    cameraLocked = false;
    window.changeCursorState(Raz::Cursor::DISABLED);
  }, Raz::Input::ALWAYS, [&cameraLocked, &window] () {
    cameraLocked = true;
    window.changeCursorState(Raz::Cursor::NORMAL);
  });

  window.addMouseMoveCallback([&cameraLocked, &cameraTrans, &window] (double xMove, double yMove) {
    if (cameraLocked)
      return;

    // Dividing move by window size to scale between -1 and 1
    cameraTrans.rotate(-90_deg * yMove / window.getHeight(),
                       -90_deg * xMove / window.getWidth());
  });

  //////////////////////////
  // Starting application //
  //////////////////////////

  app.run();

  return EXIT_SUCCESS;
}
