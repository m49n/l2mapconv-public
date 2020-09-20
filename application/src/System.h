#pragma once

#include "Timestep.h"

class System {
public:
  virtual ~System() {}

  virtual void start() {}
  virtual void stop() {}

  virtual void frame_begin(Timestep /*frame_time*/) {}
  virtual void frame_end(Timestep /*frame_time*/) {}
};
