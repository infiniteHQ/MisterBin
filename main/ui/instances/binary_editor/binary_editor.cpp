#include "binary_editor.hpp"
#include "../../../src/module.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

namespace ModuleUI {

namespace {

constexpr size_t kNpos = static_cast<size_t>(-1);
const char *kHexDigits = "0123456789ABCDEF";

ImU32 class_color(MisterBin::ByteClass c) {
  switch (c) {
  case MisterBin::ByteClass::Null:
    return IM_COL32(95, 95, 95, 255);
  case MisterBin::ByteClass::Full:
    return IM_COL32(235, 110, 110, 255);
  case MisterBin::ByteClass::Whitespace:
    return IM_COL32(110, 170, 235, 255);
  case MisterBin::ByteClass::Printable:
    return IM_COL32(150, 215, 150, 255);
  case MisterBin::ByteClass::Control:
    return IM_COL32(235, 185, 95, 255);
  case MisterBin::ByteClass::High:
    return IM_COL32(200, 140, 230, 255);
  }
  return IM_COL32_WHITE;
}

ImU32 field_color(int index, int alpha) {
  struct Rgb {
    int r, g, b;
  };
  static const Rgb palette[8] = {{220, 80, 80},  {230, 150, 60}, {210, 200, 70},
                                 {90, 190, 100}, {70, 190, 200}, {90, 130, 230},
                                 {160, 100, 220}, {220, 100, 170}};
  const Rgb &c = palette[index & 7];
  return IM_COL32(c.r, c.g, c.b, alpha);
}

} // namespace

BinaryEditorAppWindow::BinaryEditorAppWindow(const std::string &path,
                                             const std::string &name) {
  m_AppWindow = std::make_shared<Cherry::AppWindow>(name, name);

  m_AppWindow->SetLeftMenubarCallback([this]() { left_menubar(); });
  m_AppWindow->SetLeftBottombarCallback([this]() { left_bottombar(); });
  m_AppWindow->SetSaveMode(true);

  m_AppWindow->m_CloseCallback = [=]() {
    Cherry::DeleteAppWindow(m_AppWindow);
    m_AppWindow->SetVisibility(false);
  };

  this->ctx = vxe::get_current_context();
  m_session = MisterBin::create_binary_session(path);
}

std::shared_ptr<Cherry::AppWindow> &BinaryEditorAppWindow::GetAppWindow() {
  return m_AppWindow;
}

std::shared_ptr<BinaryEditorAppWindow>
BinaryEditorAppWindow::Create(const std::string &path, const std::string &name) {
  auto instance = std::shared_ptr<BinaryEditorAppWindow>(
      new BinaryEditorAppWindow(path, name));
  instance->SetupRenderCallback();
  return instance;
}

void BinaryEditorAppWindow::SetupRenderCallback() {
  auto self = shared_from_this();
  m_AppWindow->SetRenderCallback([self]() {
    if (self) {
      self->render();
    }
  });
}

void BinaryEditorAppWindow::render() {
  if (!m_session) {
    ImGui::TextUnformatted("Unable to open this file.");
    return;
  }

  if (m_edit_active && !m_session->editable()) {
    cancel_edit();
  }

  handle_shortcuts();

  Cherry::PushFont("JetBrainsMono");
  CherryStyle::PushFontSize(0.6f);

  const ImVec2 avail = ImGui::GetContentRegionAvail();
  const float footer_h = ImGui::GetTextLineHeightWithSpacing() + 6.0f;
  const float body_h = std::max(avail.y - footer_h, 60.0f);

  render_hex_pane(avail.x * 0.6f, body_h);
  ImGui::SameLine();

  render_side_panel(body_h);

  CherryStyle::PopFontSize();
  Cherry::PopFont();
}

void BinaryEditorAppWindow::handle_shortcuts() {
  if (m_edit_active ||
      !ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
    return;
  }

  const ImGuiIO &io = ImGui::GetIO();
  if (!io.KeyCtrl) {
    return;
  }

  if (ImGui::IsKeyPressed(ImGuiKey_S, false)) {
    m_session->save();
  } else if (ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
    if (io.KeyShift ? m_session->redo() : m_session->undo()) {
      m_scroll_to_cursor = true;
    }
  } else if (ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
    if (m_session->redo()) {
      m_scroll_to_cursor = true;
    }
  }
}

void BinaryEditorAppWindow::start_edit(size_t offset) {
  if (!m_session->editable() || offset >= m_session->document().size()) {
    return;
  }
  m_edit_active = true;
  m_edit_focus = true;
  m_edit_offset = offset;
  std::snprintf(m_edit_buf, sizeof(m_edit_buf), "%02X",
                m_session->document().at(offset));
  m_session->set_cursor(offset);
}

void BinaryEditorAppWindow::cancel_edit() { m_edit_active = false; }

void BinaryEditorAppWindow::render_toolbar() {
  MisterBin::BinarySession &s = *m_session;

  ImGui::BeginDisabled(!s.document().is_dirty());
  CherryNextComponent.SetProperty("color_border", "#00000000");
  CherryNextComponent.SetProperty("color_border_hovered", "#00000000");
  CherryNextComponent.SetProperty("color_border_pressed", "#00000000");
  CherryNextComponent.SetProperty("padding_y", "6.0f");
  if (CherryKit::ButtonImageText("Save", GetPath("resources/imgs/icons/misc/icon_save.png"))
          .GetDataAs<bool>("isClicked")) {
    s.save();
  }
  ImGui::EndDisabled();

  ImGui::BeginDisabled(!s.can_undo());
  if (CherryKit::ButtonImageText("Undo", GetPath("resources/imgs/icons/misc/icon_undo.png"))
          .GetDataAs<bool>("isClicked")) {
    if (s.undo()) {
      m_scroll_to_cursor = true;
    }
  }
  ImGui::EndDisabled();

  ImGui::BeginDisabled(!s.can_redo());
  if (CherryKit::ButtonImageText("Redo", GetPath("resources/imgs/icons/misc/icon_redo.png"))
          .GetDataAs<bool>("isClicked")) {
    if (s.redo()) {
      m_scroll_to_cursor = true;
    }
  }
  ImGui::EndDisabled();

    CherryGUI::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    CherryGUI::PushStyleColor(ImGuiCol_Separator, Cherry::HexToRGBA("#444444AA"));
    CherryGUI::Separator();
    CherryGUI::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 12));
  
  if (ImGui::RadioButton("View", !s.edit_mode())) {
    s.set_edit_mode(false);
  }
  
  ImGui::BeginDisabled(s.read_only());
  if (ImGui::RadioButton("Edit", s.edit_mode())) {
    s.set_edit_mode(true);
  }
  ImGui::EndDisabled();

  bool read_only = s.read_only();
  if (ImGui::Checkbox("Read-only", &read_only)) {
    s.set_read_only(read_only);
  }

  ImGui::SetNextItemWidth(60.0f);
  const std::string cols_label = std::to_string(s.columns());
  if (ImGui::BeginCombo("Cols", cols_label.c_str())) {
    for (size_t option : MisterBin::BinarySession::column_options()) {
      if (ImGui::Selectable(std::to_string(option).c_str(),
                            option == s.columns())) {
        s.set_columns(option);
      }
    }
    ImGui::EndCombo();
  }

  ImGui::SetNextItemWidth(110.0f);
  if (ImGui::InputTextWithHint("##goto", "Go to (hex)", m_goto_buf,
                               sizeof(m_goto_buf),
                               ImGuiInputTextFlags_EnterReturnsTrue |
                                   ImGuiInputTextFlags_CharsHexadecimal)) {
    if (s.goto_text(m_goto_buf)) {
      m_scroll_to_cursor = true;
    }
  }

  if (!s.last_error().empty()) {
    ImGui::TextColored(ImVec4(0.95f, 0.4f, 0.4f, 1.0f), "%s",
                       s.last_error().c_str());
  }
}

void BinaryEditorAppWindow::render_hex_pane(float width, float height) {
  MisterBin::BinarySession &s = *m_session;

  if (m_mono_font) {
    ImGui::PushFont(m_mono_font);
  }

  const size_t cols = s.columns();
  const size_t total = s.document().size();
  const size_t rows = (total + cols - 1) / cols;
  const int digits = total > 0xFFFFFFFFull ? 16 : 8;

  const float char_w = ImGui::CalcTextSize("F").x;
  const float text_h = ImGui::GetTextLineHeight();
  const float line_h = ImGui::GetTextLineHeightWithSpacing();
  const float group_gap = char_w;

  auto hex_x = [&](size_t i) {
    return char_w * static_cast<float>(digits + 2) +
           char_w * 3.0f * static_cast<float>(i) +
           group_gap * static_cast<float>(i / 8);
  };
  const float ascii_x = hex_x(cols) + char_w;
  const float content_w =
      ascii_x + char_w * static_cast<float>(cols) + char_w;

  ImGui::BeginChild("##hex_view", ImVec2(width, height), false,
                    ImGuiWindowFlags_HorizontalScrollbar);

  ImDrawList *dl = ImGui::GetWindowDrawList();
  const ImVec2 win_pos = ImGui::GetWindowPos();
  const ImVec2 mouse = ImGui::GetIO().MousePos;
  const bool hovered = ImGui::IsWindowHovered();
  const bool focused = ImGui::IsWindowFocused();
  const float content_top = ImGui::GetCursorPosY();
  const float origin_x = ImGui::GetCursorScreenPos().x;

  if (focused && !m_edit_active) {
    const bool shift = ImGui::GetIO().KeyShift;
    const int64_t page = static_cast<int64_t>(
        std::max(1.0f, ImGui::GetWindowHeight() / line_h - 2.0f));
    const int64_t step = static_cast<int64_t>(cols);
    auto move = [&](int64_t delta) {
      s.move_cursor(delta, shift);
      m_scroll_to_cursor = true;
    };
    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) move(1);
    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) move(-1);
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) move(step);
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) move(-step);
    if (ImGui::IsKeyPressed(ImGuiKey_PageDown)) move(page * step);
    if (ImGui::IsKeyPressed(ImGuiKey_PageUp)) move(-page * step);
    if (s.editable() && ImGui::IsKeyPressed(ImGuiKey_Enter, false)) {
      start_edit(s.cursor());
    }
  }

  // Space reserved for the sticky header drawn at the end.
  ImGui::Dummy(ImVec2(content_w, text_h));

  size_t hit = kNpos;
  bool hit_hex = false;
  bool commit_edit = false;
  bool cancel_pending = false;

  ImGuiListClipper clipper;
  clipper.Begin(static_cast<int>(rows), line_h);
  while (clipper.Step()) {
    for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
      const ImVec2 rp = ImGui::GetCursorScreenPos();
      const size_t row_off = static_cast<size_t>(row) * cols;

      char off_txt[24];
      std::snprintf(off_txt, sizeof(off_txt), "%0*llX", digits,
                    static_cast<unsigned long long>(row_off));
      dl->AddText(rp, ImGui::GetColorU32(ImGuiCol_TextDisabled), off_txt);

      const bool row_hot = hovered && mouse.y >= rp.y &&
                           mouse.y < rp.y + line_h &&
                           mouse.y >= win_pos.y + line_h;

      for (size_t i = 0; i < cols; ++i) {
        const size_t off = row_off + i;
        if (off >= total) {
          break;
        }

        const float hx = rp.x + hex_x(i);
        const float ax =
            rp.x + ascii_x + char_w * static_cast<float>(i);

        if (row_hot) {
          if (mouse.x >= hx && mouse.x < hx + 3.0f * char_w) {
            hit = off;
            hit_hex = true;
          } else if (mouse.x >= ax && mouse.x < ax + char_w) {
            hit = off;
            hit_hex = false;
          }
        }

        const uint8_t b = s.document().at(off);
        const MisterBin::ByteStyle style = s.byte_style(off);
        const ImVec2 hmin(hx, rp.y), hmax(hx + 2.0f * char_w, rp.y + text_h);
        const ImVec2 amin(ax, rp.y), amax(ax + char_w, rp.y + text_h);

        if (style.color >= 0) {
          const ImU32 bg = field_color(style.color, 70);
          dl->AddRectFilled(hmin, hmax, bg);
          dl->AddRectFilled(amin, amax, bg);
        }
        if (s.is_selected(off)) {
          const ImU32 sel = IM_COL32(60, 110, 200, 150);
          dl->AddRectFilled(hmin, hmax, sel);
          dl->AddRectFilled(amin, amax, sel);
        }

        const bool editing_here = m_edit_active && off == m_edit_offset;
        if (off == s.cursor() && !editing_here) {
          const ImU32 outline = IM_COL32(255, 255, 255, 220);
          dl->AddRect(hmin, hmax, outline);
          dl->AddRect(amin, amax, outline);
        }

        const ImU32 col = class_color(style.cls);
        const char ch = (b >= 0x20 && b < 0x7F) ? static_cast<char>(b) : '.';
        dl->AddText(ImVec2(ax, rp.y), col, &ch, &ch + 1);

        if (editing_here) {
          ImGui::PushID(static_cast<int>(off & 0x7FFFFFFF));
          ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
          ImGui::SetCursorScreenPos(ImVec2(hx, rp.y));
          ImGui::SetNextItemWidth(char_w * 2.0f + 2.0f);
          if (m_edit_focus) {
            ImGui::SetKeyboardFocusHere();
            m_edit_focus = false;
          }
          const bool edited = ImGui::InputText(
              "##byte", m_edit_buf, sizeof(m_edit_buf),
              ImGuiInputTextFlags_CharsHexadecimal |
                  ImGuiInputTextFlags_CharsUppercase |
                  ImGuiInputTextFlags_AutoSelectAll |
                  ImGuiInputTextFlags_NoHorizontalScroll);
          const bool deactivated = ImGui::IsItemDeactivated();
          const bool enter = ImGui::IsKeyPressed(ImGuiKey_Enter, false) ||
                             ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false);
          const size_t len = std::strlen(m_edit_buf);
          if ((edited && len >= 2) || (deactivated && enter && len >= 1)) {
            commit_edit = true;
          } else if (deactivated) {
            cancel_pending = true;
          }
          ImGui::PopStyleVar();
          ImGui::PopID();
          ImGui::SetCursorScreenPos(rp);
        } else {
          const char hex_txt[2] = {kHexDigits[b >> 4], kHexDigits[b & 0x0F]};
          dl->AddText(ImVec2(hx, rp.y), col, hex_txt, hex_txt + 2);
        }
      }

      ImGui::Dummy(ImVec2(content_w, text_h));
    }
  }

  if (hovered && ImGui::IsMouseClicked(0)) {
    const bool on_editor = m_edit_active && hit == m_edit_offset;
    if (m_edit_active && !on_editor) {
      cancel_edit();
    }
    if (hit != kNpos && !on_editor) {
      s.begin_selection(hit);
      if (ImGui::IsMouseDoubleClicked(0) && hit_hex && s.editable()) {
        start_edit(hit);
      } else {
        m_dragging = true;
      }
    }
  }
  if (m_dragging) {
    if (ImGui::IsMouseDown(0)) {
      if (hit != kNpos) {
        s.extend_selection(hit);
      }
    } else {
      m_dragging = false;
    }
  }

  if (commit_edit) {
    const size_t off = m_edit_offset;
    s.write_hex(off, m_edit_buf);
    if (off + 1 < total && s.editable()) {
      start_edit(off + 1);
      m_scroll_to_cursor = true;
    } else {
      cancel_edit();
    }
  } else if (cancel_pending) {
    cancel_edit();
  }

  if (m_scroll_to_cursor) {
    const float y = content_top + line_h +
                    static_cast<float>(s.cursor() / cols) * line_h;
    const float scroll = ImGui::GetScrollY();
    const float view = ImGui::GetWindowHeight();
    if (y < scroll + line_h) {
      ImGui::SetScrollY(y - line_h);
    } else if (y + line_h > scroll + view) {
      ImGui::SetScrollY(y + line_h - view);
    }
    m_scroll_to_cursor = false;
  }

  // Sticky header drawn last so it covers the rows scrolling under it.
  dl->AddRectFilled(win_pos,
                    ImVec2(win_pos.x + ImGui::GetWindowWidth(),
                           win_pos.y + line_h),
                    ImGui::GetColorU32(ImGuiCol_WindowBg));
  const ImU32 head_col = ImGui::GetColorU32(ImGuiCol_TextDisabled);
  dl->AddText(ImVec2(origin_x, win_pos.y), head_col, "Offset");
  for (size_t i = 0; i < cols; ++i) {
    char tmp[8];
    std::snprintf(tmp, sizeof(tmp), "%02X", static_cast<unsigned>(i));
    dl->AddText(ImVec2(origin_x + hex_x(i), win_pos.y), head_col, tmp);
  }
  dl->AddText(ImVec2(origin_x + ascii_x, win_pos.y), head_col, "Text");

  ImGui::EndChild();

  if (m_mono_font) {
    ImGui::PopFont();
  }
}

void BinaryEditorAppWindow::render_side_panel(float height) {
  ImGui::BeginChild("##decode_panel", ImVec2(0.0f, height), true);
  if (ImGui::BeginTabBar("##decode_tabs")) {
    if (ImGui::BeginTabItem("Inspector")) {
      render_inspector_tab();
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Format")) {
      render_format_tab();
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Disassembly")) {
      render_disassembly_tab();
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }
  ImGui::EndChild();
}

void BinaryEditorAppWindow::render_inspector_tab() {
  MisterBin::BinarySession &s = *m_session;

  int endian = s.endian() == MisterBin::Endian::Little ? 0 : 1;
  ImGui::RadioButton("Little endian", &endian, 0);
  ImGui::SameLine();
  ImGui::RadioButton("Big endian", &endian, 1);
  s.set_endian(endian == 0 ? MisterBin::Endian::Little
                           : MisterBin::Endian::Big);

  ImGui::Text("Offset: 0x%llX", static_cast<unsigned long long>(s.cursor()));

  if (ImGui::BeginTable("##inspector", 2,
                        ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
                            ImGuiTableFlags_SizingStretchProp)) {
    ImGui::TableSetupColumn("Type");
    ImGui::TableSetupColumn("Value");
    ImGui::TableHeadersRow();
    for (const MisterBin::Field &f : s.inspector()) {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(f.name.c_str());
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(f.value.c_str());
    }
    ImGui::EndTable();
  }
}

void BinaryEditorAppWindow::render_format_tab() {
  MisterBin::BinarySession &s = *m_session;

  const std::vector<std::string> names = s.format_names();
  const int sel = s.format_selection();
  const std::string preview =
      sel < 0 ? "Auto (" + s.detected_format() + ")"
              : names[static_cast<size_t>(sel)];

  ImGui::SetNextItemWidth(-1.0f);
  if (ImGui::BeginCombo("##format", preview.c_str())) {
    if (ImGui::Selectable("Auto", sel < 0)) {
      s.set_format_selection(-1);
    }
    for (size_t i = 0; i < names.size(); ++i) {
      if (ImGui::Selectable(names[i].c_str(), sel == static_cast<int>(i))) {
        s.set_format_selection(static_cast<int>(i));
      }
    }
    ImGui::EndCombo();
  }

  const MisterBin::DecodeResult &result = s.decode_result();
  if (result.fields.empty()) {
    ImGui::TextDisabled("No format detected for this file.");
    return;
  }

  if (ImGui::BeginTable("##fields", 5,
                        ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
                            ImGuiTableFlags_SizingStretchProp |
                            ImGuiTableFlags_ScrollY)) {
    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 14.0f);
    ImGui::TableSetupColumn("Field");
    ImGui::TableSetupColumn("Offset");
    ImGui::TableSetupColumn("Size");
    ImGui::TableSetupColumn("Value");
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableHeadersRow();

    for (size_t i = 0; i < result.fields.size(); ++i) {
      const MisterBin::Field &f = result.fields[i];
      ImGui::PushID(static_cast<int>(i));
      ImGui::TableNextRow();

      ImGui::TableSetColumnIndex(0);
      if (f.color >= 0) {
        const ImVec2 p = ImGui::GetCursorScreenPos();
        const float h = ImGui::GetTextLineHeight();
        ImGui::GetWindowDrawList()->AddRectFilled(
            ImVec2(p.x, p.y + 2.0f), ImVec2(p.x + 10.0f, p.y + h - 2.0f),
            field_color(f.color, 200));
      }

      ImGui::TableSetColumnIndex(1);
      if (ImGui::Selectable(f.name.c_str(), false,
                            ImGuiSelectableFlags_SpanAllColumns)) {
        s.select_range(f.offset, f.length);
        m_scroll_to_cursor = true;
      }
      ImGui::TableSetColumnIndex(2);
      ImGui::Text("0x%llX", static_cast<unsigned long long>(f.offset));
      ImGui::TableSetColumnIndex(3);
      ImGui::Text("%llu", static_cast<unsigned long long>(f.length));
      ImGui::TableSetColumnIndex(4);
      ImGui::TextUnformatted(f.value.c_str());
      ImGui::PopID();
    }
    ImGui::EndTable();
  }
}

void BinaryEditorAppWindow::render_disassembly_tab() {
  MisterBin::BinarySession &s = *m_session;

  const std::vector<std::string> names = s.disassembler_names();
  if (names.empty()) {
    ImGui::TextDisabled("No disassembler registered.");
    return;
  }

  const int sel = s.disassembler_selection();
  const std::string preview =
      sel >= 0 ? names[static_cast<size_t>(sel)] : std::string("None");
  ImGui::SetNextItemWidth(-1.0f);
  if (ImGui::BeginCombo("##arch", preview.c_str())) {
    for (size_t i = 0; i < names.size(); ++i) {
      if (ImGui::Selectable(names[i].c_str(), sel == static_cast<int>(i))) {
        s.set_disassembler_selection(static_cast<int>(i));
      }
    }
    ImGui::EndCombo();
  }

  const std::vector<MisterBin::Instruction> &code = s.disassembly();
  if (code.empty()) {
    return;
  }

  if (ImGui::BeginTable("##disasm", 3,
                        ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
                            ImGuiTableFlags_SizingStretchProp |
                            ImGuiTableFlags_ScrollY)) {
    ImGui::TableSetupColumn("Address");
    ImGui::TableSetupColumn("Bytes");
    ImGui::TableSetupColumn("Instruction");
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableHeadersRow();

    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(code.size()));
    while (clipper.Step()) {
      for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
        const MisterBin::Instruction &ins = code[static_cast<size_t>(i)];
        ImGui::TableNextRow();

        char label[48];
        std::snprintf(label, sizeof(label), "%08llX##ins%d",
                      static_cast<unsigned long long>(ins.address), i);
        ImGui::TableSetColumnIndex(0);
        if (ImGui::Selectable(label, ins.address == s.cursor(),
                              ImGuiSelectableFlags_SpanAllColumns)) {
          s.select_range(static_cast<size_t>(ins.address), ins.size);
          m_scroll_to_cursor = true;
        }
        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted(ins.bytes.c_str());
        ImGui::TableSetColumnIndex(2);
        ImGui::TextUnformatted(ins.text.c_str());
      }
    }
    ImGui::EndTable();
  }
}

void BinaryEditorAppWindow::render_status_bar() {
  ImGui::Separator();
  ImGui::TextUnformatted(m_session->status_text().c_str());
}

void BinaryEditorAppWindow::left_menubar() {
  render_toolbar();
}

void BinaryEditorAppWindow::left_bottombar() {
  render_status_bar();
}

}; // namespace ModuleUI
