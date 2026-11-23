
#include <vxcore/include/vortex.h>
#include <vxcore/include/vortex_internals.h>
#include <vxgui/editor/main/editor.hpp>

#include "../ui/instances/text_editor/text_editor.hpp"

#ifndef MISTERBIN_HPP
#define MISTERBIN_HPP

namespace MisterBin {
struct Context {
  std::shared_ptr<ModuleInterface> m_interface;
  std::vector<std::shared_ptr<ModuleUI::TextEditorAppWindow>>
      m_text_editor_instances;
};
} // namespace MisterBin

#ifndef MISTERBIN_API
#define MISTERBIN_API
#endif

#ifndef CMisterBin
extern MISTERBIN_API std::weak_ptr<MisterBin::Context> CMisterBin;
#endif

namespace MisterBin {
// Context
MISTERBIN_API std::shared_ptr<MisterBin::Context> create_context();
MISTERBIN_API void destroy_context(std::shared_ptr<MisterBin::Context> ctx);
MISTERBIN_API void set_current_context(std::shared_ptr<MisterBin::Context> ctx);
MISTERBIN_API std::shared_ptr<MisterBin::Context> get_current_context();

// Utils
MISTERBIN_API std::string get_path(const std::string &path);
MISTERBIN_API bool is_binary_file(const std::string &path);

// Editors
MISTERBIN_API void open_hexadecimal_editor(const std::string &path);

MISTERBIN_API void StartTextEditorInstance(const std::string &path);
MISTERBIN_API bool IsValidFile(const std::string &path);
} // namespace MisterBin

#endif // MISTERBIN_HPP