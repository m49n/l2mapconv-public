#pragma once

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED

#include <unreal/BSP.h>
#include <unreal/Level.h>
#include <unreal/Package.h>
#include <unreal/StaticMesh.h>
#include <unreal/Terrain.h>

#include <rendering/Camera.h>
#include <rendering/Context.h>
#include <rendering/Entity.h>
#include <rendering/EntityMesh.h>
#include <rendering/EntityRenderer.h>
#include <rendering/EntityShader.h>
#include <rendering/EntityTree.h>
#include <rendering/ErrorHandling.h>
#include <rendering/FrameSettings.h>
#include <rendering/GeodataBlock.h>
#include <rendering/GeodataMesh.h>
#include <rendering/Material.h>
#include <rendering/MeshSurface.h>
#include <rendering/ShaderLoader.h>
#include <rendering/Texture.h>
#include <rendering/TextureLoader.h>
#include <rendering/Vertex.h>

#include <geodata/Geodata.h>
#include <geodata/Loader.h>

#include <utils/Assert.h>
#include <utils/Log.h>
#include <utils/NonCopyable.h>

#include <math/Box.h>
#include <math/Transformation.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>

#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

#include <llvm/Endian.h>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <mutex>
#include <numbers>
#include <sstream>
#include <thread>
#include <utility>
#include <vector>
