module;
#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

#define SOKOL_GLCORE
#include <sokol/sokol_gfx.h>
#include <sokol/sokol_glue.h>
#include <sokol/sokol_log.h>
#include <stb/stb_image.h>

module crumb.renderer;

import crumb.type;
import crumb.logger;
import crumb.assets;

namespace crumb {

ui32 Renderer::PackColor(Color color) {
  return to_ui32(color.r)
       | (to_ui32(color.g) << 8)
       | (to_ui32(color.b) << 16)
       | (to_ui32(color.a) << 24);
}

Renderer::Texture Renderer::MakeTexture(const uchar* pixels, int width, int height) {
  Texture texture;
  texture.width  = width;
  texture.height = height;

  sg_image_desc imageDesc {};
  imageDesc.width  = width;
  imageDesc.height = height;
  imageDesc.pixel_format = SG_PIXELFORMAT_RGBA8;
  imageDesc.data.mip_levels[0] = {
    pixels,
    to_size_t(width) * to_size_t(height) * 4
  };
  texture.image = sg_make_image(&imageDesc);

  sg_view_desc viewDesc {};
  viewDesc.texture.image = texture.image;
  texture.view = sg_make_view(&viewDesc);

  sg_sampler_desc samplerDesc {};
  samplerDesc.min_filter = SG_FILTER_NEAREST;
  samplerDesc.mag_filter = SG_FILTER_NEAREST;
  samplerDesc.wrap_u     = SG_WRAP_CLAMP_TO_EDGE;
  samplerDesc.wrap_v     = SG_WRAP_CLAMP_TO_EDGE;
  texture.sampler = sg_make_sampler(&samplerDesc);

  texture.valid =
    sg_query_image_state(texture.image)     == SG_RESOURCESTATE_VALID &&
    sg_query_view_state(texture.view)       == SG_RESOURCESTATE_VALID &&
    sg_query_sampler_state(texture.sampler) == SG_RESOURCESTATE_VALID;
  return texture;
}

Renderer::Texture Renderer::MakeWhiteTexture() {
  const uchar pixel[4] = { 255, 255, 255, 255 };
  return MakeTexture(pixel, 1, 1);
}

Renderer::Texture Renderer::MakeFontTexture() {
  i32 width    = 0;
  i32 height   = 0;
  i32 channels = 0;

  const assets::Asset font = assets::FontPng();
  if (font.Empty()) {
    Logger::Error("renderer: embedded font_16x16.png is empty");
    return {};
  }

  uchar* pixels = stbi_load_from_memory(
    font.data, to_i32(font.size), &width, &height, &channels, 4);
  if (!pixels) {
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

Renderer::Texture Renderer::MakePlaceholderTexture() {
  uchar pixels[16 * 16 * 4] {};
  for (int y = 0; y < 16; ++y) {
    for (int x = 0; x < 16; ++x) {
      const bool light = ((x / 4) + (y / 4)) % 2 == 0;
      const size_t index = (to_size_t(y) * 16 + x) * 4;
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
  if (!texture.valid)
    return;

  if (texture.view.id != SG_INVALID_ID)
    sg_destroy_view(texture.view);
  if (texture.sampler.id != SG_INVALID_ID)
    sg_destroy_sampler(texture.sampler);
  if (texture.image.id != SG_INVALID_ID)
    sg_destroy_image(texture.image);

  texture = {};
}

// Replace the placeholder with a project sprite sheet.
bool Renderer::LoadSprites(string_view path) {
  if (!m_ready || m_spriteSize <= 0 || path.empty())
    return false;

  const string filePath(path);
  i32 width    = 0;
  i32 height   = 0;
  i32 channels = 0;

  uchar* pixels = stbi_load(
    filePath.c_str(), &width, &height, &channels, 4);
  if (!pixels) {
    Logger::Warn("renderer: sprites.png not found or invalid: {}", filePath);
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

// ---------- 内部批处理 ----------

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

// ---------- 资源创建 ----------

void Renderer::CreatePipeline() {
  const string vertexSource =
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

  const char* fragmentSource = R"GLSL(
#version 410 core
in vec2 vUV;
in vec4 vColor;
uniform sampler2D uTex;
out vec4 fragColor;
void main() {
  fragColor = texture(uTex, vUV) * vColor;
}
)GLSL";

  sg_shader_desc shaderDesc {};
  shaderDesc.vertex_func.source   = vertexSource.c_str();
  shaderDesc.fragment_func.source = fragmentSource;

  shaderDesc.attrs[0].base_type = SG_SHADERATTRBASETYPE_FLOAT;
  shaderDesc.attrs[0].glsl_name = "aPos";
  shaderDesc.attrs[1].base_type = SG_SHADERATTRBASETYPE_FLOAT;
  shaderDesc.attrs[1].glsl_name = "aUV";
  shaderDesc.attrs[2].base_type = SG_SHADERATTRBASETYPE_FLOAT;
  shaderDesc.attrs[2].glsl_name = "aColor";

  shaderDesc.views[0].texture.stage       = SG_SHADERSTAGE_FRAGMENT;
  shaderDesc.views[0].texture.image_type  = SG_IMAGETYPE_2D;
  shaderDesc.views[0].texture.sample_type = SG_IMAGESAMPLETYPE_FLOAT;

  shaderDesc.samplers[0].stage        = SG_SHADERSTAGE_FRAGMENT;
  shaderDesc.samplers[0].sampler_type = SG_SAMPLERTYPE_FILTERING;

  shaderDesc.texture_sampler_pairs[0].stage        = SG_SHADERSTAGE_FRAGMENT;
  shaderDesc.texture_sampler_pairs[0].view_slot    = 0;
  shaderDesc.texture_sampler_pairs[0].sampler_slot = 0;
  shaderDesc.texture_sampler_pairs[0].glsl_name    = "uTex";

  const sg_shader shader = sg_make_shader(&shaderDesc);

  sg_pipeline_desc pipelineDesc {};
  pipelineDesc.shader = shader;
  pipelineDesc.layout.buffers[0].stride = sizeof(Vertex);
  pipelineDesc.layout.attrs[0].offset = offsetof(Vertex, x);
  pipelineDesc.layout.attrs[0].format = SG_VERTEXFORMAT_FLOAT2;
  pipelineDesc.layout.attrs[1].offset = offsetof(Vertex, u);
  pipelineDesc.layout.attrs[1].format = SG_VERTEXFORMAT_FLOAT2;
  pipelineDesc.layout.attrs[2].offset = offsetof(Vertex, rgba);
  pipelineDesc.layout.attrs[2].format = SG_VERTEXFORMAT_UBYTE4N;

  pipelineDesc.colors[0].pixel_format =
    sg_query_desc().environment.defaults.color_format;
  pipelineDesc.colors[0].blend.enabled = true;
  pipelineDesc.colors[0].blend.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA;
  pipelineDesc.colors[0].blend.dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
  pipelineDesc.colors[0].blend.src_factor_alpha = SG_BLENDFACTOR_ONE;
  pipelineDesc.colors[0].blend.dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;

  pipelineDesc.depth.pixel_format = SG_PIXELFORMAT_NONE;
  pipelineDesc.depth.compare = SG_COMPAREFUNC_ALWAYS;
  pipelineDesc.depth.write_enabled = false;

  m_pipeline = sg_make_pipeline(&pipelineDesc);
}

void Renderer::CreateVertexBuffer() {
  sg_buffer_desc bufferDesc {};
  bufferDesc.size = to_size_t(MAX_VERTICES) * sizeof(Vertex);
  bufferDesc.usage.vertex_buffer = true;
  bufferDesc.usage.write_transient = true;
  m_vertexBuffer = sg_make_buffer(&bufferDesc);
}

// ---------- 生命周期 ----------

bool Renderer::Init(i32 width, i32 height, i32 spriteSize) {
  if (m_ready)
    return true;

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
  m_spriteSize = spriteSize > 0 ? spriteSize : 16;
  m_vertices.reserve(MAX_VERTICES);
  m_batches.reserve(256);

  m_white   = MakeWhiteTexture();
  m_font    = MakeFontTexture();
  m_sprites = MakePlaceholderTexture();

  if (!m_white.valid || !m_font.valid || !m_sprites.valid) {
    Logger::Error("renderer: failed to create texture resources");
    sg_shutdown();
    return false;
  }

  CreatePipeline();
  CreateVertexBuffer();

  if (sg_query_pipeline_state(m_pipeline) != SG_RESOURCESTATE_VALID ||
      sg_query_buffer_state(m_vertexBuffer) != SG_RESOURCESTATE_VALID) {
    Logger::Error("renderer: failed to create pipeline or vertex buffer");
    sg_shutdown();
    return false;
  }

  m_passAction.colors[0].load_action = SG_LOADACTION_CLEAR;
  m_passAction.colors[0].clear_value = { 0.0f, 0.0f, 0.0f, 1.0f };
  m_ready = true;
  return true;
}

void Renderer::Shutdown() {
  if (!m_ready)
    return;
  sg_shutdown();
  m_ready = false;
}

// ---------- 帧控制 ----------

// Fit the game resolution into the window with letterboxing.
void Renderer::BeginFrame(i32 windowWidth, i32 windowHeight) {
  if (!m_ready)
    return;

  m_vertices.clear();
  m_batches.clear();
  m_currentTexture = nullptr;

  const float scale = std::min(
    to_r32(windowWidth)  / to_r32(m_gameWidth),
    to_r32(windowHeight) / to_r32(m_gameHeight));
  m_scale = scale > 0.0f ? scale : 1.0f;

  m_viewportW = to_i32(to_r32(m_gameWidth)  * m_scale);
  m_viewportH = to_i32(to_r32(m_gameHeight) * m_scale);
  m_viewportX = (windowWidth  - m_viewportW) / 2;
  m_viewportY = (windowHeight - m_viewportH) / 2;
  if (m_viewportX < 0) m_viewportX = 0;
  if (m_viewportY < 0) m_viewportY = 0;

  PushQuad(m_white, 0.0f, 0.0f,
           to_r32(m_gameWidth),
           to_r32(m_gameHeight),
           0.0f, 0.0f, 1.0f, 1.0f, m_clearColor);
}

// Upload the queued vertices and draw all batches.
void Renderer::EndFrame() {
  if (!m_ready)
    return;

  if (!m_vertices.empty()) {
    const size_t bytes = m_vertices.size() * sizeof(Vertex);
    sg_write_buffer_desc writeDesc {};
    writeDesc.src.data.ptr  = m_vertices.data();
    writeDesc.src.data.size = bytes;
    writeDesc.dst.buffer    = m_vertexBuffer;
    writeDesc.size          = bytes;
    sg_write_buffer_transient(&writeDesc);
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

// ---------- 绘制 ----------

void Renderer::Clear(Color color) {
  PushQuad(m_white, 0.0f, 0.0f,
           to_r32(m_gameWidth),
           to_r32(m_gameHeight),
           0.0f, 0.0f, 1.0f, 1.0f, color);
}

void Renderer::RectFill(float x, float y, float w, float h, Color color) {
  PushQuad(m_white, x, y, w, h, 0.0f, 0.0f, 1.0f, 1.0f, color);
}

void Renderer::Text(string_view text, float x, float y, Color color, float scale) {
  if (!m_font.valid)
    return;

  const int columns = m_font.columns > 0 ? m_font.columns : FONT_COLUMNS;
  const int rows    = m_font.rows    > 0 ? m_font.rows    : FONT_ROWS;
  const float textScale  = scale > 0.0f ? scale : 1.0f;
  const float cellWidth  = to_r32(FONT_CELL_WIDTH)  * textScale;
  const float cellHeight = to_r32(FONT_CELL_HEIGHT) * textScale;

  float cursorX = x;
  float cursorY = y;
  for (uchar character : text) {
    if (character == '\n') {
      cursorX = x;
      cursorY += cellHeight;
      continue;
    }
    if (character == '\r')
      continue;

    if (character >= 32 && character < 127) {
      const int index = character - 32;
      if (index < columns * rows) {
        const int column = index % columns;
        const int row    = index / columns;
        const float u0 = to_r32(column * FONT_CELL_WIDTH)
                       / to_r32(m_font.width);
        const float v0 = to_r32(row * FONT_CELL_HEIGHT)
                       / to_r32(m_font.height);
        const float u1 = to_r32((column + 1) * FONT_CELL_WIDTH)
                       / to_r32(m_font.width);
        const float v1 = to_r32((row + 1) * FONT_CELL_HEIGHT)
                       / to_r32(m_font.height);
        PushQuad(m_font, cursorX, cursorY,
                 cellWidth, cellHeight,
                 u0, v0, u1, v1, color);
      }
    }
    cursorX += cellWidth;
  }
}

// Draw a 1-based sprite block; wCells/hCells follow PICO-8 spr(w,h).
void Renderer::Sprite(i32 index, float x, float y,
                      i32 wCells, i32 hCells,
                      bool flipX, bool flipY, Color tint) {
  if (!m_sprites.valid || m_spriteSize <= 0 || index < 1)
    return;

  const i32 size = to_i32(m_spriteSize);
  const i32 columns = m_sprites.columns > 0
    ? to_i32(m_sprites.columns)
    : to_i32(m_sprites.width) / size;
  const i32 rows = m_sprites.rows > 0
    ? to_i32(m_sprites.rows)
    : to_i32(m_sprites.height) / size;
  if (columns < 1 || rows < 1)
    return;

  if (wCells < 1) wCells = 1;
  if (hCells < 1) hCells = 1;
  if (wCells > columns) wCells = columns;
  if (hCells > rows) hCells = rows;

  const i32 base = index - 1;
  const i32 total = columns * rows;
  if (base >= total)
    return;

  for (i32 dy = 0; dy < hCells; ++dy) {
    for (i32 dx = 0; dx < wCells; ++dx) {
      // Mirror the whole block, not just each cell in place.
      const i32 srcDx = flipX ? (wCells - 1 - dx) : dx;
      const i32 srcDy = flipY ? (hCells - 1 - dy) : dy;
      const i32 cell = base + srcDy * columns + srcDx;
      if (cell < 0 || cell >= total)
        continue;

      const i32 column = cell % columns;
      const i32 row    = cell / columns;

      float u0 = to_r32(column * size)
               / to_r32(m_sprites.width);
      float v0 = to_r32(row * size)
               / to_r32(m_sprites.height);
      float u1 = to_r32((column + 1) * size)
               / to_r32(m_sprites.width);
      float v1 = to_r32((row + 1) * size)
               / to_r32(m_sprites.height);

      if (flipX) {
        const float temp = u0;
        u0 = u1;
        u1 = temp;
      }
      if (flipY) {
        const float temp = v0;
        v0 = v1;
        v1 = temp;
      }

      PushQuad(m_sprites,
               x + to_r32(dx * size),
               y + to_r32(dy * size),
               to_r32(size), to_r32(size),
               u0, v0, u1, v1, tint);
    }
  }
}

// ---------- 查询 ----------

i32 Renderer::MeasureText(string_view text, float scale) const {
  i32 lineWidth = 0;
  i32 maxWidth  = 0;
  for (uchar character : text) {
    if (character == '\n') {
      maxWidth = std::max(maxWidth, lineWidth);
      lineWidth = 0;
      continue;
    }
    if (character == '\r')
      continue;
    lineWidth += FONT_CELL_WIDTH;
  }

  const float textScale = scale > 0.0f ? scale : 1.0f;
  return to_i32(to_r32(std::max(maxWidth, lineWidth)) * textScale);
}

float Renderer::ScreenToGameX(float x) const {
  return (x - to_r32(m_viewportX))
       / to_r32(m_scale);
}

float Renderer::ScreenToGameY(float y) const {
  return (y - to_r32(m_viewportY))
       / to_r32(m_scale);
}

} // namespace crumb
