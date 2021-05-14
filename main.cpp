#include <RaZ/Application.hpp>
#include <RaZ/Math/Transform.hpp>
#include <RaZ/Render/RenderSystem.hpp>

using namespace std::literals;
using namespace Raz::Literals;

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
  camera.addComponent<Raz::Camera>(window.getWidth(), window.getHeight());
  camera.addComponent<Raz::Transform>(Raz::Vec3f(0.f, 0.f, -20.f));

  //////////////////////////
  // Starting application //
  //////////////////////////

  app.run();

  return EXIT_SUCCESS;
}
