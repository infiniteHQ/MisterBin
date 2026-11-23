#include "module.hpp"

// runtime pointer
#ifndef CMisterBin
std::weak_ptr<MisterBin::Context> CMisterBin;
#endif

std::shared_ptr<MisterBin::Context> MisterBin::create_context() {
  auto ctx = std::make_shared<MisterBin::Context>();

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
  namespace fs = std::filesystem;

  if (!fs::is_directory(path)) {
    return false;
  }

  for (const auto &entry : fs::directory_iterator(path)) {
    if (entry.is_regular_file() &&
        entry.path().filename() == "SampleConfig.txt") {
      return true;
    }
  }

  return false;
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
          MisterBin::get_current_context()->m_text_editor_instances.size());

  auto inst = ModuleUI::TextEditorAppWindow::Create(path, window_name);
  Cherry::AddAppWindow(inst->GetAppWindow());
  MisterBin::get_current_context()->m_text_editor_instances.push_back(inst);
}
