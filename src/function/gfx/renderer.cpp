module;
#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

#include <sokol/sokol_gfx.h>
#include <sokol/sokol_glue.h>
#include <sokol/sokol_log.h>
#include <stb/stb_image.h>

module crumb.gfx.renderer;

import crumb.core.types;
import crumb.gfx.color;
import crumb.core.log;
import crumb.core.assets;

namespace crumb {

// Pack into the byte order expected by SG_VERTEXFORMAT_UBYTE4N: R, G, B, A.
ui32 Renderer::PackColor(Color color) {
  return to_ui32(color.r)
       | (to_ui32(color.g) << 8)
       | (to_ui32(color.b) << 16)
       | (to_ui32(color.a) << 24);
}

// Create an immutable RGBA8 texture, view, and nearest-neighbor sampler.
Renderer::Texture Renderer::MakeTexture(const ui8* pixels, i32 width, i32 height) {
  Texture texture;
  texture.width  = width;
  texture.height = height;

  sg_image_desc image_desc {};
  image_desc.width  = width;
  image_desc.height = height;
  image_desc.pixel_format = SG_PIXELFORMAT_RGBA8;
  image_desc.data.mip_levels[0] = {
    pixels,
    to_usize(width) * to_usize(height) * 4
  };
  texture.image = sg_make_image(&image_desc);

  sg_view_desc view_desc {};
  view_desc.texture.image = texture.image;
  texture.view = sg_make_view(&view_desc);

  sg_sampler_desc sampler_desc {};
  sampler_desc.min_filter = SG_FILTER_NEAREST;
  sampler_desc.mag_filter = SG_FILTER_NEAREST;
  sampler_desc.wrap_u     = SG_WRAP_CLAMP_TO_EDGE;
  sampler_desc.wrap_v     = SG_WRAP_CLAMP_TO_EDGE;
  texture.sampler = sg_make_sampler(&sampler_desc);

  texture.valid =
    sg_query_image_state(texture.image)     == SG_RESOURCESTATE_VALID &&
    sg_query_view_state(texture.view)       == SG_RESOURCESTATE_VALID &&
    sg_query_sampler_state(texture.sampler) == SG_RESOURCESTATE_VALID;
  return texture;
}

Renderer::Texture Renderer::MakeWhiteTexture() {
  const ui8 pixel[4] = { 255, 255, 255, 255 };
  return MakeTexture(pixel, 1, 1);
}

// Decode the embedded atlas and derive its glyph grid.
Renderer::Texture Renderer::MakeFontTexture() {
  i32 width    = 0;
  i32 height   = 0;
  i32 channels = 0;

  const Asset font = FontPng();
  if (font.Empty()) {
    Logger::Error("renderer: embedded font_16x16.png is empty");
    return {};
  }

  ui8* pixels = stbi_load_from_memory(
    font.data, to_i32(font.size), &width, &height, &channels, 4);
  if (pixels == nullptr) {
    Logger::Error("renderer: failed to decode embedded font_16x16.png");
    return {};
  }

  if (width <= 0 || height <= 0 ||
      width  % FONT_CELL_WIDTH  != 0 ||
      height % FONT_CELL_HEIGHT != 0) {
    Logger::Error("renderer: font atlas {}x{} is not divisible by {}x{}",
                  width, height, FONT_CELL_WIDTH, FONT_CELL_HEIGHT);
    stbi_image_free(pixels);
    return {};
  }

  Texture texture = MakeTexture(pixels, width, height);
  stbi_image_free(pixels);

  texture.columns = width  / FONT_CELL_WIDTH;
  texture.rows    = height / FONT_CELL_HEIGHT;
  return texture;
}

// Checkerboard fallback used until a sprite sheet loads.
Renderer::Texture Renderer::MakePlaceholderTexture() {
  ui8 pixels[16 * 16 * 4] {};
  for (i32 y = 0; y < 16; ++y) {
    for (i32 x = 0; x < 16; ++x) {
      const bool light = ((x / 4) + (y / 4)) % 2 == 0;
      const usize index = (to_usize(y) * 16 + to_usize(x)) * 4;
      pixels[index + 0] = light ? 80 : 40;
      pixels[index + 1] = light ? 80 : 40;
      pixels[index + 2] = light ? 100 : 50;
      pixels[index + 3] = 255;
    }
  }

  Texture texture = MakeTexture(pixels, 16, 16);
  texture.columns = 1;
  texture.rows    = 1;
  return texture;
}

void Renderer::DestroyTexture(Texture& texture) {
  if (texture.view.id != SG_INVALID_ID)
    sg_destroy_view(texture.view);
  if (texture.sampler.id != SG_INVALID_ID)
    sg_destroy_sampler(texture.sampler);
  if (texture.image.id != SG_INVALID_ID)
    sg_destroy_image(texture.image);

  texture = {};
}

// Validate the sheet, replace the previous texture, and free the stbi pixels.
bool Renderer::SetSprites(ui8* pixels, i32 width, i32 height) {
  if (pixels == nullptr)
    return false;
  if (!m_ready || m_spriteSize <= 0) {
    stbi_image_free(pixels);
    return false;
  }

  if (width < m_spriteSize || height < m_spriteSize) {
    Logger::Warn("renderer: sprites.png {}x{} is smaller than sprite size {}",
                 width, height, m_spriteSize);
    stbi_image_free(pixels);
    return false;
  }

  if (width % m_spriteSize != 0 || height % m_spriteSize != 0) {
    Logger::Warn("renderer: sprites.png {}x{} is not divisible by sprite size {}",
                 width, height, m_spriteSize);
  }

  Texture texture = MakeTexture(pixels, width, height);
  stbi_image_free(pixels);

  if (!texture.valid) {
    Logger::Error("renderer: failed to create sprite texture");
    return false;
  }

  texture.columns = width  / m_spriteSize;
  texture.rows    = height / m_spriteSize;
  if (texture.columns < 1 || texture.rows < 1) {
    Logger::Error("renderer: sprites.png has no complete sprite cells");
    DestroyTexture(texture);
    return false;
  }

  DestroyTexture(m_sprites);
  m_sprites = texture;

  Logger::Debug("renderer: loaded sprites.png {}x{} ({}x{} cells)",
                width, height, m_sprites.columns, m_sprites.rows);
  return true;
}

// Load a sheet from disk; the runtime normally uses LoadSpritesFromMemory.
bool Renderer::LoadSprites(string_view path) {
  if (!m_ready || m_spriteSize <= 0 || path.empty())
    return false;

  const string file_path(path);
  i32 width    = 0;
  i32 height   = 0;
  i32 channels = 0;

  ui8* pixels = stbi_load(
    file_path.c_str(), &width, &height, &channels, 4);
  if (pixels == nullptr) {
    Logger::Warn("renderer: sprites.png not found or invalid: {}", file_path);
    return false;
  }
  return SetSprites(pixels, width, height);
}

bool Renderer::LoadSpritesFromMemory(const ui8* data, usize size) {
  if (!m_ready || m_spriteSize <= 0 || data == nullptr || size == 0)
    return false;

  i32 width    = 0;
  i32 height   = 0;
  i32 channels = 0;

  ui8* pixels = stbi_load_from_memory(
    data, to_i32(size), &width, &height, &channels, 4);
  if (pixels == nullptr) {
    Logger::Warn("renderer: sprites.png is not a valid image");
    return false;
  }
  return SetSprites(pixels, width, height);
}

// Append two triangles. A texture change starts a new batch; a full buffer
// drops the quad.
void Renderer::PushQuad(
  const Texture& texture,
  float x, float y, float w, float h,
  float u0, float v0, float u1, float v1,
  Color color
) {
  if (!texture.valid)
    return;
  if (m_vertices.size() + 6 > MAX_VERTICES)
    return;

  if (m_currentTexture != &texture) {
    m_currentTexture = &texture;
    m_batches.push_back({ &texture, to_i32(m_vertices.size()), 0 });
  }

  const ui32 rgba = PackColor(color);
  auto& vertices = m_vertices;
  vertices.push_back({ x,     y,     u0, v0, rgba });
  vertices.push_back({ x + w, y,     u1, v0, rgba });
  vertices.push_back({ x + w, y + h, u1, v1, rgba });
  vertices.push_back({ x,     y,     u0, v0, rgba });
  vertices.push_back({ x + w, y + h, u1, v1, rgba });
  vertices.push_back({ x,     y + h, u0, v1, rgba });

  m_batches.back().count += 6;
}

// Shader maps game pixels to clip space and flips Y for a top-left origin.
void Renderer::CreatePipeline() {
  const string vertex_source =
    "#version 410 core\n"
    "layout(location=0) in vec2 aPos;\n"
    "layout(location=1) in vec2 aUV;\n"
    "layout(location=2) in vec4 aColor;\n"
    "out vec2 vUV;\n"
    "out vec4 vColor;\n"
    "void main() {\n"
    "  vec2 p = aPos / vec2(" + std::to_string(m_gameWidth) + ".0, "
    + std::to_string(m_gameHeight) + ".0) * 2.0 - 1.0;\n"
    "  gl_Position = vec4(p.x, -p.y, 0.0, 1.0);\n"
    "  vUV = aUV;\n"
    "  vColor = aColor;\n"
    "}\n";

  const char* fragment_source = R"GLSL(
#version 410 core
in vec2 vUV;
in vec4 vColor;
uniform sampler2D uTex;
out vec4 fragColor;
void main() {
  fragColor = texture(uTex, vUV) * vColor;
}
)GLSL";

  sg_shader_desc shader_desc {};
  shader_desc.vertex_func.source   = vertex_source.c_str();
  shader_desc.fragment_func.source = fragment_source;

  shader_desc.attrs[0].base_type = SG_SHADERATTRBASETYPE_FLOAT;
  shader_desc.attrs[0].glsl_name = "aPos";
  shader_desc.attrs[1].base_type = SG_SHADERATTRBASETYPE_FLOAT;
  shader_desc.attrs[1].glsl_name = "aUV";
  shader_desc.attrs[2].base_type = SG_SHADERATTRBASETYPE_FLOAT;
  shader_desc.attrs[2].glsl_name = "aColor";

  shader_desc.views[0].texture.stage       = SG_SHADERSTAGE_FRAGMENT;
  shader_desc.views[0].texture.image_type  = SG_IMAGETYPE_2D;
  shader_desc.views[0].texture.sample_type = SG_IMAGESAMPLETYPE_FLOAT;

  shader_desc.samplers[0].stage        = SG_SHADERSTAGE_FRAGMENT;
  shader_desc.samplers[0].sampler_type = SG_SAMPLERTYPE_FILTERING;

  shader_desc.texture_sampler_pairs[0].stage        = SG_SHADERSTAGE_FRAGMENT;
  shader_desc.texture_sampler_pairs[0].view_slot    = 0;
  shader_desc.texture_sampler_pairs[0].sampler_slot = 0;
  shader_desc.texture_sampler_pairs[0].glsl_name    = "uTex";

  m_shader = sg_make_shader(&shader_desc);

  sg_pipeline_desc pipeline_desc {};
  pipeline_desc.shader = m_shader;
  pipeline_desc.layout.buffers[0].stride = sizeof(Vertex);
  pipeline_desc.layout.attrs[0].offset = offsetof(Vertex, x);
  pipeline_desc.layout.attrs[0].format = SG_VERTEXFORMAT_FLOAT2;
  pipeline_desc.layout.attrs[1].offset = offsetof(Vertex, u);
  pipeline_desc.layout.attrs[1].format = SG_VERTEXFORMAT_FLOAT2;
  pipeline_desc.layout.attrs[2].offset = offsetof(Vertex, rgba);
  pipeline_desc.layout.attrs[2].format = SG_VERTEXFORMAT_UBYTE4N;

  pipeline_desc.colors[0].pixel_format =
    sg_query_desc().environment.defaults.color_format;
  pipeline_desc.colors[0].blend.enabled = true;
  pipeline_desc.colors[0].blend.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA;
  pipeline_desc.colors[0].blend.dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
  pipeline_desc.colors[0].blend.src_factor_alpha = SG_BLENDFACTOR_ONE;
  pipeline_desc.colors[0].blend.dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;

  pipeline_desc.depth.pixel_format = SG_PIXELFORMAT_NONE;
  pipeline_desc.depth.compare = SG_COMPAREFUNC_ALWAYS;
  pipeline_desc.depth.write_enabled = false;

  m_pipeline = sg_make_pipeline(&pipeline_desc);
}

// One transient buffer holds all vertices for the current frame.
void Renderer::CreateVertexBuffer() {
  sg_buffer_desc buffer_desc {};
  buffer_desc.size = to_usize(MAX_VERTICES) * sizeof(Vertex);
  buffer_desc.usage.vertex_buffer = true;
  buffer_desc.usage.write_transient = true;
  m_vertexBuffer = sg_make_buffer(&buffer_desc);
}

// Destroy GPU objects and reset CPU-side batch state.
void Renderer::DestroyResources() {
  if (sg_isvalid()) {
    DestroyTexture(m_white);
    DestroyTexture(m_font);
    DestroyTexture(m_sprites);

    if (m_pipeline.id != SG_INVALID_ID)
      sg_destroy_pipeline(m_pipeline);
    if (m_vertexBuffer.id != SG_INVALID_ID)
      sg_destroy_buffer(m_vertexBuffer);
    if (m_shader.id != SG_INVALID_ID)
      sg_destroy_shader(m_shader);
  }

  m_white = {};
  m_font = {};
  m_sprites = {};
  m_pipeline = {};
  m_vertexBuffer = {};
  m_shader = {};
  m_passAction = {};
  m_vertices.clear();
  m_batches.clear();
  m_currentTexture = nullptr;
  m_scale = 1.0f;
}

// (Re)create every GPU resource. Any failure shuts sokol down again.
bool Renderer::Init(i32 width, i32 height, i32 sprite_size) {
  if (m_ready)
    return true;

  DestroyResources();
  if (sg_isvalid())
    sg_shutdown();

  sg_desc desc {};
  desc.environment = sglue_environment();
  desc.logger.func = slog_func;
  sg_setup(&desc);

  if (!sg_isvalid()) {
    Logger::Error("renderer: sg_setup failed");
    return false;
  }

  m_gameWidth  = width  > 0 ? width  : 320;
  m_gameHeight = height > 0 ? height : 180;
  m_spriteSize = sprite_size > 0 ? sprite_size : 16;
  m_vertices.reserve(MAX_VERTICES);
  m_batches.reserve(256);

  m_white   = MakeWhiteTexture();
  m_font    = MakeFontTexture();
  m_sprites = MakePlaceholderTexture();

  if (!m_white.valid || !m_font.valid || !m_sprites.valid) {
    Logger::Error("renderer: failed to create texture resources");
    DestroyResources();
    sg_shutdown();
    return false;
  }

  CreatePipeline();
  CreateVertexBuffer();

  if (sg_query_shader_state(m_shader) != SG_RESOURCESTATE_VALID ||
      sg_query_pipeline_state(m_pipeline) != SG_RESOURCESTATE_VALID ||
      sg_query_buffer_state(m_vertexBuffer) != SG_RESOURCESTATE_VALID) {
    Logger::Error("renderer: failed to create shader, pipeline or vertex buffer");
    DestroyResources();
    sg_shutdown();
    return false;
  }

  m_passAction.colors[0].load_action = SG_LOADACTION_CLEAR;
  m_passAction.colors[0].clear_value = { 0.0f, 0.0f, 0.0f, 1.0f };
  m_ready = true;
  return true;
}

// Idempotent shutdown, also called by the destructor.
void Renderer::Shutdown() {
  DestroyResources();
  if (sg_isvalid())
    sg_shutdown();
  m_ready = false;
}

// Reset the batch, compute the letterboxed viewport, and paint the background.
void Renderer::BeginFrame(i32 window_width, i32 window_height) {
  if (!m_ready)
    return;

  m_vertices.clear();
  m_batches.clear();
  m_currentTexture = nullptr;

  const float scale = std::min(
    to_float(window_width)  / to_float(m_gameWidth),
    to_float(window_height) / to_float(m_gameHeight));
  m_scale = scale > 0.0f ? scale : 1.0f;

  m_viewportW = to_i32(to_float(m_gameWidth)  * m_scale);
  m_viewportH = to_i32(to_float(m_gameHeight) * m_scale);
  m_viewportX = (window_width  - m_viewportW) / 2;
  m_viewportY = (window_height - m_viewportH) / 2;
  if (m_viewportX < 0) m_viewportX = 0;
  if (m_viewportY < 0) m_viewportY = 0;

  PushQuad(m_white, 0.0f, 0.0f,
           to_float(m_gameWidth),
           to_float(m_gameHeight),
           0.0f, 0.0f, 1.0f, 1.0f, m_clearColor);
}

// Upload the vertex data once, then draw each texture batch in order.
void Renderer::EndFrame() {
  if (!m_ready)
    return;

  if (!m_vertices.empty()) {
    const usize bytes = m_vertices.size() * sizeof(Vertex);
    sg_write_buffer_desc write_desc {};
    write_desc.src.data.ptr  = m_vertices.data();
    write_desc.src.data.size = bytes;
    write_desc.dst.buffer    = m_vertexBuffer;
    write_desc.size          = bytes;
    sg_write_buffer_transient(&write_desc);
  }

  sg_pass pass {};
  pass.action    = m_passAction;
  pass.swapchain = sglue_swapchain();
  sg_begin_pass(&pass);

  if (!m_vertices.empty()) {
    sg_apply_viewport(m_viewportX, m_viewportY,
                      m_viewportW, m_viewportH, true);
    sg_apply_pipeline(m_pipeline);

    for (const auto& batch : m_batches) {
      sg_bindings bindings {};
      bindings.vertex_buffers[0] = m_vertexBuffer;
      bindings.views[0]          = batch.texture->view;
      bindings.samplers[0]       = batch.texture->sampler;
      sg_apply_bindings(&bindings);
      sg_draw(batch.first, batch.count, 1);
    }
  }

  sg_end_pass();
  sg_commit();
}

// The pass still clears to black; this quad covers the game area with `color`.
void Renderer::Clear(Color color) {
  PushQuad(m_white, 0.0f, 0.0f,
           to_float(m_gameWidth),
           to_float(m_gameHeight),
           0.0f, 0.0f, 1.0f, 1.0f, color);
}

void Renderer::RectFill(float x, float y, float w, float h, Color color) {
  PushQuad(m_white, x, y, w, h, 0.0f, 0.0f, 1.0f, 1.0f, color);
}

// ASCII-only: bytes outside 32..126 advance the cursor but draw no glyph.
void Renderer::Text(string_view text, float x, float y, Color color, float scale) {
  if (!m_font.valid)
    return;

  const i32 columns = m_font.columns > 0 ? m_font.columns : FONT_COLUMNS;
  const i32 rows    = m_font.rows    > 0 ? m_font.rows    : FONT_ROWS;
  const float text_scale  = scale > 0.0f ? scale : 1.0f;
  const float cell_width  = to_float(FONT_CELL_WIDTH)  * text_scale;
  const float cell_height = to_float(FONT_CELL_HEIGHT) * text_scale;

  float cursor_x = x;
  float cursor_y = y;
  for (char character : text) {
    if (character == '\n') {
      cursor_x = x;
      cursor_y += cell_height;
      continue;
    }
    if (character == '\r')
      continue;

    if (character >= 32 && character < 127) {
      const i32 glyph_index = character - 32;
      if (glyph_index < columns * rows) {
        const i32 column = glyph_index % columns;
        const i32 row    = glyph_index / columns;
        const float u0 = to_float(column * FONT_CELL_WIDTH)
                       / to_float(m_font.width);
        const float v0 = to_float(row * FONT_CELL_HEIGHT)
                       / to_float(m_font.height);
        const float u1 = to_float((column + 1) * FONT_CELL_WIDTH)
                       / to_float(m_font.width);
        const float v1 = to_float((row + 1) * FONT_CELL_HEIGHT)
                       / to_float(m_font.height);
        PushQuad(m_font, cursor_x, cursor_y,
                 cell_width, cell_height,
                 u0, v0, u1, v1, color);
      }
    }
    cursor_x += cell_width;
  }
}

// Cells are drawn row-major; flips mirror the whole block, not each cell.
void Renderer::Sprite(i32 index, float x, float y,
                      i32 w_cells, i32 h_cells,
                      bool flip_x, bool flip_y, Color tint) {
  if (!m_sprites.valid || m_spriteSize <= 0 || index < 1)
    return;

  const i32 size = m_spriteSize;
  const i32 columns = m_sprites.columns > 0
    ? m_sprites.columns
    : m_sprites.width / size;
  const i32 rows = m_sprites.rows > 0
    ? m_sprites.rows
    : m_sprites.height / size;
  if (columns < 1 || rows < 1)
    return;

  if (w_cells < 1) w_cells = 1;
  if (h_cells < 1) h_cells = 1;
  if (w_cells > columns) w_cells = columns;
  if (h_cells > rows) h_cells = rows;

  const i32 base = index - 1;
  const i32 total = columns * rows;
  if (base >= total)
    return;

  for (i32 dy = 0; dy < h_cells; ++dy) {
    for (i32 dx = 0; dx < w_cells; ++dx) {

      const i32 src_dx = flip_x ? (w_cells - 1 - dx) : dx;
      const i32 src_dy = flip_y ? (h_cells - 1 - dy) : dy;
      const i32 cell = base + src_dy * columns + src_dx;
      if (cell < 0 || cell >= total)
        continue;

      const i32 column = cell % columns;
      const i32 row    = cell / columns;

      float u0 = to_float(column * size)
               / to_float(m_sprites.width);
      float v0 = to_float(row * size)
               / to_float(m_sprites.height);
      float u1 = to_float((column + 1) * size)
               / to_float(m_sprites.width);
      float v1 = to_float((row + 1) * size)
               / to_float(m_sprites.height);

      if (flip_x) {
        const float temp = u0;
        u0 = u1;
        u1 = temp;
      }
      if (flip_y) {
        const float temp = v0;
        v0 = v1;
        v1 = temp;
      }

      PushQuad(m_sprites,
               x + to_float(dx * size),
               y + to_float(dy * size),
               to_float(size), to_float(size),
               u0, v0, u1, v1, tint);
    }
  }
}

// Width in game pixels; multiline text returns the widest line.
i32 Renderer::MeasureText(string_view text, float scale) const {
  i32 line_width = 0;
  i32 max_width  = 0;
  for (char character : text) {
    if (character == '\n') {
      max_width = std::max(max_width, line_width);
      line_width = 0;
      continue;
    }
    if (character == '\r')
      continue;
    line_width += FONT_CELL_WIDTH;
  }

  const float text_scale = scale > 0.0f ? scale : 1.0f;
  return to_i32(to_float(std::max(max_width, line_width)) * text_scale);
}

// Convert framebuffer coordinates back into game coordinates.
float Renderer::ScreenToGameX(float x) const {
  return (x - to_float(m_viewportX))
       / to_float(m_scale);
}

float Renderer::ScreenToGameY(float y) const {
  return (y - to_float(m_viewportY))
       / to_float(m_scale);
}

}
