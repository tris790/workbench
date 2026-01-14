#ifdef _WIN32
/*
 * windows_font.c - DirectWrite font implementation for Windows
 *
 * Uses DirectWrite (dwrite.h) for high-quality glyph rendering.
 * C99, handmade hero style.
 */

#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <dwrite.h>
#include <initguid.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "font.h"
#include "renderer.h"
#include "types.h"

#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "uuid.lib")

/* Manually define IIDs for cross-compilation */
DEFINE_GUID(IID_IDWriteFactory, 0xb859ee5a, 0xd838, 0x4b5b, 0xa2, 0xe8, 0x1a,
            0xdc, 0x7d, 0x93, 0xdb, 0x48);
DEFINE_GUID(IID_IDWriteFontFileLoader, 0x727cad4e, 0x0d65, 0x49ce, 0x8f, 0xf9,
            0xfa, 0x19, 0x31, 0x63, 0x15, 0x90);
DEFINE_GUID(IID_IDWriteFontFileStream, 0x6d48655c, 0xe508, 0x4903, 0x90, 0xa7,
            0x33, 0x67, 0xc9, 0x63, 0x66, 0x5a);

/* ===== Constants ===== */

#define GLYPH_CACHE_SIZE 256
#define MAX_FONT_PATH 512

/* ===== logging ===== */

static void Font_Log(const char *fmt, ...) {
  FILE *f = fopen("font_debug.txt", "a");
  if (f) {
    va_list args;
    va_start(args, fmt);
    vfprintf(f, fmt, args);
    va_end(args);
    fprintf(f, "\n");
    fclose(f);
  }
}

/* ===== Cached Glyph ===== */

typedef struct {
  u8 *bitmap; /* Alpha map (coverage) */
  i32 width;
  i32 height;
  i32 bearing_x; /* Offset X from pen position */
  i32 bearing_y; /* Offset Y from pen position (to top-left of bitmap) */
  i32 advance;   /* Horizontal advance */
  u32 codepoint;
  b32 valid;
} cached_glyph;

/* ===== Font Structure ===== */

struct font {
  IDWriteFontFace *face;
  i32 size_pixels;
  f32 em_size; /* Size in DIPs */
  i32 line_height;
  i32 ascender;
  i32 descender;
  f32 scale; /* Scale factor if needed (DWrite handles scaling typically) */
  cached_glyph cache[GLYPH_CACHE_SIZE];
};

/* ===== Global State ===== */

static struct {
  IDWriteFactory *factory;
  b32 initialized;
} g_font_system = {0};

/* ===== Helper: Wide String Conversion ===== */

static void ToWide(const char *utf8, wchar_t *out, i32 out_len) {
  MultiByteToWideChar(CP_UTF8, 0, utf8, -1, out, out_len);
}

/* ===== Memory Font Loader implementation ===== */

typedef struct {
  const void *data;
  usize size;
} MemoryFontKey;

typedef struct {
  IDWriteFontFileStreamVtbl *lpVtbl;
  LONG ref_count;
  const void *data;
  UINT64 size;
} MemoryFontStream;

typedef struct {
  IDWriteFontFileLoaderVtbl *lpVtbl;
  LONG ref_count;
} MemoryFontLoader;

/* Forward declarations for VTable methods */
static HRESULT STDMETHODCALLTYPE MemoryFontStream_QueryInterface(
    IDWriteFontFileStream *This, REFIID riid, void **ppvObject);
static ULONG STDMETHODCALLTYPE
MemoryFontStream_AddRef(IDWriteFontFileStream *This);
static ULONG STDMETHODCALLTYPE
MemoryFontStream_Release(IDWriteFontFileStream *This);
static HRESULT STDMETHODCALLTYPE MemoryFontStream_ReadFileFragment(
    IDWriteFontFileStream *This, const void **fragmentStart, UINT64 fileOffset,
    UINT64 fragmentSize, void **fragmentContext);
static void STDMETHODCALLTYPE MemoryFontStream_ReleaseFileFragment(
    IDWriteFontFileStream *This, void *fragmentContext);
static HRESULT STDMETHODCALLTYPE
MemoryFontStream_GetFileSize(IDWriteFontFileStream *This, UINT64 *fileSize);
static HRESULT STDMETHODCALLTYPE MemoryFontStream_GetLastWriteTime(
    IDWriteFontFileStream *This, UINT64 *lastWriteTime);

static IDWriteFontFileStreamVtbl g_MemoryFontStreamVtbl = {
    MemoryFontStream_QueryInterface,
    MemoryFontStream_AddRef,
    MemoryFontStream_Release,
    MemoryFontStream_ReadFileFragment,
    MemoryFontStream_ReleaseFileFragment,
    MemoryFontStream_GetFileSize,
    MemoryFontStream_GetLastWriteTime};

static HRESULT STDMETHODCALLTYPE MemoryFontLoader_QueryInterface(
    IDWriteFontFileLoader *This, REFIID riid, void **ppvObject);
static ULONG STDMETHODCALLTYPE
MemoryFontLoader_AddRef(IDWriteFontFileLoader *This);
static ULONG STDMETHODCALLTYPE
MemoryFontLoader_Release(IDWriteFontFileLoader *This);
static HRESULT STDMETHODCALLTYPE MemoryFontLoader_CreateStreamFromKey(
    IDWriteFontFileLoader *This, const void *fontFileReferenceKey,
    UINT32 fontFileReferenceKeySize, IDWriteFontFileStream **fontFileStream);

static IDWriteFontFileLoaderVtbl g_MemoryFontLoaderVtbl = {
    MemoryFontLoader_QueryInterface, MemoryFontLoader_AddRef,
    MemoryFontLoader_Release, MemoryFontLoader_CreateStreamFromKey};

static MemoryFontLoader g_MemoryFontLoader = {&g_MemoryFontLoaderVtbl, 1};

static HRESULT STDMETHODCALLTYPE MemoryFontStream_QueryInterface(
    IDWriteFontFileStream *This, REFIID riid, void **ppvObject) {
  if (IsEqualIID(riid, &IID_IUnknown) ||
      IsEqualIID(riid, &IID_IDWriteFontFileStream)) {
    *ppvObject = This;
    IDWriteFontFileStream_AddRef(This);
    return S_OK;
  }
  *ppvObject = NULL;
  return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE
MemoryFontStream_AddRef(IDWriteFontFileStream *This) {
  MemoryFontStream *self = (MemoryFontStream *)This;
  return InterlockedIncrement(&self->ref_count);
}

static ULONG STDMETHODCALLTYPE
MemoryFontStream_Release(IDWriteFontFileStream *This) {
  MemoryFontStream *self = (MemoryFontStream *)This;
  ULONG count = InterlockedDecrement(&self->ref_count);
  if (count == 0) {
    free(self);
  }
  return count;
}

static HRESULT STDMETHODCALLTYPE MemoryFontStream_ReadFileFragment(
    IDWriteFontFileStream *This, const void **fragmentStart, UINT64 fileOffset,
    UINT64 fragmentSize, void **fragmentContext) {
  MemoryFontStream *self = (MemoryFontStream *)This;
  if (fileOffset + fragmentSize > self->size) {
    return E_FAIL;
  }
  *fragmentStart = (const u8 *)self->data + fileOffset;
  *fragmentContext = NULL;
  return S_OK;
}

static void STDMETHODCALLTYPE MemoryFontStream_ReleaseFileFragment(
    IDWriteFontFileStream *This, void *fragmentContext) {
  (void)fragmentContext;
}

static HRESULT STDMETHODCALLTYPE
MemoryFontStream_GetFileSize(IDWriteFontFileStream *This, UINT64 *fileSize) {
  MemoryFontStream *self = (MemoryFontStream *)This;
  *fileSize = self->size;
  return S_OK;
}

static HRESULT STDMETHODCALLTYPE MemoryFontStream_GetLastWriteTime(
    IDWriteFontFileStream *This, UINT64 *lastWriteTime) {
  *lastWriteTime = 0;
  return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE MemoryFontLoader_QueryInterface(
    IDWriteFontFileLoader *This, REFIID riid, void **ppvObject) {
  if (IsEqualIID(riid, &IID_IUnknown) ||
      IsEqualIID(riid, &IID_IDWriteFontFileLoader)) {
    *ppvObject = This;
    IDWriteFontFileLoader_AddRef(This);
    return S_OK;
  }
  *ppvObject = NULL;
  return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE
MemoryFontLoader_AddRef(IDWriteFontFileLoader *This) {
  MemoryFontLoader *self = (MemoryFontLoader *)This;
  return InterlockedIncrement(&self->ref_count);
}

static ULONG STDMETHODCALLTYPE
MemoryFontLoader_Release(IDWriteFontFileLoader *This) {
  (void)This;
  return 1;
}

static HRESULT STDMETHODCALLTYPE MemoryFontLoader_CreateStreamFromKey(
    IDWriteFontFileLoader *This, const void *fontFileReferenceKey,
    UINT32 fontFileReferenceKeySize, IDWriteFontFileStream **fontFileStream) {
  (void)This;
  if (fontFileReferenceKeySize != sizeof(MemoryFontKey)) {
    return E_INVALIDARG;
  }
  const MemoryFontKey *key = (const MemoryFontKey *)fontFileReferenceKey;

  MemoryFontStream *stream =
      (MemoryFontStream *)calloc(1, sizeof(MemoryFontStream));
  if (!stream)
    return E_OUTOFMEMORY;

  stream->lpVtbl = &g_MemoryFontStreamVtbl;
  stream->ref_count = 1;
  stream->data = key->data;
  stream->size = key->size;

  *fontFileStream = (IDWriteFontFileStream *)stream;
  return S_OK;
}

/* ===== Font System Lifecycle ===== */

b32 Font_SystemInit(void) {
  if (g_font_system.initialized)
    return true;

  HRESULT hr =
      DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, &IID_IDWriteFactory,
                          (IUnknown **)&g_font_system.factory);

  if (FAILED(hr)) {
    Font_Log("Font: DWriteCreateFactory failed: 0x%08X", (unsigned int)hr);
    fprintf(stderr, "Font: DWriteCreateFactory failed: 0x%08X\n",
            (unsigned int)hr);
    return false;
  }

  /* Register memory font loader */
  IDWriteFactory_RegisterFontFileLoader(
      g_font_system.factory, (IDWriteFontFileLoader *)&g_MemoryFontLoader);

  Font_Log("Font: System initialized successfully");
  g_font_system.initialized = true;
  return true;
}

void Font_SystemShutdown(void) {
  if (!g_font_system.initialized)
    return;

  if (g_font_system.factory) {
    IDWriteFactory_UnregisterFontFileLoader(
        g_font_system.factory, (IDWriteFontFileLoader *)&g_MemoryFontLoader);
    IDWriteFactory_Release(g_font_system.factory);
    g_font_system.factory = NULL;
  }
  g_font_system.initialized = false;
}

/* ===== Font Metric Calculation ===== */

static void UpdateFontMetrics(font *f) {
  if (!f || !f->face)
    return;

  DWRITE_FONT_METRICS metrics;
  IDWriteFontFace_GetMetrics(f->face, &metrics);

  u16 design_units = metrics.designUnitsPerEm;
  f->scale = (f32)f->size_pixels / (f32)design_units;

  f->ascender = (i32)roundf(metrics.ascent * f->scale);
  /* DWrite descent is positive value going down */
  f->descender = (i32)roundf(metrics.descent * f->scale);
  f->line_height = (i32)roundf(
      (metrics.ascent + metrics.descent + metrics.lineGap) * f->scale);

  f->em_size = (f32)f->size_pixels;
  Font_Log(
      "Font metrics updated: size=%d, em_size=%.2f, asc=%d, desc=%d, line=%d",
      f->size_pixels, f->em_size, f->ascender, f->descender, f->line_height);
}

/* ===== Font Loading ===== */

font *Font_Load(const char *name, i32 size_pixels) {
  if (!g_font_system.initialized) {
    if (!Font_SystemInit())
      return NULL;
  }

  font *f = calloc(1, sizeof(font));
  if (!f)
    return NULL;

  f->size_pixels = size_pixels;

  /* Map generic names */
  const wchar_t *family_name = L"Segoe UI"; /* Default */
  if (name) {
    if (strcmp(name, "sans") == 0)
      family_name = L"Segoe UI"; /* Better default than Arial */
    else if (strcmp(name, "serif") == 0)
      family_name = L"Times New Roman";
    else if (strcmp(name, "monospace") == 0)
      family_name = L"Consolas";
    else {
      static wchar_t buf[256];
      ToWide(name, buf, 256);
      family_name = buf;
    }
  }

  /* Find font in system collection */
  IDWriteFontCollection *collection = NULL;
  HRESULT hr = IDWriteFactory_GetSystemFontCollection(g_font_system.factory,
                                                      &collection, FALSE);
  if (FAILED(hr)) {
    Font_Log("Font: GetSystemFontCollection failed");
    free(f);
    return NULL;
  }

  u32 index;
  BOOL exists;
  hr = IDWriteFontCollection_FindFamilyName(collection, family_name, &index,
                                            &exists);

  if (FAILED(hr) || !exists) {
    /* Fallback to first font if not found, or specific fallback */
    IDWriteFontCollection_FindFamilyName(collection, L"Segoe UI", &index,
                                         &exists);
  }

  if (exists) {
    IDWriteFontFamily *family = NULL;
    hr = IDWriteFontCollection_GetFontFamily(collection, index, &family);
    if (SUCCEEDED(hr)) {
      IDWriteFont *dwrite_font = NULL;
      hr = IDWriteFontFamily_GetFirstMatchingFont(
          family, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL,
          DWRITE_FONT_STYLE_NORMAL, &dwrite_font);

      if (SUCCEEDED(hr)) {
        hr = IDWriteFont_CreateFontFace(dwrite_font, &f->face);
        IDWriteFont_Release(dwrite_font);
      }
      IDWriteFontFamily_Release(family);
    }
  }

  IDWriteFontCollection_Release(collection);

  if (!f->face) {
    Font_Log("Font: Failed to create face for %s", name ? name : "default");
    free(f);
    return NULL;
  }

  UpdateFontMetrics(f);
  Font_Log("Font loaded: %s (%dpx)", name ? name : "default", size_pixels);
  return f;
}

/* ===== Helper: Path Probing ===== */

static b32 FileExists(const char *path) {
  DWORD dwAttrib = GetFileAttributesA(path);
  return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
          !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

static void GetExeDirectory(char *buf, size_t buf_size) {
  buf[0] = '\0';
  DWORD len = GetModuleFileNameA(NULL, buf, (DWORD)buf_size);
  if (len > 0 && len < buf_size) {
    char *last_sep = strrchr(buf, '\\');
    if (!last_sep)
      last_sep = strrchr(buf, '/');
    if (last_sep)
      last_sep[1] = '\0';
  }
}

font *Font_LoadFromFile(const char *path, i32 size_pixels) {
  if (!g_font_system.initialized) {
    if (!Font_SystemInit())
      return NULL;
  }

  /* 1. Probe paths as before */
  char final_path[MAX_FONT_PATH];
  strncpy(final_path, path, MAX_FONT_PATH);
  final_path[MAX_FONT_PATH - 1] = '\0';

  if (!FileExists(final_path)) {
    char exe_dir[MAX_FONT_PATH];
    GetExeDirectory(exe_dir, sizeof(exe_dir));

    char probe_path[MAX_FONT_PATH];

    /* Try ExeDir/path */
    snprintf(probe_path, sizeof(probe_path), "%s%s", exe_dir, path);
    if (FileExists(probe_path)) {
      strncpy(final_path, probe_path, MAX_FONT_PATH);
    } else {
      /* Try ExeDir/../path */
      snprintf(probe_path, sizeof(probe_path), "%s../%s", exe_dir, path);
      if (FileExists(probe_path)) {
        strncpy(final_path, probe_path, MAX_FONT_PATH);
      }
    }
  }

  /* 2. Canonicalize path */
  /* DirectWrite requires absolute path, matching normal OS paths */
  wchar_t wpath[MAX_FONT_PATH];
  ToWide(final_path, wpath, MAX_FONT_PATH);

  wchar_t abs_path[MAX_FONT_PATH];
  DWORD res = GetFullPathNameW(wpath, MAX_FONT_PATH, abs_path, NULL);
  if (res == 0 || res >= MAX_FONT_PATH) {
    /* formatting failed, use wpath as is */
    wcsncpy(abs_path, wpath, MAX_FONT_PATH);
  }

  font *f = calloc(1, sizeof(font));
  if (!f)
    return NULL;
  f->size_pixels = size_pixels;

  IDWriteFontFile *font_file = NULL;
  HRESULT hr = IDWriteFactory_CreateFontFileReference(
      g_font_system.factory, abs_path, NULL, &font_file);

  if (FAILED(hr)) {
    Font_Log("Font: CreateFontFileReference failed for '%ls' (0x%X)", abs_path,
             (unsigned int)hr);
    fprintf(stderr,
            "Font: CreateFontFileReference failed for '%ls' (0x%X), using "
            "system fallback.\n",
            abs_path, (unsigned int)hr);
    free(f);
    return Font_Load("Segoe UI", size_pixels);
  }

  /* Analyze file to get type */
  BOOL isSupported;
  DWRITE_FONT_FILE_TYPE fileType;
  DWRITE_FONT_FACE_TYPE faceType;
  UINT32 numberOfFaces;
  hr = IDWriteFontFile_Analyze(font_file, &isSupported, &fileType, &faceType,
                               &numberOfFaces);

  if (FAILED(hr) || !isSupported) {
    Font_Log("Font: Analyze failed or unsupported for '%ls'", abs_path);
    fprintf(stderr,
            "Font: Analyze failed or unsupported for '%ls', using system "
            "fallback.\n",
            abs_path);
    IDWriteFontFile_Release(font_file);
    free(f);
    return Font_Load("Segoe UI", size_pixels);
  }

  /* Create face from file */
  IDWriteFontFile *files[] = {font_file};
  hr = IDWriteFactory_CreateFontFace(g_font_system.factory, faceType, 1, files,
                                     0, DWRITE_FONT_SIMULATIONS_NONE, &f->face);

  IDWriteFontFile_Release(font_file);

  if (FAILED(hr)) {
    Font_Log("Font: CreateFontFace failed for '%ls' (0x%X)", abs_path,
             (unsigned int)hr);
    fprintf(stderr,
            "Font: CreateFontFace failed for '%ls' (0x%X). Type: %d, Faces: "
            "%d. Using system fallback.\n",
            abs_path, (unsigned int)hr, faceType, numberOfFaces);
    free(f);
    return Font_Load("Segoe UI", size_pixels);
  }

  UpdateFontMetrics(f);
  Font_Log("Font loaded from file: %ls (%dpx)", abs_path, size_pixels);
  return f;
}

font *Font_LoadFromMemory(const void *data, usize size, i32 size_pixels) {
  if (!g_font_system.initialized) {
    if (!Font_SystemInit())
      return NULL;
  }

  MemoryFontKey key = {data, size};
  IDWriteFontFile *font_file = NULL;
  HRESULT hr = IDWriteFactory_CreateCustomFontFileReference(
      g_font_system.factory, &key, sizeof(key),
      (IDWriteFontFileLoader *)&g_MemoryFontLoader, &font_file);

  if (FAILED(hr)) {
    Font_Log("Font: CreateCustomFontFileReference failed (0x%08X)",
             (unsigned int)hr);
    return Font_Load("Segoe UI", size_pixels);
  }

  /* Analyze file to get type */
  BOOL isSupported;
  DWRITE_FONT_FILE_TYPE fileType;
  DWRITE_FONT_FACE_TYPE faceType;
  UINT32 numberOfFaces;
  hr = IDWriteFontFile_Analyze(font_file, &isSupported, &fileType, &faceType,
                               &numberOfFaces);

  if (FAILED(hr) || !isSupported) {
    Font_Log("Font: Memory font analysis failed or unsupported");
    IDWriteFontFile_Release(font_file);
    return Font_Load("Segoe UI", size_pixels);
  }

  font *f = calloc(1, sizeof(font));
  if (!f) {
    IDWriteFontFile_Release(font_file);
    return NULL;
  }
  f->size_pixels = size_pixels;

  /* Create face from the custom font file */
  IDWriteFontFile *files[] = {font_file};
  hr = IDWriteFactory_CreateFontFace(g_font_system.factory, faceType, 1, files,
                                     0, DWRITE_FONT_SIMULATIONS_NONE, &f->face);

  IDWriteFontFile_Release(font_file);

  if (FAILED(hr)) {
    Font_Log("Font: CreateFontFace failed for memory font (0x%08X)",
             (unsigned int)hr);
    free(f);
    return Font_Load("Segoe UI", size_pixels);
  }

  UpdateFontMetrics(f);
  Font_Log("Font loaded from memory (%zu bytes, %dpx)", size, size_pixels);
  return f;
}

void Font_Free(font *f) {
  if (!f)
    return;

  for (i32 i = 0; i < GLYPH_CACHE_SIZE; i++) {
    if (f->cache[i].bitmap) {
      free(f->cache[i].bitmap);
    }
  }

  if (f->face) {
    IDWriteFontFace_Release(f->face);
  }

  free(f);
}

/* ===== Font Metrics ===== */

i32 Font_GetLineHeight(font *f) { return f ? f->line_height : 0; }
i32 Font_GetAscender(font *f) { return f ? f->ascender : 0; }
i32 Font_GetDescender(font *f) { return f ? f->descender : 0; }

/* ===== Glyph Caching ===== */

static cached_glyph *GetCachedGlyph(font *f, u32 codepoint) {
  if (!f || !f->face)
    return NULL;

  u32 index = codepoint % GLYPH_CACHE_SIZE;
  cached_glyph *glyph = &f->cache[index];

  if (glyph->valid && glyph->codepoint == codepoint) {
    return glyph;
  }

  /* Clear existing */
  if (glyph->bitmap) {
    free(glyph->bitmap);
    glyph->bitmap = NULL;
  }

  /* Get Glyph Index */
  u16 glyph_indices[1];
  u32 cp = codepoint;
  HRESULT hr = IDWriteFontFace_GetGlyphIndices(f->face, &cp, 1, glyph_indices);
  if (FAILED(hr)) {
    Font_Log("Failed to get glyph index for codepoint %u", codepoint);
    return NULL;
  }

  u16 glyph_idx = glyph_indices[0];

  /* Get Metrics */
  DWRITE_GLYPH_METRICS gm;
  IDWriteFontFace_GetDesignGlyphMetrics(f->face, &glyph_idx, 1, &gm, FALSE);

  glyph->advance = (i32)roundf(gm.advanceWidth * f->scale);
  glyph->codepoint = codepoint;

  /* Render Glyph */
  DWRITE_GLYPH_RUN run = {0};
  run.fontFace = f->face;
  run.fontEmSize = f->em_size;
  run.glyphCount = 1;
  run.glyphIndices = &glyph_idx;
  run.glyphAdvances = NULL;
  run.glyphOffsets = NULL;
  run.isSideways = FALSE;
  run.bidiLevel = 0;

  IDWriteGlyphRunAnalysis *analysis = NULL;

  hr = IDWriteFactory_CreateGlyphRunAnalysis(
      g_font_system.factory, &run, 1.0f, /* pixelsPerDip */
      NULL,                              /* transform */
      DWRITE_RENDERING_MODE_CLEARTYPE_NATURAL, DWRITE_MEASURING_MODE_NATURAL,
      0.0f, /* baselineOriginX */
      0.0f, /* baselineOriginY */
      &analysis);

  if (SUCCEEDED(hr)) {
    RECT texture_bounds;
    hr = IDWriteGlyphRunAnalysis_GetAlphaTextureBounds(
        analysis, DWRITE_TEXTURE_CLEARTYPE_3x1, &texture_bounds);

    if (SUCCEEDED(hr)) {
      i32 w = texture_bounds.right - texture_bounds.left;
      i32 h = texture_bounds.bottom - texture_bounds.top;

      if (w > 0 && h > 0) {
        usize raw_size = w * h * 3;
        u8 *raw_alpha = malloc(raw_size);

        if (raw_alpha) {
          memset(raw_alpha, 0, raw_size);
          hr = IDWriteGlyphRunAnalysis_CreateAlphaTexture(
              analysis, DWRITE_TEXTURE_CLEARTYPE_3x1, &texture_bounds,
              raw_alpha, (u32)raw_size);

          if (SUCCEEDED(hr)) {
            glyph->width = w;
            glyph->height = h;
            glyph->bearing_x = texture_bounds.left;
            glyph->bearing_y = -texture_bounds.top;

            glyph->bitmap = malloc(w * h);
            if (glyph->bitmap) {
              for (i32 i = 0; i < w * h; i++) {
                u32 r = raw_alpha[i * 3];
                u32 g = raw_alpha[i * 3 + 1];
                u32 b = raw_alpha[i * 3 + 2];
                glyph->bitmap[i] = (u8)((r + g + b) / 3);
              }
            }
          } else {
            Font_Log("CreateAlphaTexture failed 0x%X", (unsigned int)hr);
          }
          free(raw_alpha);
        }
      } else {
        // Font_Log("Empty texture bounds for glyph %u: L%d T%d R%d B%d",
        // codepoint, texture_bounds.left, texture_bounds.top,
        // texture_bounds.right, texture_bounds.bottom);
        glyph->width = 0;
        glyph->height = 0;
        glyph->bitmap = NULL;
      }
    } else {
      Font_Log("GetAlphaTextureBounds failed 0x%X", (unsigned int)hr);
    }
    IDWriteGlyphRunAnalysis_Release(analysis);
  } else {
    Font_Log("CreateGlyphRunAnalysis failed 0x%X", (unsigned int)hr);
  }

  glyph->valid = true;
  return glyph;
}

/* ===== Text Measurement ===== */

i32 Font_MeasureWidth(font *f, const char *text) {
  if (!f || !text)
    return 0;

  i32 width = 0;
  const u8 *p = (const u8 *)text;

  while (*p) {
    u32 codepoint = *p++;
    cached_glyph *g = GetCachedGlyph(f, codepoint);
    if (g) {
      width += g->advance;
    }
  }
  return width;
}

void Font_MeasureText(font *f, const char *text, i32 *out_width,
                      i32 *out_height) {
  if (out_width)
    *out_width = Font_MeasureWidth(f, text);
  if (out_height)
    *out_height = f ? f->line_height : 0;
}

/* ===== Text Rendering ===== */

void Font_RenderText(font *f, u32 *pixels, i32 fb_width, i32 fb_height,
                     i32 stride, i32 x, i32 y, const char *text, u8 cr, u8 cg,
                     u8 cb, u8 ca, i32 clip_x, i32 clip_y, i32 clip_w,
                     i32 clip_h) {
  if (!f || !pixels || !text || !*text)
    return;

  /* Font_Log("RenderText: '%s' at %d,%d, col=%d,%d,%d,%d", text, x, y, cr, cg,
   * cb, ca); */

  i32 pen_x = x;
  i32 baseline_y = y + f->ascender;
  const u8 *p = (const u8 *)text;

  /* Clip bounds */
  i32 clip_x0 = clip_x;
  i32 clip_y0 = clip_y;
  i32 clip_x1 = clip_x + clip_w;
  i32 clip_y1 = clip_y + clip_h;

  /*
   * If clip_w or clip_h is <= 0, we treat it as "no right/bottom clipping
   * limit" (or limit to screen size), rather than "empty rect" which would
   * happen if we used it as is. Standard software renderer passes clip {0, 0,
   * w, h} which is correct. But we also check against screen bounds.
   */
  if (clip_x1 <= clip_x0)
    clip_x1 = fb_width;
  if (clip_y1 <= clip_y0)
    clip_y1 = fb_height;

  if (clip_x0 < 0)
    clip_x0 = 0;
  if (clip_y0 < 0)
    clip_y0 = 0;

  if (clip_x1 > fb_width)
    clip_x1 = fb_width;
  if (clip_y1 > fb_height)
    clip_y1 = fb_height;

  while (*p) {
    u32 codepoint = *p++;

    cached_glyph *glyph = GetCachedGlyph(f, codepoint);
    if (!glyph)
      continue;

    if (!glyph->bitmap) {
      pen_x += glyph->advance;
      continue;
    }

    i32 gx = pen_x + glyph->bearing_x;
    i32 gy = baseline_y - glyph->bearing_y;

    for (i32 row = 0; row < glyph->height; row++) {
      i32 py = gy + row;
      if (py < clip_y0 || py >= clip_y1)
        continue;
      if (py < 0 || py >= fb_height)
        continue;

      for (i32 col = 0; col < glyph->width; col++) {
        i32 px = gx + col;
        if (px < clip_x0 || px >= clip_x1)
          continue;
        if (px < 0 || px >= fb_width)
          continue;

        u8 alpha = glyph->bitmap[row * glyph->width + col];
        if (alpha == 0)
          continue;

        u32 *dst = &pixels[py * stride + px];
        u32 bg = *dst;
        u32 bg_r = (bg >> 16) & 0xFF;
        u32 bg_g = (bg >> 8) & 0xFF;
        u32 bg_b = bg & 0xFF;

        u32 final_alpha = ((u32)alpha * (u32)ca) / 255;
        u32 inv_alpha = 255 - final_alpha;

        u32 out_r = ((u32)cr * final_alpha + bg_r * inv_alpha) / 255;
        u32 out_g = ((u32)cg * final_alpha + bg_g * inv_alpha) / 255;
        u32 out_b = ((u32)cb * final_alpha + bg_b * inv_alpha) / 255;

        *dst = (0xFFu << 24) | (out_r << 16) | (out_g << 8) | out_b;
      }
    }

    pen_x += glyph->advance;
  }
}

b32 Font_GetGlyphBitmap(font *f, u32 codepoint, glyph_bitmap *out_glyph) {
  cached_glyph *g = GetCachedGlyph(f, codepoint);
  if (!g)
    return false;

  out_glyph->bitmap = g->bitmap;
  out_glyph->width = g->width;
  out_glyph->height = g->height;
  out_glyph->bearing_x = g->bearing_x;
  out_glyph->bearing_y = g->bearing_y;
  out_glyph->advance = g->advance;
  return true;
}
#endif /* _WIN32 */
