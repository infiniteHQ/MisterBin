#include "decoders.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace MisterBin {

ByteClass classify_byte(uint8_t v) {
  if (v == 0x00)
    return ByteClass::Null;
  if (v == 0xFF)
    return ByteClass::Full;
  if (v == '\t' || v == '\n' || v == '\r' || v == ' ')
    return ByteClass::Whitespace;
  if (v > 0x20 && v < 0x7F)
    return ByteClass::Printable;
  if (v < 0x20 || v == 0x7F)
    return ByteClass::Control;
  return ByteClass::High;
}

std::string format_hex(uint64_t value, int digits, bool prefix) {
  char buf[40];
  std::snprintf(buf, sizeof(buf), prefix ? "0x%0*llX" : "%0*llX", digits,
                static_cast<unsigned long long>(value));
  return buf;
}

namespace {

uint64_t read_uint(const BinaryDocument &doc, size_t off, size_t n,
                   Endian endian) {
  uint64_t v = 0;
  for (size_t i = 0; i < n; ++i) {
    const uint64_t b = doc.at(off + i);
    if (endian == Endian::Little)
      v |= b << (8 * i);
    else
      v = (v << 8) | b;
  }
  return v;
}

struct FieldBuilder {
  const BinaryDocument &doc;
  DecodeResult &out;

  bool fits(size_t off, size_t len) const { return off + len <= doc.size(); }

  void add(const std::string &name, const std::string &value, size_t off,
           size_t len) {
    if (!fits(off, len))
      return;
    Field f;
    f.name = name;
    f.value = value;
    f.offset = off;
    f.length = len;
    f.color = static_cast<int>(out.fields.size() % 8);
    out.fields.push_back(std::move(f));
  }

  void number(const std::string &name, size_t off, size_t len, Endian endian,
              bool decimal = false, const char *label = nullptr) {
    if (!fits(off, len))
      return;
    const uint64_t v = read_uint(doc, off, len, endian);
    std::string text = decimal ? std::to_string(v)
                               : format_hex(v, static_cast<int>(len * 2));
    if (label)
      text += std::string(" (") + label + ")";
    add(name, text, off, len);
  }

  void text(const std::string &name, size_t off, size_t len) {
    if (!fits(off, len))
      return;
    std::string s;
    for (size_t i = 0; i < len; ++i) {
      const uint8_t c = doc.at(off + i);
      s += (c >= 0x20 && c < 0x7F) ? static_cast<char>(c) : '.';
    }
    add(name, "\"" + s + "\"", off, len);
  }
};

const char *elf_type_name(uint64_t v) {
  switch (v) {
  case 1:
    return "ET_REL";
  case 2:
    return "ET_EXEC";
  case 3:
    return "ET_DYN";
  case 4:
    return "ET_CORE";
  default:
    return nullptr;
  }
}

const char *elf_machine_name(uint64_t v) {
  switch (v) {
  case 3:
    return "x86";
  case 8:
    return "MIPS";
  case 20:
    return "PowerPC";
  case 40:
    return "ARM";
  case 62:
    return "x86-64";
  case 183:
    return "AArch64";
  case 243:
    return "RISC-V";
  default:
    return nullptr;
  }
}

class ElfDecoder : public IFormatDecoder {
public:
  const char *name() const override { return "ELF"; }

  bool probe(const BinaryDocument &d) const override {
    return d.size() >= 20 && d.at(0) == 0x7F && d.at(1) == 'E' &&
           d.at(2) == 'L' && d.at(3) == 'F';
  }

  DecodeResult decode(const BinaryDocument &d) const override {
    DecodeResult r;
    r.format = "ELF";
    FieldBuilder fb{d, r};

    const bool is64 = d.at(4) == 2;
    const Endian e = d.at(5) == 2 ? Endian::Big : Endian::Little;
    const size_t n = is64 ? 8 : 4;

    fb.add("EI_MAG", "7F 45 4C 46", 0, 4);
    fb.add("EI_CLASS",
           is64 ? "ELFCLASS64" : (d.at(4) == 1 ? "ELFCLASS32" : "invalid"), 4,
           1);
    fb.add("EI_DATA",
           d.at(5) == 2   ? "ELFDATA2MSB (big endian)"
           : d.at(5) == 1 ? "ELFDATA2LSB (little endian)"
                          : "invalid",
           5, 1);
    fb.number("EI_VERSION", 6, 1, e);
    fb.number("EI_OSABI", 7, 1, e);
    fb.number("EI_ABIVERSION", 8, 1, e);
    fb.add("EI_PAD", "padding", 9, 7);

    fb.number("e_type", 16, 2, e, false,
              elf_type_name(read_uint(d, 16, 2, e)));
    fb.number("e_machine", 18, 2, e, false,
              elf_machine_name(read_uint(d, 18, 2, e)));
    fb.number("e_version", 20, 4, e);
    fb.number("e_entry", 24, n, e);
    fb.number("e_phoff", 24 + n, n, e);
    fb.number("e_shoff", 24 + 2 * n, n, e);

    const size_t f = 24 + 3 * n;
    fb.number("e_flags", f, 4, e);
    fb.number("e_ehsize", f + 4, 2, e, true);
    fb.number("e_phentsize", f + 6, 2, e, true);
    fb.number("e_phnum", f + 8, 2, e, true);
    fb.number("e_shentsize", f + 10, 2, e, true);
    fb.number("e_shnum", f + 12, 2, e, true);
    fb.number("e_shstrndx", f + 14, 2, e, true);
    return r;
  }
};

const uint8_t kPngSignature[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};

class PngDecoder : public IFormatDecoder {
public:
  const char *name() const override { return "PNG"; }

  bool probe(const BinaryDocument &d) const override {
    return d.size() >= 8 && std::memcmp(d.data(), kPngSignature, 8) == 0;
  }

  DecodeResult decode(const BinaryDocument &d) const override {
    DecodeResult r;
    r.format = "PNG";
    FieldBuilder fb{d, r};
    const Endian e = Endian::Big;

    fb.add("Signature", "89 50 4E 47 0D 0A 1A 0A", 0, 8);
    fb.number("IHDR length", 8, 4, e, true);
    fb.text("IHDR type", 12, 4);
    fb.number("Width", 16, 4, e, true);
    fb.number("Height", 20, 4, e, true);
    fb.number("Bit depth", 24, 1, e, true);
    fb.number("Color type", 25, 1, e, true);
    fb.number("Compression", 26, 1, e, true);
    fb.number("Filter", 27, 1, e, true);
    fb.number("Interlace", 28, 1, e, true);
    fb.number("IHDR CRC", 29, 4, e);
    return r;
  }
};

class RawDisassembler : public IDisassembler {
public:
  const char *name() const override { return "Raw bytes (db)"; }

  std::vector<Instruction> disassemble(const uint8_t *data, size_t size,
                                       uint64_t base,
                                       size_t max_count) const override {
    std::vector<Instruction> out;
    const size_t n = std::min(size, max_count);
    out.reserve(n);
    for (size_t i = 0; i < n; ++i) {
      Instruction ins;
      ins.address = base + i;
      ins.size = 1;
      ins.bytes = format_hex(data[i], 2, false);
      ins.text = "db " + format_hex(data[i], 2);
      out.push_back(std::move(ins));
    }
    return out;
  }
};

} // namespace

std::shared_ptr<DecoderRegistry> DecoderRegistry::create_with_builtins() {
  auto registry = std::make_shared<DecoderRegistry>();
  registry->add_format_decoder(std::make_shared<ElfDecoder>());
  registry->add_format_decoder(std::make_shared<PngDecoder>());
  registry->add_disassembler(std::make_shared<RawDisassembler>());
  return registry;
}

void DecoderRegistry::add_format_decoder(
    std::shared_ptr<IFormatDecoder> decoder) {
  if (decoder)
    m_formats.push_back(std::move(decoder));
}

void DecoderRegistry::add_disassembler(
    std::shared_ptr<IDisassembler> disassembler) {
  if (disassembler)
    m_disassemblers.push_back(std::move(disassembler));
}

int DecoderRegistry::detect(const BinaryDocument &doc) const {
  for (size_t i = 0; i < m_formats.size(); ++i) {
    if (m_formats[i]->probe(doc))
      return static_cast<int>(i);
  }
  return -1;
}

std::vector<Field> inspect_at(const BinaryDocument &doc, size_t offset,
                              Endian endian) {
  std::vector<Field> out;
  if (offset >= doc.size())
    return out;

  const size_t avail = doc.size() - offset;
  auto push = [&](const char *name, std::string value, size_t len) {
    Field f;
    f.name = name;
    f.value = std::move(value);
    f.offset = offset;
    f.length = len;
    out.push_back(std::move(f));
  };

  const uint8_t b = doc.at(offset);

  std::string bits;
  for (int i = 7; i >= 0; --i)
    bits += ((b >> i) & 1) ? '1' : '0';
  push("Binary", bits, 1);
  push("uint8", std::to_string(b), 1);
  push("int8", std::to_string(static_cast<int8_t>(b)), 1);
  push("char", (b >= 0x20 && b < 0x7F) ? std::string(1, static_cast<char>(b))
                                        : std::string("."),
       1);

  char buf[64];
  if (avail >= 2) {
    const uint64_t v = read_uint(doc, offset, 2, endian);
    push("uint16", std::to_string(static_cast<uint16_t>(v)), 2);
    push("int16", std::to_string(static_cast<int16_t>(v)), 2);
  }
  if (avail >= 4) {
    const uint32_t v = static_cast<uint32_t>(read_uint(doc, offset, 4, endian));
    push("uint32", std::to_string(v), 4);
    push("int32", std::to_string(static_cast<int32_t>(v)), 4);
    float f;
    std::memcpy(&f, &v, sizeof(f));
    std::snprintf(buf, sizeof(buf), "%.9g", static_cast<double>(f));
    push("float", buf, 4);
  }
  if (avail >= 8) {
    const uint64_t v = read_uint(doc, offset, 8, endian);
    push("uint64", std::to_string(v), 8);
    push("int64", std::to_string(static_cast<int64_t>(v)), 8);
    double d;
    std::memcpy(&d, &v, sizeof(d));
    std::snprintf(buf, sizeof(buf), "%.17g", d);
    push("double", buf, 8);
  }
  return out;
}

} // namespace MisterBin
