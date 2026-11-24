#include "./src/module.hpp"

class infintiehq_codeedit : public ModuleInterface {
public:
  std::shared_ptr<MisterBin::Context> ctx;

  void execute() override {
    ctx = MisterBin::create_context();

    auto m = ModuleInterface::get_editor_module_by_name(this->name());
    MisterBin::get_current_context()->m_interface = m;

    this->add_content_browser_item_handler(ItemHandlerInterface(
        "file_bin", MisterBin::open_binary_editor, "View",
        "View this binary file", MisterBin::get_path("resources/icons/edit.png")));

    this->set_credits_file(MisterBin::get_path("CREDITS"));
    this->add_documentation("Take the editor", "Edit a txt file",
                            MisterBin::get_path("docs/main.md"));
    this->add_documentation("Take the editor", "Find specific text",
                            MisterBin::get_path("docs/main.md"));
  }

  void init_ui() override {
    // CherryApp.AddFont(
    //     "JetBrainsMono",
    //     MisterBin::get_path("resources/fonts/JetBrainsMono-Regular.ttf"), 40.0f);
  }

  void destroy() override {
    // Reset module
    this->reset_module();

    // Clear windows
    for (auto i : MisterBin::get_current_context()->m_binary_editor_instances) {
      CherryApp.DeleteAppWindow(i->GetAppWindow());
    }

    MisterBin::destroy_context(ctx);
    ctx.reset();
  }
};

#ifdef _WIN32
extern "C" __declspec(dllexport) ModuleInterface *create_em() {
  return new infintiehq_codeedit();
}
#else
extern "C" ModuleInterface *create_em() { return new infintiehq_codeedit(); }
#endif
