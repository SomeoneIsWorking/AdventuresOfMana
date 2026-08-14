#pragma once

#include <cstdint>

namespace mana {

struct UiFrame;

// Transitional GLES consumer for the shared UI frame. It owns submission only;
// main retains resource lifetime until the GLES renderer is deleted.
void DrawUiGles(const UiFrame &frame, std::uint32_t program,
                std::uint32_t font_texture, std::uint32_t vertex_buffer);

} // namespace mana
