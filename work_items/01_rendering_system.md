# Work Item 01: Rendering System

## Objective
Implement the core rendering system with GPU acceleration, supporting text, shapes, and smooth animations.

## Requirements
- System font rendering (no font embedding)
- Dark theme as default
- Smooth animations
- Professional look and feel

## Deliverables

### 1. Renderer Core (`renderer.h` / `renderer.c`)
- GPU-accelerated drawing (OpenGL/Vulkan or platform native)
- Immediate mode API
- Batched rendering for performance

### 2. Drawing Primitives
```c
void draw_rect(Rect rect, Color color);
void draw_rect_rounded(Rect rect, f32 radius, Color color);
void draw_text(Vec2 pos, String text, Font font, Color color);
void draw_icon(Vec2 pos, Icon icon, Color color);
```

### 3. Font System (`font.h` / `font.c`)
- Load system fonts
- Text measurement
- Glyph caching

### 4. Theme System (`theme.h` / `theme.c`)
- Color palette definition
- Spacing constants
- Font sizes
- Dark theme as default
```c
typedef struct {
    Color background;
    Color panel;
    Color text;
    Color text_muted;
    Color accent;
    Color border;
    // ... more colors
} Theme;
```

### 5. Animation System (`animation.h` / `animation.c`)
- Lerp utilities
- Easing functions
- Animation state tracking
- Smooth transitions for UI elements

## Success Criteria
- Can render text using system font
- Rectangles and rounded rectangles render correctly
- Theme colors applied consistently
- Animations are smooth (60fps target)
- No visible flickering

## Dependencies
- 00_project_foundation.md

## Next Work Item
02_ui_framework.md
