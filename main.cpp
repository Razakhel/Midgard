#include "Midgard/Terrain.hpp"

#include <RaZ/Application.hpp>
#include <RaZ/Math/Transform.hpp>
#include <RaZ/Render/Light.hpp>
#include <RaZ/Render/RenderSystem.hpp>
#include <RaZ/Utils/Logger.hpp>

using namespace std::literals;
using namespace Raz::Literals;

constexpr unsigned int terrainWidth  = 512;
constexpr unsigned int terrainHeight = 512;

int main() {
  try {
    ////////////////////
    // Initialization //
    ////////////////////

    Raz::Application app;
    Raz::World& world = app.addWorld(3);

    Raz::Logger::setLoggingLevel(Raz::LoggingLevel::ALL);

    ///////////////
    // Rendering //
    ///////////////

    auto& renderSystem = world.addSystem<Raz::RenderSystem>(1280u, 720u, "Midgard", Raz::WindowSetting::DEFAULT, 2);

    Raz::RenderPass& geometryPass = renderSystem.getGeometryPass();
    geometryPass.getProgram().setShaders(Raz::VertexShader(RAZ_ROOT + "shaders/common.vert"s),
                                         Raz::FragmentShader(RAZ_ROOT + "shaders/cook-torrance.frag"s));

    Raz::Window& window = renderSystem.getWindow();

    // Allowing to quit the application with the Escape key
    window.addKeyCallback(Raz::Keyboard::ESCAPE, [&app] (float /* deltaTime */) noexcept { app.quit(); });

    ///////////////////
    // Camera entity //
    ///////////////////

    Raz::Entity& camera = world.addEntity();
    auto& cameraComp    = camera.addComponent<Raz::Camera>(window.getWidth(), window.getHeight(), 45_deg, 0.1f, 1000.f);
    auto& cameraTrans   = camera.addComponent<Raz::Transform>(Raz::Vec3f(0.f, 30.f, 120.f));

    /////////
    // Sun //
    /////////

    Raz::Entity& light = world.addEntity();
    light.addComponent<Raz::Light>(Raz::LightType::DIRECTIONAL,            // Type
                                   Raz::Vec3f(0.f, -1.f, 1.f).normalize(), // Direction
                                   3.f,                                    // Energy
                                   Raz::Vec3f(1.f));                       // Color (RGB)
    light.addComponent<Raz::Transform>();

    //////////////
    // Fog pass //
    //////////////

    Raz::RenderGraph& renderGraph = renderSystem.getRenderGraph();

    const Raz::Texture& depthBuffer = renderGraph.addTextureBuffer(window.getWidth(), window.getHeight(), Raz::ImageColorspace::DEPTH);
    const Raz::Texture& colorBuffer = renderGraph.addTextureBuffer(window.getWidth(), window.getHeight(), Raz::ImageColorspace::RGB);

    geometryPass.addWriteTexture(depthBuffer);
    geometryPass.addWriteTexture(colorBuffer);

    Raz::RenderPass& fogPass = renderGraph.addNode(Raz::FragmentShader(MIDGARD_ROOT + "shaders/fog.frag"s));

    fogPass.addReadTexture(depthBuffer, "uniSceneBuffers.depth");
    fogPass.addReadTexture(colorBuffer, "uniSceneBuffers.color");
    fogPass.getProgram().sendUniform("uniSunDir", light.getComponent<Raz::Light>().getDirection());
    fogPass.getProgram().sendUniform("uniFogDensity", 0.1f);

    geometryPass.addChildren(fogPass);

    /////////////
    // Terrain //
    /////////////

    Terrain terrain(world.addEntity(), terrainWidth, terrainHeight, 30.f, 3.f);

    const Raz::Image& colorMap = terrain.computeColorMap();
    colorMap.save("colorMap.png");

    const Raz::Image& normalMap = terrain.computeNormalMap();
    normalMap.save("normalMap.png");

    const Raz::Image& slopeMap = terrain.computeSlopeMap();
    slopeMap.save("slopeMap.png");

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
      const float moveVal = (-10.f * deltaTime) * cameraSpeed;

      cameraTrans.move(0.f, 0.f, moveVal);
      cameraComp.setOrthoBoundX(cameraComp.getOrthoBoundX() + moveVal);
      cameraComp.setOrthoBoundY(cameraComp.getOrthoBoundY() + moveVal);
    });
    window.addKeyCallback(Raz::Keyboard::S, [&cameraTrans, &cameraComp, &cameraSpeed] (float deltaTime) {
      const float moveVal = (10.f * deltaTime) * cameraSpeed;

      cameraTrans.move(0.f, 0.f, moveVal);
      cameraComp.setOrthoBoundX(cameraComp.getOrthoBoundX() + moveVal);
      cameraComp.setOrthoBoundY(cameraComp.getOrthoBoundY() + moveVal);
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
    }, Raz::Input::ONCE, [&cameraLocked, &window] () {
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

    /////////////
    // Overlay //
    /////////////

    Raz::OverlayWindow& overlay = window.getOverlay().addWindow("Midgard", Raz::Vec2f(-1.f));

    overlay.addLabel("Press WASD to fly the camera around,");
    overlay.addLabel("Space/V to go up/down,");
    overlay.addLabel("& Shift to move faster.");
    overlay.addLabel("Hold the right mouse button to rotate the camera.");

    overlay.addSeparator();

    Raz::Texture colorTexture(colorMap, 0);
    Raz::Texture normalTexture(normalMap, 1);
    Raz::Texture slopeTexture(slopeMap, 2);

    overlay.addTexture(colorTexture, 150, 150);
    overlay.addTexture(normalTexture, 150, 150);
    overlay.addTexture(slopeTexture, 150, 150);

    overlay.addSeparator();

    overlay.addSlider("Height factor", [&terrain, &normalTexture, &slopeTexture] (float value) {
      terrain.setHeightFactor(value);
      normalTexture.load(terrain.computeNormalMap());
      slopeTexture.load(terrain.computeSlopeMap());
    }, 0.001f, 50.f, 30.f);

    overlay.addSlider("Flatness", [&terrain, &normalTexture, &slopeTexture] (float value) {
      terrain.setFlatness(value);
      normalTexture.load(terrain.computeNormalMap());
      slopeTexture.load(terrain.computeSlopeMap());
    }, 1.f, 10.f, 3.f);

    overlay.addSlider("Fog density", [&fogPass] (float value) {
      fogPass.getProgram().sendUniform("uniFogDensity", value);
    }, 0.f, 1.f, 0.1f);

    overlay.addSeparator();

    overlay.addFrameTime("Frame time: %.3f ms/frame");
    overlay.addFpsCounter("FPS: %.1f");

    //////////////////////////
    // Starting application //
    //////////////////////////

    app.run();
  } catch (const std::exception& exception) {
    Raz::Logger::error(exception.what());
  }

  return EXIT_SUCCESS;
}
