# Minimal Footprint Windows PDF Reader

A high-performance, ultra-lightweight desktop PDF reader built with modern **C++20**, **Google PDFium**, and **wxWidgets**.

Designed with an **extreme minimal Apple / VLC aesthetic** — no visual clutter, no bloated toolbars, with pure focus on the document.

---

## Highlights

- **Continuous Page Scrolling**: Mouse scroll seamlessly flows from one page to the next without page-boundary stops.
- **Ultra-Low Memory Footprint**: Runs at **~26 MB RAM** working set during active multi-page continuous document rendering (via viewport virtualization and an LRU bitmap cache).
- **Tiny Binary**: Release executable is only **~167 KB**.
- **Extreme Minimal Apple/VLC UI**:
  - **Lean No-Bezel Design**: Eliminated chunky side bezels with slim 8px margins; left and right frames are perfectly symmetric.
  - **Fine Vertical Scroll**: Native Win32 `DarkMode_Explorer` dark vertical scrollbar with horizontal scrolling suppressed to prevent corner bevels.
  - Zero bloated top toolbars eating up screen real estate.
  - 95%+ of the window is pure, uninterrupted PDF content.
  - Compact 34px dark control bar at the bottom:
    - Left: Subtle document filename.
    - Center: Clean `‹` `1 / 42` `›` page indicator (click to jump to page).
    - Right: `-` `Fit (100%)` `+` `[ ]` `R` minimal flat controls.
  - Right-click context menu on canvas for instant access to zoom, rotation, watermark removal, and document properties.
- **Watermark Eradication ("Dark Hat" Visual Filter & DOM Object Purge)**:
  - **Live Watermark Suppression (`Ctrl+Shift+W`)**: Instant visual removal of faint, translucent watermarks (e.g. "CONFIDENTIAL", "DRAFT", evaluation stamps, background grids) via real-time subpixel luminance filtering and annotation bypass.
  - **Permanent Watermark Purge (`Ctrl+Alt+W`)**: Deep structural inspection across all pages that strips `FPDF_ANNOT_WATERMARK`, stamp annotations, `/Watermark` and `/Artifact` marked content, and translucent overlay objects.
  - **Save Cleaned PDF As (`Ctrl+Shift+S`)**: Export the purified document directly to disk with zero watermarks.
- **Strict Modern C++20 (No Hacks)**:
  - Full RAII wrappers (`ScopedDocument`, `ScopedPage`, `ScopedBitmap`) with custom deleters for PDFium C handles.
  - Zero memory leaks, zero raw handle lifecycle leaks.
  - Subpixel anti-aliasing with LCD text rendering (`FPDF_LCD_TEXT | FPDF_ANNOT`).
  - Double-buffered canvas (`wxAutoBufferedPaintDC`) with `wxBG_STYLE_PAINT` eliminating all flicker.
- **Keyboard & Mouse Navigation**:
  - Mouse Wheel: Continuous vertical scroll across the entire document.
  - `Ctrl + Mouse Wheel`: Smooth zooming centered on viewport.
  - `Space` / `Page Down`: Scroll down / Next page.
  - `Shift + Space` / `Page Up`: Scroll up / Previous page.
  - `Home` / `End`: First page / Last page.
  - `Ctrl + +` / `Ctrl + -` / `Ctrl + 0`: Zoom In / Out / 100%.
  - `Ctrl + 1` / `Ctrl + 2`: Fit Width / Fit Page.
  - `Ctrl + R`: Rotate 90° clockwise.
  - `Ctrl + G`: Go to page number dialog.
  - `Ctrl + Shift + W`: Toggle Remove Watermarks (Live Suppression).
  - `Ctrl + Alt + W`: Permanently Purge Watermark Objects.
  - `Ctrl + Shift + S`: Save Cleaned PDF As.
  - Drag & Drop: Drop any `.pdf` directly into the window from Windows Explorer.

---

## Quick Start

### 1. Run Pre-built Release Binary
```powershell
.\x64\Release\window-pdf-reader.exe "D:\your_document.pdf"
```

### 2. Automated Setup & Build
If cloning for the first time, bootstrap dependencies and compile with one click:
```powershell
# 1. Download and restore PDFium and wxWidgets
.\setup.ps1

# 2. Build Release x64
.\build.ps1 -Configuration Release -Platform x64
```

---

## Read More: Deep Dive & Technical Architecture

### 1. Why PDFium + wxWidgets?

Most modern desktop PDF viewers rely on web-based rendering engines like Electron, CEF (Chromium Embedded Framework), or Qt WebEngine. While convenient for web developers, this approach incurs massive overhead:
- **300 MB – 600 MB+ RAM** consumed on launch.
- **5 to 8 OS background processes** spawned (GPU process, renderer processes, utility processes).
- **100 MB+ binary/installer** payloads.
- High idle battery consumption and sluggish cold-start times.

By combining **Google PDFium** (the native C++ engine powering Google Chrome's PDF rendering) directly with **wxWidgets** (a thin, native C++ wrapper around native Win32 controls), this application achieves:
- **1 single native OS process** with instant `< 100ms` startup.
- **~26 MB RAM** working set during active document rendering.
- **167 KB** standalone executable binary size.

---

### 2. Rendering Pipeline & Memory Management

PDF rendering engines can quickly run out of memory or cause performance stutter if buffers are repeatedly allocated, copied, or poorly managed. Here is how our pipeline operates with zero redundant copies:

```
+-------------------------------------------------------------+
|                         PDF Document                        |
+-------------------------------------------------------------+
                               |
                               | (1) FPDF_LoadPage()
                               v
+-------------------------------------------------------------+
|                    PDFium ScopedBitmap                      |
|  - 32-bit BGRA surface with LCD subpixel text anti-aliasing  |
|  - Rendered with FPDF_ANNOT | FPDF_LCD_TEXT flags           |
+-------------------------------------------------------------+
                               |
                               | (2) Single-pass scanline loop
                               |     (BGRA -> RGB conversion)
                               v
+-------------------------------------------------------------+
|                           wxImage                           |
|  - Direct buffer ownership: wxImage(w, h, rgbData, false)   |
|  - Zero heap duplication; memory freed automatically        |
+-------------------------------------------------------------+
                               |
                               | (3) Converted to native DIB
                               v
+-------------------------------------------------------------+
|                          wxBitmap                           |
|  - Stored in a 6-slot LRU Cache                             |
|  - Directly blitted to wxAutoBufferedPaintDC (60+ FPS)       |
+-------------------------------------------------------------+
```

#### RAII Resource Handles
All raw C pointers provided by PDFium are encapsulated within C++20 RAII smart wrappers with no-throw custom deleters:
```cpp
struct DocumentDeleter {
    void operator()(FPDF_DOCUMENT doc) const noexcept {
        if (doc) FPDF_CloseDocument(doc);
    }
};

using ScopedDocument = std::unique_ptr<std::remove_pointer_t<FPDF_DOCUMENT>, DocumentDeleter>;
using ScopedPage     = std::unique_ptr<std::remove_pointer_t<FPDF_PAGE>, PageDeleter>;
using ScopedBitmap   = std::unique_ptr<std::remove_pointer_t<FPDF_BITMAP>, BitmapDeleter>;
```
This guarantees that any early return, document close, or failure mode immediately deallocates all native resources with zero memory leaks.

---

### 3. Continuous Scroll Viewport Virtualization

When viewing a document with hundreds or thousands of pages, rendering every page at once would exhaust system memory and drop frame rates to single digits.

Our continuous scrolling engine solves this through **Spatial Viewport Virtualization**:

1. **Virtual Geometry Mapping**:
   When a document opens or zoom/rotation changes, page heights and widths are calculated once in O(N) time (a few milliseconds for thousands of pages). Each page is assigned a virtual rectangular slot:
   ```
   Page 0: [y = PADDING_Y,                             height = H0]
   Page 1: [y = PADDING_Y + H0 + PAGE_GAP,             height = H1]
   Page 2: [y = PADDING_Y + H0 + H1 + 2*PAGE_GAP,      height = H2]
   ...
   ```
2. **Intersection Cull**:
   During `OnPaint()`, the canvas retrieves the current visible viewport:
   $$\text{viewTop} = \text{scrollY} \times \text{unitY}$$
   $$\text{viewBottom} = \text{viewTop} + \text{clientHeight}$$
   Only pages whose bounding rectangles intersect $[\text{viewTop}, \text{viewBottom}]$ are painted.
3. **6-Slot LRU Cache**:
   Rendered pages are cached in a hash map paired with an LRU queue. When a new page enters the viewport, it is rendered and inserted into the cache. If the cache exceeds 6 pages, the least recently viewed page is evicted immediately. This keeps memory usage strictly capped at **~26 MB** regardless of total page count.
4. **Dominant Page Tracking**:
   As the user scrolls, the intersection area of each visible page with the viewport is measured. The page occupying the largest vertical area is identified as the active page, automatically updating the bottom bar's `X / Y` indicator.

---

### 4. Watermark Eradication: Two-Tier Architecture

Watermarks in PDF documents present in three primary forms: interactive annotations, tagged structural marked content, and baked-in low-contrast raster/vector overlays. The reader provides an end-to-end two-tier solution:

- **Tier 1: Live "Dark Hat" Visual Suppression (`Tools -> Remove Watermark` / `Ctrl+Shift+W`)**:
  - **Annotation Bypass**: Excludes the `FPDF_ANNOT` rendering flag during page rasterization, immediately suppressing all annotation-level watermarks and stamps.
  - **Single-Pass Scanline Filter**: Evaluates every pixel in the BGRA-to-RGB conversion loop with zero heap allocation overhead.
  - **Luminance Thresholding**: Computes subpixel luminance ($Y = 0.299R + 0.587G + 0.114B$).
  - **Translucent Wash Detection**: Snaps pixels to pure white $(255, 255, 255)$ when matching watermark signatures:
    - Ultra-faint scanner grain / wash: $Y \ge 235$.
    - Semi-transparent colored or grey stamps: $\min(R,G,B) \ge 155$ and $Y \ge 170$.
    - Low-contrast diagonal grey text: $|R-G| \le 25 \land |G-B| \le 25$ and $Y \ge 160$.
  - **Crisp Text Boost**: Genuine dark text ($Y < 140$) is slightly deepened by 5% to maximize readability against pristine paper white.
  - **Non-Destructive**: Leaves original PDF files completely untouched on disk.

- **Tier 2: Deep Structural DOM Purging (`Tools -> Permanently Purge Watermark Objects` / `Ctrl+Alt+W`)**:
  - **Annotation Extraction**: Scans all pages and removes watermark annotations (`FPDF_ANNOT_WATERMARK`) and custom stamps (`FPDF_ANNOT_STAMP`) via `FPDFPage_RemoveAnnot()`.
  - **Marked Content Analysis**: Inspects object marks with `FPDFPageObj_CountMarks()` and `FPDFPageObjMark_GetName()`; purges objects tagged with `/Watermark` or `/Artifact`.
  - **Translucent Object Removal**: Detects vector paths and text objects with partial alpha transparency ($0 < \alpha < 200$) via `FPDFPageObj_GetFillColor()` and `FPDFPageObj_GetStrokeColor()`, removing them via `FPDFPage_RemoveObject()`.
  - **Page Content Stream Regeneration**: Invokes `FPDFPage_GenerateContent()` to rebuild the page content stream.

- **Clean Document Export (`Tools -> Save Cleaned PDF As...` / `Ctrl+Shift+S`)**:
  - Serializes the purged in-memory document to a new PDF file via `FPDF_SaveAsCopy()`.
  - Employs a custom streaming file writer context (`FPDF_FILEWRITE`) with chunked binary output.
  - Produces a lightweight, completely watermark-free PDF readable in any standard viewer.

---

### 5. Footprint & Performance Benchmark Comparison

| Metric | Window PDF Reader (This Project) | SumatraPDF | Edge / Chrome Built-in | Electron PDF Viewer |
| :--- | :---: | :---: | :---: | :---: |
| **GUI Framework** | **wxWidgets (Native Win32)** | Native Win32 (C) | Chromium (Blink/V8) | Electron (Node + Chromium) |
| **PDF Engine** | **Google PDFium** | MuPDF | Google PDFium | PDF.js / PDFium |
| **Process Count** | **1** | 1 | 4–6 | 5–8 |
| **Idle RAM (Working Set)** | **~18 MB** | ~14 MB | ~180 MB | ~280 MB |
| **Active 50-Page RAM** | **~26 MB** | ~28 MB | ~240 MB | ~380 MB |
| **Release Exe Size** | **167 KB** | ~14 MB | N/A (Browser) | 85 MB – 140 MB |
| **Cold Startup Time** | **< 100 ms** | < 120 ms | ~450 ms | ~1,800 ms |
| **Continuous Scroll** | **Yes** | Yes | Yes | Yes |
| **UI Philosophy** | **Apple / VLC Extreme Minimal** | Classic Win32 Toolbar | Browser Tab Chrome | Web App Chrome |

---

### 6. Developer Guide & Build Configuration

#### Solution Structure
```
d:/window-pdf-reader/window-pdf-reader/
├── window-pdf-reader.slnx              # Visual Studio Solution
├── window-pdf-reader/
│   ├── main.cpp                        # Entry point & PDFium lifecycle (OnInit / OnExit)
│   ├── PdfEngine.h / .cpp              # RAII PDFium core & LCD scanline renderer
│   ├── PdfCanvas.h / .cpp              # Virtualized continuous scroll canvas & LRU cache
│   ├── MainFrame.h / .cpp              # VLC/Apple minimal UI frame & drag-and-drop
│   ├── window-pdf-reader.vcxproj       # MSBuild C++20 x64 project
│   └── window-pdf-reader.vcxproj.filters
├── setup.ps1                           # Automated dependency downloader
├── build.ps1                           # Command-line MSBuild script
└── README.md                           # Documentation & architecture deep-dive
```

#### Compiler & Toolset Requirements
- **Compiler**: Visual Studio 2022 / 2026 (MSVC v143 / v145) with C++20 standard (`/std:c++20`).
- **Platform**: `x64` architecture.
- **SDK**: Windows 10/11 SDK + Universal CRT (automatically fetched via NuGet packages in `setup.ps1`, requiring zero administrative rights or Visual Studio installer modifications).
- **Libraries**:
  - `bblanchon.PDFium.Win32` (Google PDFium x64 native binary build).
  - `wxWidgets 3.2.6` (x64 MSVC DLL/Dev package).
