#pragma once

#include <memory>
#include <string>

#include <vxcore/include/vortex.h>
#include <vxcore/include/vortex_internals.h>

#ifndef BINARY_EDITOR_HPP
#define BINARY_EDITOR_HPP

namespace MisterBin {
class BinarySession;
}

namespace ModuleUI {

class BinaryEditorAppWindow
    : public std::enable_shared_from_this<BinaryEditorAppWindow> {
public:
  BinaryEditorAppWindow(const std::string &path, const std::string &name);

  std::shared_ptr<Cherry::AppWindow> &GetAppWindow();
  static std::shared_ptr<BinaryEditorAppWindow> Create(const std::string &path,
                                                       const std::string &name);
  void SetupRenderCallback();
  void SetMonoFont(ImFont *font) { m_mono_font = font; }

  void render();
  void left_menubar();
  void left_bottombar();

private:
  void render_toolbar();
  void render_hex_pane(float width, float height);
  void render_side_panel(float height);
  void render_inspector_tab();
  void render_format_tab();
  void render_disassembly_tab();
  void render_status_bar();
  void handle_shortcuts();

  void start_edit(size_t offset);
  void cancel_edit();

  std::shared_ptr<VxContext> ctx;

  std::shared_ptr<MisterBin::BinarySession> m_session;
  ImFont *m_mono_font = nullptr;

  bool m_dragging = false;
  bool m_scroll_to_cursor = false;
  bool m_edit_active = false;
  bool m_edit_focus = false;
  size_t m_edit_offset = 0;
  char m_edit_buf[3] = {0, 0, 0};
  char m_goto_buf[20] = {0};

  // Cherry
  std::shared_ptr<Cherry::AppWindow> m_AppWindow;
  ComponentsPool m_ComponentPool;
};
}; // namespace ModuleUI

#endif // BINARY_EDITOR_HPP
