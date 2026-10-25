module;
#include <vector>

#include <sokol/sokol_gfx.h>

export module crumb.gfx.renderer;
import crumb.core.types;
import crumb.gfx.color;

export namespace crumb {

// Immediate-mode 2D renderer. Font and icon are embedded; sprites load at runtime.
class Renderer {
public:
  Renderer() = default;
  ~Renderer() { Shutdown(); }

  Renderer(const Renderer&) = delete;
  Renderer& operator=(const Renderer&) = delete;
  Renderer(Renderer&&) = delete;
  Renderer& operator=(Renderer&&) = delete;

  // Create GPU resources for a logical game resolution.
  bool Init(i32 width, i32 height, i32 sprite_size);
  // Release all sokol resources.
  void Shutdown();

  bool Ready() const { return m_ready; }

  i32 GameWidth()  const { return m_gameWidth; }
  i32 GameHeight() const { return m_gameHeight; }
  i32 SpriteSize() const { return m_spriteSize; }

  void SetClearColor(Color color) { m_clearColor = color; }

  // Start a frame and compute the letterboxed viewport.
  void BeginFrame(i32 window_width, i32 window_height);
  // Upload vertices, draw batches, and present.
  void EndFrame();

  void Clear(Color color);
  void RectFill(float x, float y, float w, float h, Color color);
  void Text(string_view text, float x, float y, Color color, float scale = 1.0f);
  // Draw a 1-based sprite block. w_cells / h_cells count sprite cells.
  void Sprite(i32 index, float x, float y,
              i32 w_cells = 1, i32 h_cells = 1,
              bool flip_x = false, bool flip_y = false,
              Color tint = TRUE_WHITE);

  // Load sprites.png from disk or from memory. Failure keeps the placeholder.
  bool LoadSprites(string_view path);
  bool LoadSpritesFromMemory(const ui8* data, usize size);

  i32   MeasureText(string_view text, float scale = 1.0f) const;
  float ScreenToGameX(float x) const;
  float ScreenToGameY(float y) const;

private:
  // The bundled font_16x16.png is a 10x10 atlas of 16x16 pixel glyph cells.
  // ASCII 32..126 starts at the top-left cell; the remaining cells are unused.
  static constexpr int FONT_CELL_WIDTH  = 16;
  static constexpr int FONT_CELL_HEIGHT = 16;
  static constexpr int FONT_COLUMNS     = 10;
  static constexpr int FONT_ROWS        = 10;
  static constexpr int MAX_VERTICES     = 65536;

  struct Texture {
    sg_image image {};
    sg_view  view {};
    sg_sampler sampler {};
    i32  width   = 0;
    i32  height  = 0;
    i32  columns = 0;
    i32  rows    = 0;
    bool valid   = false;
  };

  struct Vertex {
    float x, y, u, v;
    ui32  rgba;
  };

  struct Batch {
    const Texture* texture = nullptr;
    i32 first = 0;
    i32 count = 0;
  };

  static ui32 PackColor(Color color);
  static Texture MakeTexture(const ui8* pixels, i32 width, i32 height);
  static Texture MakeWhiteTexture();
  static Texture MakeFontTexture();
  static Texture MakePlaceholderTexture();
  static void DestroyTexture(Texture& texture);

  void PushQuad(const Texture& texture,
                float x, float y, float w, float h,
                float u0, float v0, float u1, float v1,
                Color color);
  void CreatePipeline();
  void CreateVertexBuffer();
  // Destroy shader/pipeline/buffer/textures and reset frame state.
  void DestroyResources();
  // Takes ownership of `pixels` (stbi memory) and builds the sprite texture.
  bool SetSprites(ui8* pixels, i32 width, i32 height);

  sg_shader      m_shader {};
  sg_pipeline    m_pipeline {};
  sg_buffer      m_vertexBuffer {};
  sg_pass_action m_passAction {};
  Texture        m_white;
  Texture        m_font;
  Texture        m_sprites;
  std::vector<Vertex> m_vertices;
  std::vector<Batch>  m_batches;
  const Texture* m_currentTexture = nullptr;

  Color  m_clearColor { 15, 15, 23, 255 };
  bool   m_ready       = false;
  i32    m_gameWidth   = 320;
  i32    m_gameHeight  = 180;
  i32    m_spriteSize  = 16;
  i32    m_viewportX   = 0;
  i32    m_viewportY   = 0;
  i32    m_viewportW   = 0;
  i32    m_viewportH   = 0;
  float  m_scale       = 1.0f;
};

} // namespace crumb
