#pragma once

#include "binary_document.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace MisterBin {

enum class Endian { Little, Big };

enum class ByteClass : uint8_t {
  Null,
  Full,
  Whitespace,
  Printable,
  Control,
  High
};

ByteClass classify_byte(uint8_t value);
std::string format_hex(uint64_t value, int digits = 0, bool prefix = true);

// A decoded field. When color >= 0 the bytes it covers are tinted in the hex
// view; decoders should not emit overlapping colored fields.
struct Field {
  std::string name;
  std::string value;
  size_t offset = 0;
  size_t length = 0;
  int color = -1;
};

struct DecodeResult {
  std::string format;
  std::vector<Field> fields;
};

class IFormatDecoder {
public:
  virtual ~IFormatDecoder() = default;
  virtual const char *name() const = 0;
  virtual bool probe(const BinaryDocument &doc) const = 0;
  // May be called on data that failed probe() (manual selection): must not
  // read out of bounds.
  virtual DecodeResult decode(const BinaryDocument &doc) const = 0;
};

struct Instruction {
  uint64_t address = 0;
  size_t size = 0;
  std::string bytes;
  std::string text;
};

// Plug a real engine (Capstone, ...) in by implementing this interface.
class IDisassembler {
public:
  virtual ~IDisassembler() = default;
  virtual const char *name() const = 0;
  virtual std::vector<Instruction> disassemble(const uint8_t *data, size_t size,
                                               uint64_t base,
                                               size_t max_count) const = 0;
};

class DecoderRegistry {
public:
  static std::shared_ptr<DecoderRegistry> create_with_builtins();

  void add_format_decoder(std::shared_ptr<IFormatDecoder> decoder);
  void add_disassembler(std::shared_ptr<IDisassembler> disassembler);

  const std::vector<std::shared_ptr<IFormatDecoder>> &format_decoders() const {
    return m_formats;
  }
  const std::vector<std::shared_ptr<IDisassembler>> &disassemblers() const {
    return m_disassemblers;
  }

  // Index of the first decoder whose probe() matches, or -1.
  int detect(const BinaryDocument &doc) const;

private:
  std::vector<std::shared_ptr<IFormatDecoder>> m_formats;
  std::vector<std::shared_ptr<IDisassembler>> m_disassemblers;
};

// Numeric interpretations (int8..double, char, binary) of the bytes at offset.
std::vector<Field> inspect_at(const BinaryDocument &doc, size_t offset,
                              Endian endian);

} // namespace MisterBin
