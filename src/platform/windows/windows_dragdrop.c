#ifdef _WIN32
/*
 * windows_dragdrop.c - Native external file drag-and-drop (Windows)
 *
 * Uses OLE DoDragDrop with a custom IDataObject exposing CF_HDROP.
 * C99, handmade hero style.
 */

#include "windows_internal.h"
#include <objidl.h>
#include <shlobj.h>
#include <stdlib.h>
#include <string.h>

typedef struct wb_data_object_s wb_data_object;
typedef struct wb_enum_format_etc_s wb_enum_format_etc;
typedef struct wb_drop_source_s wb_drop_source;

struct wb_data_object_s {
  IDataObject iface;
  LONG ref_count;
  HGLOBAL hdrop_data;
  HGLOBAL effect_data;
  UINT cf_preferred_drop_effect;
  FORMATETC formats[2];
  ULONG format_count;
};

struct wb_enum_format_etc_s {
  IEnumFORMATETC iface;
  LONG ref_count;
  FORMATETC formats[2];
  ULONG format_count;
  ULONG index;
};

struct wb_drop_source_s {
  IDropSource iface;
  LONG ref_count;
};

/* ===== Forward Declarations ===== */

static HRESULT STDMETHODCALLTYPE
WbDataObject_QueryInterface(IDataObject *This, REFIID riid, void **ppvObject);
static ULONG STDMETHODCALLTYPE WbDataObject_AddRef(IDataObject *This);
static ULONG STDMETHODCALLTYPE WbDataObject_Release(IDataObject *This);
static HRESULT STDMETHODCALLTYPE WbDataObject_GetData(IDataObject *This,
                                                      FORMATETC *pformatetcIn,
                                                      STGMEDIUM *pmedium);
static HRESULT STDMETHODCALLTYPE WbDataObject_GetDataHere(
    IDataObject *This, FORMATETC *pformatetc, STGMEDIUM *pmedium);
static HRESULT STDMETHODCALLTYPE WbDataObject_QueryGetData(
    IDataObject *This, FORMATETC *pformatetc);
static HRESULT STDMETHODCALLTYPE WbDataObject_GetCanonicalFormatEtc(
    IDataObject *This, FORMATETC *pformatectIn, FORMATETC *pformatetcOut);
static HRESULT STDMETHODCALLTYPE
WbDataObject_SetData(IDataObject *This, FORMATETC *pformatetc, STGMEDIUM *pmedium,
                     BOOL fRelease);
static HRESULT STDMETHODCALLTYPE WbDataObject_EnumFormatEtc(
    IDataObject *This, DWORD dwDirection, IEnumFORMATETC **ppenumFormatEtc);
static HRESULT STDMETHODCALLTYPE WbDataObject_DAdvise(
    IDataObject *This, FORMATETC *pformatetc, DWORD advf,
    IAdviseSink *pAdvSink, DWORD *pdwConnection);
static HRESULT STDMETHODCALLTYPE WbDataObject_DUnadvise(IDataObject *This,
                                                        DWORD dwConnection);
static HRESULT STDMETHODCALLTYPE
WbDataObject_EnumDAdvise(IDataObject *This, IEnumSTATDATA **ppenumAdvise);

static HRESULT STDMETHODCALLTYPE WbEnumFormatEtc_QueryInterface(
    IEnumFORMATETC *This, REFIID riid, void **ppvObject);
static ULONG STDMETHODCALLTYPE WbEnumFormatEtc_AddRef(IEnumFORMATETC *This);
static ULONG STDMETHODCALLTYPE WbEnumFormatEtc_Release(IEnumFORMATETC *This);
static HRESULT STDMETHODCALLTYPE
WbEnumFormatEtc_Next(IEnumFORMATETC *This, ULONG celt, FORMATETC *rgelt,
                     ULONG *pceltFetched);
static HRESULT STDMETHODCALLTYPE WbEnumFormatEtc_Skip(IEnumFORMATETC *This,
                                                      ULONG celt);
static HRESULT STDMETHODCALLTYPE WbEnumFormatEtc_Reset(IEnumFORMATETC *This);
static HRESULT STDMETHODCALLTYPE WbEnumFormatEtc_Clone(IEnumFORMATETC *This,
                                                       IEnumFORMATETC **ppenum);

static HRESULT STDMETHODCALLTYPE
WbDropSource_QueryInterface(IDropSource *This, REFIID riid, void **ppvObject);
static ULONG STDMETHODCALLTYPE WbDropSource_AddRef(IDropSource *This);
static ULONG STDMETHODCALLTYPE WbDropSource_Release(IDropSource *This);
static HRESULT STDMETHODCALLTYPE WbDropSource_QueryContinueDrag(
    IDropSource *This, BOOL fEscapePressed, DWORD grfKeyState);
static HRESULT STDMETHODCALLTYPE WbDropSource_GiveFeedback(IDropSource *This,
                                                           DWORD dwEffect);

static IDataObjectVtbl g_wb_data_object_vtbl = {
    WbDataObject_QueryInterface,      WbDataObject_AddRef,
    WbDataObject_Release,             WbDataObject_GetData,
    WbDataObject_GetDataHere,         WbDataObject_QueryGetData,
    WbDataObject_GetCanonicalFormatEtc, WbDataObject_SetData,
    WbDataObject_EnumFormatEtc,       WbDataObject_DAdvise,
    WbDataObject_DUnadvise,           WbDataObject_EnumDAdvise,
};

static IEnumFORMATETCVtbl g_wb_enum_format_etc_vtbl = {
    WbEnumFormatEtc_QueryInterface, WbEnumFormatEtc_AddRef,
    WbEnumFormatEtc_Release,        WbEnumFormatEtc_Next,
    WbEnumFormatEtc_Skip,           WbEnumFormatEtc_Reset,
    WbEnumFormatEtc_Clone,
};

static IDropSourceVtbl g_wb_drop_source_vtbl = {
    WbDropSource_QueryInterface,
    WbDropSource_AddRef,
    WbDropSource_Release,
    WbDropSource_QueryContinueDrag,
    WbDropSource_GiveFeedback,
};

/* ===== Helpers ===== */

static HGLOBAL DuplicateGlobalMemory(HGLOBAL source) {
  if (!source) {
    return NULL;
  }

  SIZE_T size = GlobalSize(source);
  if (size == 0) {
    return NULL;
  }

  void *src_ptr = GlobalLock(source);
  if (!src_ptr) {
    return NULL;
  }

  HGLOBAL copy = GlobalAlloc(GMEM_MOVEABLE, size);
  if (!copy) {
    GlobalUnlock(source);
    return NULL;
  }

  void *dst_ptr = GlobalLock(copy);
  if (!dst_ptr) {
    GlobalUnlock(source);
    GlobalFree(copy);
    return NULL;
  }

  memcpy(dst_ptr, src_ptr, size);
  GlobalUnlock(copy);
  GlobalUnlock(source);
  return copy;
}

static HGLOBAL CreateDropFilesGlobal(const char **paths, i32 count) {
  usize total_wchars = 0;

  for (i32 i = 0; i < count; i++) {
    int len = MultiByteToWideChar(CP_UTF8, 0, paths[i], -1, NULL, 0);
    if (len <= 0) {
      return NULL;
    }
    total_wchars += (usize)len;
  }

  total_wchars += 1; /* Double-null terminator */

  usize total_size = sizeof(DROPFILES) + total_wchars * sizeof(wchar_t);
  HGLOBAL global = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, total_size);
  if (!global) {
    return NULL;
  }

  DROPFILES *df = (DROPFILES *)GlobalLock(global);
  if (!df) {
    GlobalFree(global);
    return NULL;
  }

  df->pFiles = sizeof(DROPFILES);
  df->pt.x = 0;
  df->pt.y = 0;
  df->fNC = FALSE;
  df->fWide = TRUE;

  wchar_t *dest = (wchar_t *)((BYTE *)df + sizeof(DROPFILES));
  for (i32 i = 0; i < count; i++) {
    int written = MultiByteToWideChar(CP_UTF8, 0, paths[i], -1, dest,
                                      (int)(total_wchars));
    if (written <= 0) {
      GlobalUnlock(global);
      GlobalFree(global);
      return NULL;
    }

    dest += written;
    total_wchars -= (usize)written;
  }

  *dest = L'\0';
  GlobalUnlock(global);
  return global;
}

static HGLOBAL CreateDropEffectGlobal(b32 copy_only) {
  HGLOBAL global = GlobalAlloc(GMEM_MOVEABLE, sizeof(DWORD));
  if (!global) {
    return NULL;
  }

  DWORD *effect = (DWORD *)GlobalLock(global);
  if (!effect) {
    GlobalFree(global);
    return NULL;
  }

  *effect = copy_only ? DROPEFFECT_COPY : (DROPEFFECT_COPY | DROPEFFECT_MOVE);
  GlobalUnlock(global);
  return global;
}

static wb_data_object *WbDataObject_Create(const char **paths, i32 count,
                                           b32 copy_only) {
  wb_data_object *obj = (wb_data_object *)calloc(1, sizeof(*obj));
  if (!obj) {
    return NULL;
  }

  obj->iface.lpVtbl = &g_wb_data_object_vtbl;
  obj->ref_count = 1;
  obj->hdrop_data = CreateDropFilesGlobal(paths, count);
  if (!obj->hdrop_data) {
    free(obj);
    return NULL;
  }

  obj->cf_preferred_drop_effect = RegisterClipboardFormat(CFSTR_PREFERREDDROPEFFECT);
  obj->effect_data = CreateDropEffectGlobal(copy_only);

  obj->formats[0].cfFormat = CF_HDROP;
  obj->formats[0].ptd = NULL;
  obj->formats[0].dwAspect = DVASPECT_CONTENT;
  obj->formats[0].lindex = -1;
  obj->formats[0].tymed = TYMED_HGLOBAL;
  obj->format_count = 1;

  if (obj->cf_preferred_drop_effect != 0 && obj->effect_data) {
    obj->formats[1].cfFormat = (CLIPFORMAT)obj->cf_preferred_drop_effect;
    obj->formats[1].ptd = NULL;
    obj->formats[1].dwAspect = DVASPECT_CONTENT;
    obj->formats[1].lindex = -1;
    obj->formats[1].tymed = TYMED_HGLOBAL;
    obj->format_count = 2;
  }

  return obj;
}

static wb_drop_source *WbDropSource_Create(void) {
  wb_drop_source *src = (wb_drop_source *)calloc(1, sizeof(*src));
  if (!src) {
    return NULL;
  }

  src->iface.lpVtbl = &g_wb_drop_source_vtbl;
  src->ref_count = 1;
  return src;
}

/* ===== IDataObject ===== */

static HRESULT STDMETHODCALLTYPE
WbDataObject_QueryInterface(IDataObject *This, REFIID riid, void **ppvObject) {
  if (!ppvObject) {
    return E_POINTER;
  }

  if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IDataObject)) {
    *ppvObject = This;
    WbDataObject_AddRef(This);
    return S_OK;
  }

  *ppvObject = NULL;
  return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE WbDataObject_AddRef(IDataObject *This) {
  wb_data_object *self = (wb_data_object *)This;
  return (ULONG)InterlockedIncrement(&self->ref_count);
}

static ULONG STDMETHODCALLTYPE WbDataObject_Release(IDataObject *This) {
  wb_data_object *self = (wb_data_object *)This;
  LONG refs = InterlockedDecrement(&self->ref_count);
  if (refs == 0) {
    if (self->hdrop_data) {
      GlobalFree(self->hdrop_data);
    }
    if (self->effect_data) {
      GlobalFree(self->effect_data);
    }
    free(self);
  }
  return (ULONG)refs;
}

static HRESULT STDMETHODCALLTYPE WbDataObject_GetData(IDataObject *This,
                                                      FORMATETC *pformatetcIn,
                                                      STGMEDIUM *pmedium) {
  wb_data_object *self = (wb_data_object *)This;
  if (!pformatetcIn || !pmedium) {
    return E_POINTER;
  }

  if (!(pformatetcIn->tymed & TYMED_HGLOBAL) ||
      pformatetcIn->dwAspect != DVASPECT_CONTENT ||
      pformatetcIn->lindex != -1) {
    return DV_E_FORMATETC;
  }

  HGLOBAL copy = NULL;

  if (pformatetcIn->cfFormat == CF_HDROP) {
    copy = DuplicateGlobalMemory(self->hdrop_data);
  } else if (self->cf_preferred_drop_effect != 0 &&
             pformatetcIn->cfFormat ==
                 (CLIPFORMAT)self->cf_preferred_drop_effect &&
             self->effect_data) {
    copy = DuplicateGlobalMemory(self->effect_data);
  } else {
    return DV_E_FORMATETC;
  }

  if (!copy) {
    return STG_E_MEDIUMFULL;
  }

  pmedium->tymed = TYMED_HGLOBAL;
  pmedium->hGlobal = copy;
  pmedium->pUnkForRelease = NULL;
  return S_OK;
}

static HRESULT STDMETHODCALLTYPE WbDataObject_GetDataHere(
    IDataObject *This, FORMATETC *pformatetc, STGMEDIUM *pmedium) {
  (void)This;
  (void)pformatetc;
  (void)pmedium;
  return DATA_E_FORMATETC;
}

static HRESULT STDMETHODCALLTYPE WbDataObject_QueryGetData(IDataObject *This,
                                                           FORMATETC *pformatetc) {
  wb_data_object *self = (wb_data_object *)This;
  if (!pformatetc) {
    return E_POINTER;
  }

  if (!(pformatetc->tymed & TYMED_HGLOBAL) ||
      pformatetc->dwAspect != DVASPECT_CONTENT || pformatetc->lindex != -1) {
    return DV_E_FORMATETC;
  }

  if (pformatetc->cfFormat == CF_HDROP) {
    return S_OK;
  }

  if (self->cf_preferred_drop_effect != 0 &&
      pformatetc->cfFormat == (CLIPFORMAT)self->cf_preferred_drop_effect &&
      self->effect_data) {
    return S_OK;
  }

  return DV_E_FORMATETC;
}

static HRESULT STDMETHODCALLTYPE WbDataObject_GetCanonicalFormatEtc(
    IDataObject *This, FORMATETC *pformatectIn, FORMATETC *pformatetcOut) {
  (void)This;
  (void)pformatectIn;
  if (pformatetcOut) {
    pformatetcOut->ptd = NULL;
  }
  return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE
WbDataObject_SetData(IDataObject *This, FORMATETC *pformatetc, STGMEDIUM *pmedium,
                     BOOL fRelease) {
  (void)This;
  (void)pformatetc;
  (void)pmedium;
  (void)fRelease;
  return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE WbDataObject_EnumFormatEtc(
    IDataObject *This, DWORD dwDirection, IEnumFORMATETC **ppenumFormatEtc) {
  wb_data_object *self = (wb_data_object *)This;
  if (!ppenumFormatEtc) {
    return E_POINTER;
  }

  *ppenumFormatEtc = NULL;

  if (dwDirection != DATADIR_GET) {
    return E_NOTIMPL;
  }

  wb_enum_format_etc *enumerator =
      (wb_enum_format_etc *)calloc(1, sizeof(*enumerator));
  if (!enumerator) {
    return E_OUTOFMEMORY;
  }

  enumerator->iface.lpVtbl = &g_wb_enum_format_etc_vtbl;
  enumerator->ref_count = 1;
  enumerator->format_count = self->format_count;
  for (ULONG i = 0; i < self->format_count; i++) {
    enumerator->formats[i] = self->formats[i];
  }

  *ppenumFormatEtc = &enumerator->iface;
  return S_OK;
}

static HRESULT STDMETHODCALLTYPE WbDataObject_DAdvise(
    IDataObject *This, FORMATETC *pformatetc, DWORD advf,
    IAdviseSink *pAdvSink, DWORD *pdwConnection) {
  (void)This;
  (void)pformatetc;
  (void)advf;
  (void)pAdvSink;
  (void)pdwConnection;
  return OLE_E_ADVISENOTSUPPORTED;
}

static HRESULT STDMETHODCALLTYPE WbDataObject_DUnadvise(IDataObject *This,
                                                        DWORD dwConnection) {
  (void)This;
  (void)dwConnection;
  return OLE_E_ADVISENOTSUPPORTED;
}

static HRESULT STDMETHODCALLTYPE
WbDataObject_EnumDAdvise(IDataObject *This, IEnumSTATDATA **ppenumAdvise) {
  (void)This;
  (void)ppenumAdvise;
  return OLE_E_ADVISENOTSUPPORTED;
}

/* ===== IEnumFORMATETC ===== */

static HRESULT STDMETHODCALLTYPE WbEnumFormatEtc_QueryInterface(
    IEnumFORMATETC *This, REFIID riid, void **ppvObject) {
  if (!ppvObject) {
    return E_POINTER;
  }

  if (IsEqualIID(riid, &IID_IUnknown) ||
      IsEqualIID(riid, &IID_IEnumFORMATETC)) {
    *ppvObject = This;
    WbEnumFormatEtc_AddRef(This);
    return S_OK;
  }

  *ppvObject = NULL;
  return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE WbEnumFormatEtc_AddRef(IEnumFORMATETC *This) {
  wb_enum_format_etc *self = (wb_enum_format_etc *)This;
  return (ULONG)InterlockedIncrement(&self->ref_count);
}

static ULONG STDMETHODCALLTYPE WbEnumFormatEtc_Release(IEnumFORMATETC *This) {
  wb_enum_format_etc *self = (wb_enum_format_etc *)This;
  LONG refs = InterlockedDecrement(&self->ref_count);
  if (refs == 0) {
    free(self);
  }
  return (ULONG)refs;
}

static HRESULT STDMETHODCALLTYPE
WbEnumFormatEtc_Next(IEnumFORMATETC *This, ULONG celt, FORMATETC *rgelt,
                     ULONG *pceltFetched) {
  wb_enum_format_etc *self = (wb_enum_format_etc *)This;
  if (!rgelt) {
    return E_POINTER;
  }
  if (celt > 1 && !pceltFetched) {
    return E_INVALIDARG;
  }

  ULONG fetched = 0;
  while (fetched < celt && self->index < self->format_count) {
    rgelt[fetched] = self->formats[self->index];
    fetched++;
    self->index++;
  }

  if (pceltFetched) {
    *pceltFetched = fetched;
  }

  return (fetched == celt) ? S_OK : S_FALSE;
}

static HRESULT STDMETHODCALLTYPE WbEnumFormatEtc_Skip(IEnumFORMATETC *This,
                                                      ULONG celt) {
  wb_enum_format_etc *self = (wb_enum_format_etc *)This;
  ULONG remaining = self->format_count - self->index;
  if (celt > remaining) {
    self->index = self->format_count;
    return S_FALSE;
  }

  self->index += celt;
  return S_OK;
}

static HRESULT STDMETHODCALLTYPE WbEnumFormatEtc_Reset(IEnumFORMATETC *This) {
  wb_enum_format_etc *self = (wb_enum_format_etc *)This;
  self->index = 0;
  return S_OK;
}

static HRESULT STDMETHODCALLTYPE WbEnumFormatEtc_Clone(IEnumFORMATETC *This,
                                                       IEnumFORMATETC **ppenum) {
  wb_enum_format_etc *self = (wb_enum_format_etc *)This;
  if (!ppenum) {
    return E_POINTER;
  }

  wb_enum_format_etc *clone = (wb_enum_format_etc *)calloc(1, sizeof(*clone));
  if (!clone) {
    return E_OUTOFMEMORY;
  }

  clone->iface.lpVtbl = &g_wb_enum_format_etc_vtbl;
  clone->ref_count = 1;
  clone->format_count = self->format_count;
  clone->index = self->index;
  for (ULONG i = 0; i < self->format_count; i++) {
    clone->formats[i] = self->formats[i];
  }

  *ppenum = &clone->iface;
  return S_OK;
}

/* ===== IDropSource ===== */

static HRESULT STDMETHODCALLTYPE
WbDropSource_QueryInterface(IDropSource *This, REFIID riid, void **ppvObject) {
  if (!ppvObject) {
    return E_POINTER;
  }

  if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IDropSource)) {
    *ppvObject = This;
    WbDropSource_AddRef(This);
    return S_OK;
  }

  *ppvObject = NULL;
  return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE WbDropSource_AddRef(IDropSource *This) {
  wb_drop_source *self = (wb_drop_source *)This;
  return (ULONG)InterlockedIncrement(&self->ref_count);
}

static ULONG STDMETHODCALLTYPE WbDropSource_Release(IDropSource *This) {
  wb_drop_source *self = (wb_drop_source *)This;
  LONG refs = InterlockedDecrement(&self->ref_count);
  if (refs == 0) {
    free(self);
  }
  return (ULONG)refs;
}

static HRESULT STDMETHODCALLTYPE WbDropSource_QueryContinueDrag(
    IDropSource *This, BOOL fEscapePressed, DWORD grfKeyState) {
  (void)This;

  if (fEscapePressed) {
    return DRAGDROP_S_CANCEL;
  }

  if ((grfKeyState & MK_LBUTTON) == 0) {
    return DRAGDROP_S_DROP;
  }

  return S_OK;
}

static HRESULT STDMETHODCALLTYPE WbDropSource_GiveFeedback(IDropSource *This,
                                                           DWORD dwEffect) {
  (void)This;
  (void)dwEffect;
  return DRAGDROP_S_USEDEFAULTCURSORS;
}

/* ===== Platform API ===== */

b32 Platform_StartExternalFileDrag(const char **paths, i32 count,
                                   b32 copy_only) {
  if (!paths || count <= 0 || count > 1024) {
    return false;
  }

  if (!g_platform.initialized || !g_platform.ole_initialized) {
    return false;
  }

  wb_data_object *data_obj = WbDataObject_Create(paths, count, copy_only);
  if (!data_obj) {
    return false;
  }

  wb_drop_source *drop_source = WbDropSource_Create();
  if (!drop_source) {
    WbDataObject_Release(&data_obj->iface);
    return false;
  }

  DWORD allowed = copy_only ? DROPEFFECT_COPY : (DROPEFFECT_COPY | DROPEFFECT_MOVE);
  DWORD effect = DROPEFFECT_NONE;
  HRESULT hr =
      DoDragDrop(&data_obj->iface, &drop_source->iface, allowed, &effect);

  WbDropSource_Release(&drop_source->iface);
  WbDataObject_Release(&data_obj->iface);

  return (hr == DRAGDROP_S_DROP || hr == DRAGDROP_S_CANCEL);
}
#endif /* _WIN32 */
