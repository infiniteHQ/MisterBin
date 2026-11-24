#include "module.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

// runtime pointer
#ifndef CMisterBin
std::weak_ptr<MisterBin::Context> CMisterBin;
#endif

std::shared_ptr<MisterBin::Context> MisterBin::create_context() {
  auto ctx = std::make_shared<MisterBin::Context>();
  ctx->m_registry = DecoderRegistry::create_with_builtins();

  set_current_context(ctx);

  return ctx;
}

void MisterBin::destroy_context(std::shared_ptr<MisterBin::Context> ctx) {
  set_current_context(nullptr);
}

void MisterBin::set_current_context(std::shared_ptr<MisterBin::Context> ctx) {
  CMisterBin = ctx;
}

std::shared_ptr<MisterBin::Context> MisterBin::get_current_context() {
  return CMisterBin.lock();
}

std::string MisterBin::get_path(const std::string &path) {
  return get_current_context()->m_interface->cook_path(path);
}

bool MisterBin::is_binary_file(const std::string &path) {
  std::error_code ec;
  if (!fs::is_regular_file(path, ec)) {
    return false;
  }

  // Heuristic: a NUL byte in the first 8 KiB means binary.
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return false;
  }

  char buffer[8192];
  in.read(buffer, sizeof(buffer));
  const std::streamsize n = in.gcount();
  return std::find(buffer, buffer + n, '\0') != buffer + n;
}

std::shared_ptr<MisterBin::DecoderRegistry> MisterBin::get_registry() {
  auto ctx = get_current_context();
  return ctx ? ctx->m_registry : nullptr;
}

void MisterBin::register_format_decoder(std::shared_ptr<IFormatDecoder> decoder) {
  if (auto registry = get_registry()) {
    registry->add_format_decoder(std::move(decoder));
  }
}

void MisterBin::register_disassembler(std::shared_ptr<IDisassembler> disassembler) {
  if (auto registry = get_registry()) {
    registry->add_disassembler(std::move(disassembler));
  }
}

std::shared_ptr<MisterBin::BinarySession>
MisterBin::create_binary_session(const std::string &path) {
  auto session = std::make_shared<BinarySession>(get_registry());
  if (!session->open(path)) {
    return nullptr;
  }
  return session;
}

void MisterBin::open_binary_editor(const std::string &path) {
  std::string filename = fs::path(path).filename().string();

  const size_t maxLen = 24;
  if (filename.size() > maxLen) {
    filename = filename.substr(0, maxLen - 3) + "...";
  }

  std::string window_name =
      filename + "####" +
      std::to_string(
          MisterBin::get_current_context()->m_binary_editor_instances.size());

  auto inst = ModuleUI::BinaryEditorAppWindow::Create(path, window_name);
  Cherry::AddAppWindow(inst->GetAppWindow());
  MisterBin::get_current_context()->m_binary_editor_instances.push_back(inst);
}
