# Minimal Footprint Windows PDF Reader

A high-performance, ultra-lightweight desktop PDF reader built with modern **C++20**, **Google PDFium**, and **wxWidgets**.

Designed with an **extreme minimal Apple / VLC aesthetic** — no visual clutter, no bloated toolbars, with pure focus on the document.

---

## Highlights

- **Continuous Page Scrolling**: Mouse scroll seamlessly flows from one page to the next without page-boundary stops.
- **Ultra-Low Memory Footprint**: Runs at **~26 MB RAM** working set during active multi-page continuous document rendering (via viewport virtualization and an LRU bitmap cache).
- **Tiny Binary**: Release executable is only **~167 KB**.
- **Extreme Minimal Apple/VLC UI**:
  - Zero bloated top toolbars eating up screen real estate.
  - 95%+ of the window is pure, uninterrupted PDF content.
  - Compact 34px dark control bar at the bottom:
    - Left: Subtle document filename.
    - Center: Clean `‹` `1 / 42` `›` page indicator (click to jump to page).
    - Right: `-` `Fit (100%)` `+` `[ ]` `R` minimal flat controls.
  - Right-click context menu on canvas for instant access to zoom, rotation, and document properties.
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
  - Drag & Drop: Drop any `.pdf` directly into the window from Windows Explorer.

---

## Building and Running

### Run Pre-built Release Binary
```powershell
.\x64\Release\window-pdf-reader.exe "D:\your_document.pdf"
```

### Build from MSBuild / Terminal
```powershell
# Build Release x64
& "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" window-pdf-reader.slnx /p:Configuration=Release /p:Platform=x64

# Build Debug x64
& "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" window-pdf-reader.slnx /p:Configuration=Debug /p:Platform=x64
```
