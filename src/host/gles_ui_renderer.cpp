#include "host/gles_ui_renderer.h"

#include <GLES2/gl2.h>

#include <cstddef>

#include "host/render_ui.h"

namespace mana {

void DrawUiGles(const UiFrame &frame, std::uint32_t program,
                std::uint32_t font_texture, std::uint32_t vertex_buffer) {
  if (frame.vertices.empty())
    return;
  glUseProgram(program);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_DEPTH_TEST);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, font_texture);
  glUniform1i(glGetUniformLocation(program, "tex"), 0);
  const GLint tint = glGetUniformLocation(program, "tint");
  const GLint use_texture = glGetUniformLocation(program, "useTex");
  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
  glBufferData(GL_ARRAY_BUFFER,
               GLsizeiptr(frame.vertices.size() * sizeof(UiVertex)),
               frame.vertices.data(), GL_STREAM_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(UiVertex),
                        reinterpret_cast<const void *>(
                            offsetof(UiVertex, position)));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(
      1, 2, GL_FLOAT, GL_FALSE, sizeof(UiVertex),
      reinterpret_cast<const void *>(offsetof(UiVertex, uv)));
  for (const auto &batch : frame.batches) {
    glUniform4fv(tint, 1, batch.color.data());
    glUniform1f(use_texture, batch.textured ? 1.f : 0.f);
    glDrawArrays(GL_TRIANGLES, GLint(batch.first_vertex),
                 GLsizei(batch.vertex_count));
  }
  glDisableVertexAttribArray(1);
  glDisableVertexAttribArray(0);
  glEnable(GL_DEPTH_TEST);
  glDisable(GL_BLEND);
}

} // namespace mana
