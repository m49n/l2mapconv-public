#pragma once

#include "ClientSessionContext.h"
#include "MapSelectionContext.h"
#include "RenderingContext.h"
#include "System.h"
#include "Timestep.h"
#include "UIContext.h"
#include "WindowContext.h"
#include "TerritoryRenderWindow.h"

class UISystem : public System {
public:
  explicit UISystem(UIContext &ui_context, WindowContext &window_context,
                    RenderingContext &rendering_context);
  explicit UISystem(UIContext &ui_context, WindowContext &window_context,
                    RenderingContext &rendering_context,
                    MapSelectionContext &map_selection_context,
                    ClientSessionContext &client_session_context,
                    TerritoryRenderController *territory_controller = nullptr,
                    TerritoryRenderViewState *territory_view = nullptr);
  virtual ~UISystem();

  virtual void frame_begin(Timestep frame_time) override;
  virtual void frame_end(Timestep frame_time) override;

private:
  UIContext &m_ui_context;
  WindowContext &m_window_context;
  RenderingContext &m_rendering_context;
  MapSelectionContext *m_map_selection_context{nullptr};
  ClientSessionContext *m_client_session_context{nullptr};
  TerritoryRenderController *m_territory_controller{nullptr};
  TerritoryRenderViewState *m_territory_view{nullptr};

  void rendering_window(Timestep frame_time) const;
  void client_window() const;
  void maps_window() const;
  void geodata_window() const;
};
