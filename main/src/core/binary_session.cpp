#include "binary_session.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

namespace MisterBin {

namespace {
constexpr size_t kDisasmMaxBytes = 1024;
constexpr size_t kDisasmMaxCount = 128;
} // namespace

BinarySession::BinarySession(std::shared_ptr<DecoderRegistry> registry)
    : m_registry(std::move(registry)) {
  if (!m_registry)
    m_registry = std::make_shared<DecoderRegistry>();
  if (!m_registry->disassemblers().empty())
    m_disasm_sel = 0;
}

bool BinarySession::open(const std::string &path) {
  if (!m_doc.load(path))
    return false;
  m_cursor = m_anchor = m_disasm_origin = 0;
  m_has_selection = false;
  invalidate_caches();
  return true;
}

void BinarySession::set_edit_mode(bool value) {
  m_edit_mode = value && !m_doc.read_only();
}

void BinarySession::set_read_only(bool value) {
  m_doc.set_read_only(value);
  if (value)
    m_edit_mode = false;
}

void BinarySession::set_columns(size_t value) {
  if (value >= 1 && value <= 64)
    m_columns = value;
}

const std::vector<size_t> &BinarySession::column_options() {
  static const std::vector<size_t> options = {8, 16, 32};
  return options;
}

size_t BinarySession::clamp_offset(size_t offset) const {
  const size_t size = m_doc.size();
  return size == 0 ? 0 : std::min(offset, size - 1);
}

void BinarySession::set_cursor(size_t offset) {
  m_cursor = m_anchor = clamp_offset(offset);
  m_has_selection = false;
  m_disasm_origin = m_cursor;
}

void BinarySession::begin_selection(size_t offset) { set_cursor(offset); }

void BinarySession::extend_selection(size_t offset) {
  m_cursor = clamp_offset(offset);
  m_has_selection = m_cursor != m_anchor;
}

void BinarySession::select_range(size_t offset, size_t length) {
  if (length == 0) {
    set_cursor(offset);
    return;
  }
  // Cursor stays on the first byte so the inspector shows the field start.
  m_cursor = clamp_offset(offset);
  m_anchor = clamp_offset(offset + length - 1);
  m_has_selection = m_cursor != m_anchor;
}

void BinarySession::move_cursor(int64_t delta, bool extend) {
  if (m_doc.size() == 0)
    return;
  int64_t target = static_cast<int64_t>(m_cursor) + delta;
  target = std::max<int64_t>(0, std::min<int64_t>(
                                    target, static_cast<int64_t>(m_doc.size()) - 1));
  if (extend)
    extend_selection(static_cast<size_t>(target));
  else
    set_cursor(static_cast<size_t>(target));
}

bool BinarySession::goto_text(const std::string &text) {
  std::string s = text;
  if (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
    s = s.substr(2);
  if (s.empty())
    return false;
  char *end = nullptr;
  const unsigned long long v = std::strtoull(s.c_str(), &end, 16);
  if (*end != '\0' || v >= m_doc.size())
    return false;
  set_cursor(static_cast<size_t>(v));
  return true;
}

size_t BinarySession::selection_start() const {
  return m_has_selection ? std::min(m_cursor, m_anchor) : m_cursor;
}

size_t BinarySession::selection_size() const {
  if (m_doc.size() == 0)
    return 0;
  if (!m_has_selection)
    return 1;
  return (m_cursor > m_anchor ? m_cursor - m_anchor : m_anchor - m_cursor) + 1;
}

bool BinarySession::is_selected(size_t offset) const {
  if (!m_has_selection)
    return false;
  return offset >= std::min(m_cursor, m_anchor) &&
         offset <= std::max(m_cursor, m_anchor);
}

bool BinarySession::write_byte(size_t offset, uint8_t value) {
  if (!editable())
    return false;
  return m_doc.write_byte(offset, value);
}

bool BinarySession::write_hex(size_t offset, const std::string &text) {
  if (text.empty())
    return false;
  char *end = nullptr;
  const unsigned long v = std::strtoul(text.c_str(), &end, 16);
  if (end == text.c_str() || *end != '\0' || v > 0xFF)
    return false;
  return write_byte(offset, static_cast<uint8_t>(v));
}

bool BinarySession::undo() {
  if (!m_doc.undo())
    return false;
  set_cursor(m_doc.last_touched());
  return true;
}

bool BinarySession::redo() {
  if (!m_doc.redo())
    return false;
  set_cursor(m_doc.last_touched());
  return true;
}

void BinarySession::invalidate_caches() {
  m_decode_valid = false;
  m_insp_valid = false;
  m_disasm_valid = false;
}

void BinarySession::ensure_decode() {
  if (m_decode_valid && m_decode_rev == m_doc.revision() &&
      m_decode_sel == m_format_sel)
    return;

  m_decode = DecodeResult{};
  m_spans.clear();

  m_detected_idx = m_registry->detect(m_doc);
  const int idx = m_format_sel >= 0 ? m_format_sel : m_detected_idx;
  const auto &decoders = m_registry->format_decoders();
  if (idx >= 0 && static_cast<size_t>(idx) < decoders.size())
    m_decode = decoders[static_cast<size_t>(idx)]->decode(m_doc);

  for (const Field &f : m_decode.fields) {
    if (f.color >= 0 && f.length > 0)
      m_spans.push_back({f.offset, f.length, f.color});
  }
  std::sort(m_spans.begin(), m_spans.end(),
            [](const Span &a, const Span &b) { return a.offset < b.offset; });

  m_decode_valid = true;
  m_decode_rev = m_doc.revision();
  m_decode_sel = m_format_sel;
}

ByteStyle BinarySession::byte_style(size_t offset) {
  ByteStyle style;
  style.cls = classify_byte(m_doc.at(offset));
  ensure_decode();

  auto it = std::upper_bound(
      m_spans.begin(), m_spans.end(), offset,
      [](size_t value, const Span &s) { return value < s.offset; });
  if (it != m_spans.begin()) {
    --it;
    if (offset < it->offset + it->length)
      style.color = it->color;
  }
  return style;
}

std::vector<std::string> BinarySession::format_names() const {
  std::vector<std::string> names;
  for (const auto &d : m_registry->format_decoders())
    names.emplace_back(d->name());
  return names;
}

void BinarySession::set_format_selection(int index) {
  const int count = static_cast<int>(m_registry->format_decoders().size());
  m_format_sel = (index >= 0 && index < count) ? index : -1;
}

std::string BinarySession::detected_format() {
  ensure_decode();
  if (m_detected_idx < 0)
    return "none";
  return m_registry->format_decoders()[static_cast<size_t>(m_detected_idx)]
      ->name();
}

const DecodeResult &BinarySession::decode_result() {
  ensure_decode();
  return m_decode;
}

const std::vector<Field> &BinarySession::inspector() {
  if (!m_insp_valid || m_insp_rev != m_doc.revision() ||
      m_insp_cursor != m_cursor || m_insp_endian != m_endian) {
    m_insp = inspect_at(m_doc, m_cursor, m_endian);
    m_insp_valid = true;
    m_insp_rev = m_doc.revision();
    m_insp_cursor = m_cursor;
    m_insp_endian = m_endian;
  }
  return m_insp;
}

std::vector<std::string> BinarySession::disassembler_names() const {
  std::vector<std::string> names;
  for (const auto &d : m_registry->disassemblers())
    names.emplace_back(d->name());
  return names;
}

void BinarySession::set_disassembler_selection(int index) {
  const int count = static_cast<int>(m_registry->disassemblers().size());
  m_disasm_sel = (index >= 0 && index < count) ? index : -1;
}

const std::vector<Instruction> &BinarySession::disassembly() {
  const auto &engines = m_registry->disassemblers();
  if (m_disasm_sel < 0 || static_cast<size_t>(m_disasm_sel) >= engines.size() ||
      m_doc.size() == 0) {
    m_disasm.clear();
    m_disasm_valid = false;
    return m_disasm;
  }

  if (!m_disasm_valid || m_disasm_rev != m_doc.revision() ||
      m_disasm_cached_origin != m_disasm_origin ||
      m_disasm_cached_sel != m_disasm_sel) {
    const size_t origin = clamp_offset(m_disasm_origin);
    const size_t avail = std::min(m_doc.size() - origin, kDisasmMaxBytes);
    m_disasm = engines[static_cast<size_t>(m_disasm_sel)]->disassemble(
        m_doc.data() + origin, avail, origin, kDisasmMaxCount);
    m_disasm_valid = true;
    m_disasm_rev = m_doc.revision();
    m_disasm_cached_origin = m_disasm_origin;
    m_disasm_cached_sel = m_disasm_sel;
  }
  return m_disasm;
}

std::string BinarySession::status_text() {
  std::string s = "Offset: " + format_hex(m_cursor, 8) + " (" +
                  std::to_string(m_cursor) + ")";
  s += "  |  Selection: " + std::to_string(selection_size()) + " byte(s)";
  s += "  |  Size: " + std::to_string(m_doc.size());
  s += "  |  Format: " + detected_format();
  s += "  |  ";
  s += editable() ? "EDIT" : (read_only() ? "READ-ONLY" : "VIEW");
  if (m_doc.is_dirty())
    s += "  [modified]";
  return s;
}

} // namespace MisterBin
