#include <vxcore/include/vortex.h>
#include <vxcore/include/vortex_internals.h>
#include <vxgui/editor/main/editor.hpp>

#include "core/binary_session.hpp"
#include "core/decoders.hpp"
#include "../ui/instances/binary_editor/binary_editor.hpp"

#ifndef MISTERBIN_HPP
#define MISTERBIN_HPP

namespace MisterBin {
struct Context {
  std::shared_ptr<ModuleInterface> m_interface;
  std::shared_ptr<DecoderRegistry> m_registry;
  std::vector<std::shared_ptr<ModuleUI::BinaryEditorAppWindow>> m_binary_editor_instances;
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
MISTERBIN_API void open_binary_editor(const std::string &path);

// Sessions and decoders
MISTERBIN_API std::shared_ptr<BinarySession> create_binary_session(const std::string &path);
MISTERBIN_API std::shared_ptr<DecoderRegistry> get_registry();
MISTERBIN_API void register_format_decoder(std::shared_ptr<IFormatDecoder> decoder);
MISTERBIN_API void register_disassembler(std::shared_ptr<IDisassembler> disassembler);

} // namespace MisterBin

#endif // MISTERBIN_HPP
