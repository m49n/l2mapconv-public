#pragma once

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED

#include <rendering/ErrorHandling.h>

#include <math/Box.h>
#include <math/Frustum.h>
#include <math/Transformation.h>

#include <utils/Assert.h>
#include <utils/Log.h>
#include <utils/NonCopyable.h>
#include <utils/Timer.h>

#include <GL/glew.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/transform.hpp>

#include <stb/stb_image.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
