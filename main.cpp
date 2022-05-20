#include "Midgard/StaticTerrain.hpp"
#if !defined(USE_OPENGL_ES)
#include "Midgard/DynamicTerrain.hpp"
#endif

#include <RaZ/Application.hpp>
#include <RaZ/Data/ImageFormat.hpp>
#include <RaZ/Math/Transform.hpp>
#include <RaZ/Render/Light.hpp>
#include <RaZ/Render/MeshRenderer.hpp>
#include <RaZ/Render/RenderSystem.hpp>
#include <RaZ/Utils/Logger.hpp>

using namespace Raz::Literals;

namespace {

constexpr unsigned int terrainWidth = 512;
constexpr unsigned int terrainDepth = 512;

constexpr std::string_view fogShaderSource = {
#include "fog.frag.embed"
};

} // namespace

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

    auto& renderSystem  = world.addSystem<Raz::RenderSystem>(1280u, 720u, "Midgard", Raz::WindowSetting::DEFAULT, 2);
    Raz::Window& window = renderSystem.getWindow();

    // Allowing to quit the application with the Escape key
    window.addKeyCallback(Raz::Keyboard::ESCAPE, [&app] (float /* deltaTime */) noexcept { app.quit(); });

    // Allowing to quit the application when the close button is clicked
    window.setCloseCallback([&app] () noexcept { app.quit(); });

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
    light.addComponent<Raz::Light>(Raz::LightType::DIRECTIONAL,             // Type
                                   Raz::Vec3f(0.f, -1.f, -1.f).normalize(), // Direction
                                   3.f,                                     // Energy
                                   Raz::Vec3f(1.f));                        // Color (RGB)
    light.addComponent<Raz::Transform>();

    //////////////
    // Fog pass //
    //////////////

    Raz::RenderGraph& renderGraph = renderSystem.getRenderGraph();
    Raz::RenderPass& geometryPass = renderGraph.getGeometryPass();

    const Raz::TexturePtr depthBuffer = Raz::Texture::create(window.getWidth(), window.getHeight(), Raz::ImageColorspace::DEPTH);
    const Raz::TexturePtr colorBuffer = Raz::Texture::create(window.getWidth(), window.getHeight(), Raz::ImageColorspace::RGB);

    geometryPass.addWriteTexture(depthBuffer);
    geometryPass.addWriteTexture(colorBuffer);

    Raz::RenderPass& fogPass = renderGraph.addNode(Raz::FragmentShader::loadFromSource(fogShaderSource));

    fogPass.addReadTexture(depthBuffer, "uniSceneBuffers.depth");
    fogPass.addReadTexture(colorBuffer, "uniSceneBuffers.color");
    fogPass.getProgram().sendUniform("uniSunDir", light.getComponent<Raz::Light>().getDirection());
    fogPass.getProgram().sendUniform("uniFogDensity", 0.1f);

    geometryPass.addChildren(fogPass);

    /////////////
    // Terrain //
    /////////////

#if !defined(USE_OPENGL_ES)
      Raz::Entity& dynamicTerrainEntity = world.addEntity();
      DynamicTerrain dynamicTerrain(dynamicTerrainEntity, terrainWidth, terrainDepth, 30.f, 3.f);
#endif

    Raz::Entity& staticTerrainEntity = world.addEntity();
    StaticTerrain staticTerrain(staticTerrainEntity, terrainWidth, terrainDepth, 30.f, 3.f);

    const Raz::Image& colorMap  = staticTerrain.computeColorMap();
    const Raz::Image& normalMap = staticTerrain.computeNormalMap();
    const Raz::Image& slopeMap  = staticTerrain.computeSlopeMap();

    Raz::ImageFormat::save("colorMap.png", colorMap);
    Raz::ImageFormat::save("normalMap.png", normalMap);
    Raz::ImageFormat::save("slopeMap.png", slopeMap);

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

    window.setMouseScrollCallback([&cameraComp] (double /* xOffset */, double yOffset) {
      const float newFov = Raz::Degreesf(cameraComp.getFieldOfView()).value + static_cast<float>(-yOffset) * 2.f;
      cameraComp.setFieldOfView(Raz::Degreesf(std::clamp(newFov, 15.f, 90.f)));
    });

    bool cameraLocked = true; // To allow moving the camera using the mouse

    window.addMouseButtonCallback(Raz::Mouse::RIGHT_CLICK, [&cameraLocked, &window] (float) {
      cameraLocked = false;
      window.setCursorState(Raz::Cursor::DISABLED);
    }, Raz::Input::ONCE, [&cameraLocked, &window] () {
      cameraLocked = true;
      window.setCursorState(Raz::Cursor::NORMAL);
    });

    window.setMouseMoveCallback([&cameraLocked, &cameraTrans, &window] (double xMove, double yMove) {
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

#if !defined(USE_OPENGL_ES)
    Raz::OverlayTexture& dynamicNoiseTexture = overlay.addTexture(*dynamicTerrain.getNoiseMap(), 150, 150);
#endif

    Raz::Texture colorTexture(colorMap, false);
    Raz::Texture normalTexture(normalMap, false);
    Raz::Texture slopeTexture(slopeMap, false);

    Raz::OverlayTexture& staticColorTexture  = overlay.addTexture(colorTexture, 150, 150);
    Raz::OverlayTexture& staticNormalTexture = overlay.addTexture(normalTexture, 150, 150);
    Raz::OverlayTexture& staticSlopeTexture  = overlay.addTexture(slopeTexture, 150, 150);

    overlay.addSeparator();

#if !defined(USE_OPENGL_ES)
    Raz::OverlaySlider& dynamicNoiseMapFactorSlider = overlay.addSlider("Noise map factor", [&dynamicTerrain] (float value) {
      dynamicTerrain.computeNoiseMap(value);
    }, 0.001f, 0.1f, 0.01f);

    Raz::OverlaySlider& dynamicMinTessLevelSlider = overlay.addSlider("Min tess. level", [&dynamicTerrain] (float value) {
      dynamicTerrain.setMinTessellationLevel(value);
    }, 0.001f, 64.f, 12.f);

    Raz::OverlaySlider& dynamicHeightFactorSlider = overlay.addSlider("Height factor", [&dynamicTerrain] (float value) {
      dynamicTerrain.setHeightFactor(value);
    }, 0.001f, 50.f, 30.f);

    Raz::OverlaySlider& dynamicFlatnessSlider = overlay.addSlider("Flatness", [&dynamicTerrain] (float value) {
      dynamicTerrain.setFlatness(value);
    }, 1.f, 10.f, 3.f);
#endif

    Raz::OverlaySlider& staticHeightFactorSlider = overlay.addSlider("Height factor", [&staticTerrain, &normalTexture, &slopeTexture] (float value) {
      staticTerrain.setHeightFactor(value);
      normalTexture.load(staticTerrain.computeNormalMap());
      slopeTexture.load(staticTerrain.computeSlopeMap());
    }, 0.001f, 50.f, 30.f);

    Raz::OverlaySlider& staticFlatnessSlider = overlay.addSlider("Flatness", [&staticTerrain, &normalTexture, &slopeTexture] (float value) {
      staticTerrain.setFlatness(value);
      normalTexture.load(staticTerrain.computeNormalMap());
      slopeTexture.load(staticTerrain.computeSlopeMap());
    }, 1.f, 10.f, 3.f);

    overlay.addSlider("Fog density", [&fogPass] (float value) {
      fogPass.getProgram().sendUniform("uniFogDensity", value);
    }, 0.f, 1.f, 0.1f);

    overlay.addSeparator();

#if !defined(USE_OPENGL_ES)
    overlay.addCheckbox("Dynamic terrain", [&] () noexcept {
      dynamicTerrainEntity.enable();
      dynamicNoiseTexture.enable();
      dynamicNoiseMapFactorSlider.enable();
      dynamicMinTessLevelSlider.enable();
      dynamicHeightFactorSlider.enable();
      dynamicFlatnessSlider.enable();

      staticTerrainEntity.disable();
      staticColorTexture.disable();
      staticNormalTexture.disable();
      staticSlopeTexture.disable();
      staticHeightFactorSlider.disable();
      staticFlatnessSlider.disable();
    }, [&] () noexcept {
      staticTerrainEntity.enable();
      staticColorTexture.enable();
      staticNormalTexture.enable();
      staticSlopeTexture.enable();
      staticHeightFactorSlider.enable();
      staticFlatnessSlider.enable();

      dynamicTerrainEntity.disable();
      dynamicNoiseTexture.disable();
      dynamicNoiseMapFactorSlider.disable();
      dynamicMinTessLevelSlider.disable();
      dynamicHeightFactorSlider.disable();
      dynamicFlatnessSlider.disable();
    }, true);

    // Disabling all static elements at first, since we want the dynamic terrain to be used by default
    staticTerrainEntity.disable();
    staticColorTexture.disable();
    staticNormalTexture.disable();
    staticSlopeTexture.disable();
    staticHeightFactorSlider.disable();
    staticFlatnessSlider.disable();
#endif

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
