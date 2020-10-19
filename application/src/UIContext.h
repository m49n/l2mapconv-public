#pragma once

struct UIContext {
  struct {
    int draws;
  } output;

  struct {
    bool wireframe;
    bool passable;
    bool terrain;
    bool static_meshes;
    bool csg;
    bool bounding_boxes;
    bool geodata;
  } input;
};
