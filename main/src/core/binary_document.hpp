#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace MisterBin {

class BinaryDocument {
public:
  bool load(const std::string &path);
  bool save();
  bool save_as(const std::string &path);

  const std::string &path() const { return m_path; }
  const std::string &last_error() const { return m_error; }

  size_t size() const { return m_data.size(); }
  const uint8_t *data() const { return m_data.data(); }
  uint8_t at(size_t offset) const {
    return offset < m_data.size() ? m_data[offset] : 0;
  }
  size_t read(size_t offset, void *dst, size_t count) const;

  bool read_only() const { return m_read_only; }
  void set_read_only(bool value) { m_read_only = value; }

  bool write_byte(size_t offset, uint8_t value);
  bool can_undo() const { return !m_undo.empty(); }
  bool can_redo() const { return !m_redo.empty(); }
  bool undo();
  bool redo();
  size_t last_touched() const { return m_last_touched; }

  bool is_dirty() const { return m_undo.size() != m_save_point; }

  uint64_t revision() const { return m_revision; }

private:
  struct Edit {
    size_t offset;
    uint8_t before;
    uint8_t after;
  };

  static constexpr size_t kNoSavePoint = static_cast<size_t>(-1);

  std::vector<uint8_t> m_data;
  std::vector<Edit> m_undo;
  std::vector<Edit> m_redo;
  std::string m_path;
  std::string m_error;
  size_t m_save_point = 0;
  size_t m_last_touched = 0;
  uint64_t m_revision = 0;
  bool m_read_only = false;
};

} // namespace MisterBin
