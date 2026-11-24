#pragma once

#include "binary_document.hpp"
#include "decoders.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace MisterBin {

struct ByteStyle {
  ByteClass cls = ByteClass::Null;
  int color = -1; // decoded-field color index, -1 when not covered
};

class BinarySession {
public:
  explicit BinarySession(std::shared_ptr<DecoderRegistry> registry);

  bool open(const std::string &path);
  BinaryDocument &document() { return m_doc; }
  const BinaryDocument &document() const { return m_doc; }
  const std::string &last_error() const { return m_doc.last_error(); }

  // View / edit mode
  bool edit_mode() const { return m_edit_mode; }
  void set_edit_mode(bool value);
  bool read_only() const { return m_doc.read_only(); }
  void set_read_only(bool value);
  bool editable() const { return m_edit_mode && !m_doc.read_only(); }

  // Layout
  size_t columns() const { return m_columns; }
  void set_columns(size_t value);
  static const std::vector<size_t> &column_options();
  Endian endian() const { return m_endian; }
  void set_endian(Endian value) { m_endian = value; }

  // Cursor and selection
  size_t cursor() const { return m_cursor; }
  void set_cursor(size_t offset);
  void begin_selection(size_t offset);
  void extend_selection(size_t offset);
  void select_range(size_t offset, size_t length);
  void move_cursor(int64_t delta, bool extend);
  bool goto_text(const std::string &text);
  bool has_selection() const { return m_has_selection; }
  size_t selection_start() const;
  size_t selection_size() const;
  bool is_selected(size_t offset) const;

  // Editing
  bool write_byte(size_t offset, uint8_t value);
  bool write_hex(size_t offset, const std::string &text);
  bool can_undo() const { return m_doc.can_undo(); }
  bool can_redo() const { return m_doc.can_redo(); }
  bool undo();
  bool redo();
  bool save() { return m_doc.save(); }

  // Colorization
  ByteStyle byte_style(size_t offset);

  // Format decoding (-1 = automatic detection)
  std::vector<std::string> format_names() const;
  int format_selection() const { return m_format_sel; }
  void set_format_selection(int index);
  std::string detected_format();
  const DecodeResult &decode_result();

  // Data inspector at the cursor
  const std::vector<Field> &inspector();

  // Disassembly starting at the disassembly origin (last cursor move)
  std::vector<std::string> disassembler_names() const;
  int disassembler_selection() const { return m_disasm_sel; }
  void set_disassembler_selection(int index);
  const std::vector<Instruction> &disassembly();

  std::string status_text();

private:
  struct Span {
    size_t offset;
    size_t length;
    int color;
  };

  size_t clamp_offset(size_t offset) const;
  void invalidate_caches();
  void ensure_decode();

  std::shared_ptr<DecoderRegistry> m_registry;
  BinaryDocument m_doc;

  bool m_edit_mode = false;
  size_t m_columns = 16;
  Endian m_endian = Endian::Little;

  size_t m_cursor = 0;
  size_t m_anchor = 0;
  bool m_has_selection = false;
  size_t m_disasm_origin = 0;

  int m_format_sel = -1;
  int m_detected_idx = -1;
  bool m_decode_valid = false;
  uint64_t m_decode_rev = 0;
  int m_decode_sel = -1;
  DecodeResult m_decode;
  std::vector<Span> m_spans;

  bool m_insp_valid = false;
  uint64_t m_insp_rev = 0;
  size_t m_insp_cursor = 0;
  Endian m_insp_endian = Endian::Little;
  std::vector<Field> m_insp;

  int m_disasm_sel = -1;
  bool m_disasm_valid = false;
  uint64_t m_disasm_rev = 0;
  size_t m_disasm_cached_origin = 0;
  int m_disasm_cached_sel = -1;
  std::vector<Instruction> m_disasm;
};

} // namespace MisterBin
