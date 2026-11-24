#include "binary_document.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>

namespace MisterBin {

bool BinaryDocument::load(const std::string &path) {
  std::ifstream in(path, std::ios::binary | std::ios::ate);
  if (!in) {
    m_error = "Cannot open file: " + path;
    return false;
  }

  const std::streamsize len = in.tellg();
  if (len < 0) {
    m_error = "Cannot determine file size: " + path;
    return false;
  }

  std::vector<uint8_t> buffer(static_cast<size_t>(len));
  in.seekg(0);
  if (len > 0 && !in.read(reinterpret_cast<char *>(buffer.data()), len)) {
    m_error = "Cannot read file: " + path;
    return false;
  }

  m_data = std::move(buffer);
  m_path = path;
  m_error.clear();
  m_undo.clear();
  m_redo.clear();
  m_save_point = 0;
  m_last_touched = 0;
  ++m_revision;
  return true;
}

bool BinaryDocument::save() {
  if (m_path.empty()) {
    m_error = "No path associated with this document";
    return false;
  }
  return save_as(m_path);
}

bool BinaryDocument::save_as(const std::string &path) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out) {
    m_error = "Cannot open file for writing: " + path;
    return false;
  }

  if (!m_data.empty()) {
    out.write(reinterpret_cast<const char *>(m_data.data()),
              static_cast<std::streamsize>(m_data.size()));
  }
  if (!out) {
    m_error = "Write failed: " + path;
    return false;
  }

  m_path = path;
  m_error.clear();
  m_save_point = m_undo.size();
  return true;
}

size_t BinaryDocument::read(size_t offset, void *dst, size_t count) const {
  if (offset >= m_data.size())
    return 0;
  const size_t n = std::min(count, m_data.size() - offset);
  std::memcpy(dst, m_data.data() + offset, n);
  return n;
}

bool BinaryDocument::write_byte(size_t offset, uint8_t value) {
  if (m_read_only || offset >= m_data.size())
    return false;
  if (m_data[offset] == value)
    return true;

  // A new edit after undoing past the save point forks history for good.
  if (m_save_point != kNoSavePoint && m_undo.size() < m_save_point)
    m_save_point = kNoSavePoint;

  m_undo.push_back({offset, m_data[offset], value});
  m_redo.clear();
  m_data[offset] = value;
  m_last_touched = offset;
  ++m_revision;
  return true;
}

bool BinaryDocument::undo() {
  if (m_read_only || m_undo.empty())
    return false;
  const Edit e = m_undo.back();
  m_undo.pop_back();
  m_data[e.offset] = e.before;
  m_redo.push_back(e);
  m_last_touched = e.offset;
  ++m_revision;
  return true;
}

bool BinaryDocument::redo() {
  if (m_read_only || m_redo.empty())
    return false;
  const Edit e = m_redo.back();
  m_redo.pop_back();
  m_data[e.offset] = e.after;
  m_undo.push_back(e);
  m_last_touched = e.offset;
  ++m_revision;
  return true;
}

} // namespace MisterBin
