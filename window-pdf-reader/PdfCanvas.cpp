#include "PdfCanvas.h"
#include <wx/dcbuffer.h>
#include <wx/menu.h>
#include <algorithm>
#include <cmath>

#ifdef __WXMSW__
#include <windows.h>
#pragma comment(lib, "msimg32.lib")

namespace {

void DrawHighlightRect(HDC hdc, const wxRect& r, COLORREF fillCol, BYTE fillAlpha, COLORREF borderCol) {
    if (!hdc || r.width <= 0 || r.height <= 0) return;

    HDC memDC = CreateCompatibleDC(hdc);
    if (!memDC) return;

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = r.width;
    bmi.bmiHeader.biHeight = -r.height; // top-down DIB
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP dib = CreateDIBSection(memDC, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (dib && bits) {
        HGDIOBJ oldBmp = SelectObject(memDC, dib);

        const BYTE red = GetRValue(fillCol);
        const BYTE green = GetGValue(fillCol);
        const BYTE blue = GetBValue(fillCol);

        // Pre-multiplied alpha for AC_SRC_OVER with AC_SRC_ALPHA
        const BYTE a = fillAlpha;
        const BYTE pr = static_cast<BYTE>((static_cast<unsigned int>(red) * a) / 255);
        const BYTE pg = static_cast<BYTE>((static_cast<unsigned int>(green) * a) / 255);
        const BYTE pb = static_cast<BYTE>((static_cast<unsigned int>(blue) * a) / 255);

        const uint32_t pixel = (static_cast<uint32_t>(a) << 24) |
                               (static_cast<uint32_t>(pr) << 16) |
                               (static_cast<uint32_t>(pg) << 8) |
                               (static_cast<uint32_t>(pb));

        uint32_t* pPixels = static_cast<uint32_t*>(bits);
        const size_t totalPixels = static_cast<size_t>(r.width) * static_cast<size_t>(r.height);
        std::fill_n(pPixels, totalPixels, pixel);

        BLENDFUNCTION bf{};
        bf.BlendOp = AC_SRC_OVER;
        bf.BlendFlags = 0;
        bf.SourceConstantAlpha = 255;
        bf.AlphaFormat = AC_SRC_ALPHA;

        AlphaBlend(hdc, r.x, r.y, r.width, r.height,
                   memDC, 0, 0, r.width, r.height, bf);

        SelectObject(memDC, oldBmp);
        DeleteObject(dib);
    }
    DeleteDC(memDC);

    // Draw crisp 1px border around the highlight
    HPEN pen = CreatePen(PS_SOLID, 1, borderCol);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(hdc, r.x, r.y, r.x + r.width, r.y + r.height);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

} // namespace
#endif

namespace pdf {

wxBEGIN_EVENT_TABLE(PdfCanvas, wxScrolledCanvas)
    EVT_PAINT(PdfCanvas::OnPaint)
    EVT_SIZE(PdfCanvas::OnSize)
    EVT_MOUSEWHEEL(PdfCanvas::OnMouseWheel)
    EVT_LEFT_DCLICK(PdfCanvas::OnLeftDClick)
    EVT_KEY_DOWN(PdfCanvas::OnKeyDown)
    EVT_SCROLLWIN(PdfCanvas::OnScroll)
    EVT_CONTEXT_MENU(PdfCanvas::OnContextMenu)
wxEND_EVENT_TABLE()

PdfCanvas::PdfCanvas(wxWindow* parent, std::shared_ptr<PdfEngine> engine)
    : wxScrolledCanvas(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL | wxBORDER_NONE),
      m_engine(std::move(engine)) {
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetScrollRate(0, 16);
    ShowScrollbars(wxSHOW_SB_NEVER, wxSHOW_SB_ALWAYS);
}

int PdfCanvas::GetTotalPages() const noexcept {
    return m_engine ? m_engine->GetPageCount() : 0;
}

PageSize PdfCanvas::GetCurrentPageOriginalSize() const {
    if (!m_engine || !m_engine->IsLoaded()) {
        return {};
    }
    return m_engine->GetPageSize(m_currentPage);
}

void PdfCanvas::OnDocumentOpened() {
    m_currentPage = 0;
    m_rotation = 0;
    ClearBitmapCache();
    RecomputeContinuousLayout();

    // Scroll to the very top
    Scroll(0, 0);
    Refresh();

    if (m_onPageChanged) {
        m_onPageChanged(1, GetTotalPages());
    }
    if (m_onZoomChanged) {
        m_onZoomChanged(m_zoomFactor, m_zoomMode);
    }
}

void PdfCanvas::OnDocumentClosed() {
    m_currentPage = 0;
    ClearBitmapCache();
    ClearSearchResults();
    m_pageRects.clear();
    m_totalVirtualHeight = 0;
    m_maxPageWidth = 0;
    SetVirtualSize(1, 1);
    Refresh();

    if (m_onPageChanged) {
        m_onPageChanged(0, 0);
    }
}

void PdfCanvas::ClearBitmapCache() {
    m_bitmapCache.clear();
    m_lruOrder.clear();
}

void PdfCanvas::SetWatermarkFilter(bool enable) {
    if (m_engine) {
        m_engine->SetWatermarkFilter(enable);
        ClearCacheAndRefresh();
    }
}

bool PdfCanvas::IsWatermarkFilterEnabled() const {
    return m_engine ? m_engine->IsWatermarkFilterEnabled() : false;
}

void PdfCanvas::ClearCacheAndRefresh() {
    ClearBitmapCache();
    Refresh(false);
    Update();
}

void PdfCanvas::RecomputeContinuousLayout() {
    if (!m_engine || !m_engine->IsLoaded()) {
        m_pageRects.clear();
        m_totalVirtualHeight = 0;
        m_maxPageWidth = 0;
        SetVirtualSize(1, 1);
        return;
    }

    const int pageCount = m_engine->GetPageCount();
    if (pageCount <= 0) return;

    if (m_zoomMode == ZoomMode::FitWidth) {
        m_zoomFactor = CalculateFitWidthZoom();
    } else if (m_zoomMode == ZoomMode::FitPage) {
        m_zoomFactor = CalculateFitPageZoom();
    }

    m_pageRects.resize(pageCount);
    const bool isRotated = (m_rotation == 1 || m_rotation == 3);

    int currentY = PADDING_Y;
    int maxW = 0;

    for (int i = 0; i < pageCount; ++i) {
        const PageSize size = m_engine->GetPageSize(i);
        const double wPts = isRotated ? size.heightPoints : size.widthPoints;
        const double hPts = isRotated ? size.widthPoints : size.heightPoints;

        const int w = std::max(1, static_cast<int>(std::round(wPts * m_zoomFactor)));
        const int h = std::max(1, static_cast<int>(std::round(hPts * m_zoomFactor)));

        m_pageRects[i].width = w;
        m_pageRects[i].height = h;
        m_pageRects[i].y = currentY;

        currentY += h + PAGE_GAP;
        maxW = std::max(maxW, w);
    }

    m_totalVirtualHeight = currentY - PAGE_GAP + PADDING_Y;
    m_maxPageWidth = maxW;

    const wxSize clientSize = GetClientSize();
    const int clientW = clientSize.GetWidth();
    const int virtualWidth = std::max(maxW + (PADDING_X * 2), clientW);

    if (virtualWidth > clientW) {
        SetScrollRate(16, 16);
    } else {
        SetScrollRate(0, 16);
    }

    // Center each page horizontally with symmetric left and right margins
    for (int i = 0; i < pageCount; ++i) {
        m_pageRects[i].x = std::max(PADDING_X, (virtualWidth - m_pageRects[i].width) / 2);
    }

    SetVirtualSize(virtualWidth, m_totalVirtualHeight);
    RecomputeSearchDeviceRects();
}

bool PdfCanvas::SetPage(int pageIndex, bool alignTop) {
    if (!m_engine || !m_engine->IsLoaded()) return false;
    const int pageCount = m_engine->GetPageCount();
    if (pageIndex < 0 || pageIndex >= pageCount) return false;

    if (pageIndex < static_cast<int>(m_pageRects.size())) {
        int rateX = 16, rateY = 16;
        GetScrollPixelsPerUnit(&rateX, &rateY);

        const int targetY = std::max(0, m_pageRects[pageIndex].y - PADDING_Y);
        const int scrollY = rateY > 0 ? (targetY / rateY) : 0;

        int curX = 0, curY = 0;
        GetViewStart(&curX, &curY);

        if (alignTop) {
            Scroll(curX, scrollY);
        }

        m_currentPage = pageIndex;
        if (m_onPageChanged) {
            m_onPageChanged(m_currentPage + 1, pageCount);
        }
        Refresh();
        return true;
    }

    return false;
}

void PdfCanvas::NextPage() {
    if (m_currentPage + 1 < GetTotalPages()) {
        SetPage(m_currentPage + 1);
    }
}

void PdfCanvas::PreviousPage() {
    if (m_currentPage > 0) {
        SetPage(m_currentPage - 1);
    }
}

void PdfCanvas::FirstPage() {
    SetPage(0);
}

void PdfCanvas::LastPage() {
    if (GetTotalPages() > 0) {
        SetPage(GetTotalPages() - 1);
    }
}

void PdfCanvas::SetZoomMode(ZoomMode mode) {
    m_zoomMode = mode;
    ClearBitmapCache();
    RecomputeContinuousLayout();

    if (m_onZoomChanged) {
        m_onZoomChanged(m_zoomFactor, m_zoomMode);
    }
    Refresh();
}

void PdfCanvas::SetZoomFactor(double factor, bool preserveCenter) {
    const double newZoom = std::clamp(factor, MIN_ZOOM, MAX_ZOOM);
    if (std::abs(newZoom - m_zoomFactor) < 0.001 && m_zoomMode == ZoomMode::Manual) {
        return;
    }

    int rateX = 16, rateY = 16;
    GetScrollPixelsPerUnit(&rateX, &rateY);

    // Calculate normalized vertical center point to preserve scroll position
    int curX = 0, curY = 0;
    GetViewStart(&curX, &curY);
    const int clientH = GetClientSize().GetHeight();
    const double centerDocY = (curY * rateY) + (clientH / 2.0);
    const double ratio = (m_totalVirtualHeight > 0) ? (centerDocY / m_totalVirtualHeight) : 0.0;

    m_zoomFactor = newZoom;
    m_zoomMode = ZoomMode::Manual;

    ClearBitmapCache();
    RecomputeContinuousLayout();

    if (preserveCenter && m_totalVirtualHeight > 0 && rateY > 0) {
        const double newCenterDocY = ratio * m_totalVirtualHeight;
        const int newScrollY = std::max(0, static_cast<int>(std::round(newCenterDocY - (clientH / 2.0))) / rateY);
        Scroll(curX, newScrollY);
    }

    if (m_onZoomChanged) {
        m_onZoomChanged(m_zoomFactor, m_zoomMode);
    }
    Refresh();
}

void PdfCanvas::ZoomIn() {
    SetZoomFactor(m_zoomFactor * ZOOM_STEP);
}

void PdfCanvas::ZoomOut() {
    SetZoomFactor(m_zoomFactor / ZOOM_STEP);
}

void PdfCanvas::ZoomActualSize() {
    SetZoomFactor(1.0);
}

void PdfCanvas::RotateClockwise() {
    m_rotation = (m_rotation + 1) % 4;
    ClearBitmapCache();
    RecomputeContinuousLayout();
    Refresh();
}

void PdfCanvas::RotateCounterClockwise() {
    m_rotation = (m_rotation + 3) % 4;
    ClearBitmapCache();
    RecomputeContinuousLayout();
    Refresh();
}

double PdfCanvas::CalculateFitWidthZoom() const {
    if (!m_engine || !m_engine->IsLoaded()) return 1.0;

    const int pageCount = m_engine->GetPageCount();
    if (pageCount <= 0) return 1.0;

    const bool isRotated = (m_rotation == 1 || m_rotation == 3);
    const int pageIndex = std::clamp(m_currentPage, 0, pageCount - 1);
    const PageSize size = m_engine->GetPageSize(pageIndex);
    if (!size.IsValid()) return 1.0;

    const double pageWidthPts = isRotated ? size.heightPoints : size.widthPoints;
    const wxSize clientSize = GetClientSize();
    // Available width without double-counting scrollbar (clientSize already excludes vertical scrollbar)
    const int availableWidth = std::max(50, clientSize.GetWidth() - (PADDING_X * 2));

    const double factor = static_cast<double>(availableWidth) / pageWidthPts;
    return std::clamp(factor, MIN_ZOOM, MAX_ZOOM);
}

double PdfCanvas::CalculateFitPageZoom() const {
    if (!m_engine || !m_engine->IsLoaded()) return 1.0;

    const int pageCount = m_engine->GetPageCount();
    if (pageCount <= 0) return 1.0;

    const bool isRotated = (m_rotation == 1 || m_rotation == 3);
    const int pageIndex = std::clamp(m_currentPage, 0, pageCount - 1);
    const PageSize size = m_engine->GetPageSize(pageIndex);
    if (!size.IsValid()) return 1.0;

    const double pageWidthPts = isRotated ? size.heightPoints : size.widthPoints;
    const double pageHeightPts = isRotated ? size.widthPoints : size.heightPoints;

    const wxSize clientSize = GetClientSize();
    const int availableWidth = std::max(50, clientSize.GetWidth() - (PADDING_X * 2));
    const int availableHeight = std::max(50, clientSize.GetHeight() - (PADDING_Y * 2));

    const double factorW = static_cast<double>(availableWidth) / pageWidthPts;
    const double factorH = static_cast<double>(availableHeight) / pageHeightPts;
    return std::clamp(std::min(factorW, factorH), MIN_ZOOM, MAX_ZOOM);
}

wxBitmap PdfCanvas::GetOrRenderPageBitmap(int pageIndex, int targetWidth, int targetHeight) {
    const auto it = m_bitmapCache.find(pageIndex);
    if (it != m_bitmapCache.end() && it->second.IsOk()) {
        // Move to back of LRU order
        m_lruOrder.erase(std::remove(m_lruOrder.begin(), m_lruOrder.end(), pageIndex), m_lruOrder.end());
        m_lruOrder.push_back(pageIndex);
        return it->second;
    }

    // Evict oldest if cache exceeded
    while (m_bitmapCache.size() >= MAX_CACHED_PAGES && !m_lruOrder.empty()) {
        const int oldest = m_lruOrder.front();
        m_lruOrder.pop_front();
        m_bitmapCache.erase(oldest);
    }

    // Render from PDFium
    wxImage img = m_engine->RenderPageToImage(pageIndex, targetWidth, targetHeight, m_rotation);
    if (!img.IsOk()) {
        return {};
    }

    wxBitmap bmp(img);
    m_bitmapCache[pageIndex] = bmp;
    m_lruOrder.push_back(pageIndex);
    return bmp;
}

void PdfCanvas::UpdateCurrentPageFromScroll() {
    if (!m_engine || !m_engine->IsLoaded() || m_pageRects.empty()) return;

    int rateX = 16, rateY = 16;
    GetScrollPixelsPerUnit(&rateX, &rateY);

    int curX = 0, curY = 0;
    GetViewStart(&curX, &curY);

    const int clientH = GetClientSize().GetHeight();
    const int viewTop = curY * rateY;
    const int viewBottom = viewTop + clientH;

    int bestPage = m_currentPage;
    int maxVisibleH = -1;

    for (size_t i = 0; i < m_pageRects.size(); ++i) {
        const auto& r = m_pageRects[i];
        if (r.Intersects(viewTop, viewBottom)) {
            const int top = std::max(r.y, viewTop);
            const int bottom = std::min(r.y + r.height, viewBottom);
            const int visibleH = bottom - top;
            if (visibleH > maxVisibleH) {
                maxVisibleH = visibleH;
                bestPage = static_cast<int>(i);
            }
        }
    }

    if (bestPage != m_currentPage) {
        m_currentPage = bestPage;
        if (m_onPageChanged) {
            m_onPageChanged(m_currentPage + 1, GetTotalPages());
        }
    }
}

void PdfCanvas::OnPaint(wxPaintEvent& /*event*/) {
    wxAutoBufferedPaintDC dc(this);
    DoPrepareDC(dc);

    const wxSize clientSize = GetClientSize();
    // Sleek Apple dark charcoal canvas
    const wxColour canvasBg(30, 30, 32);
    dc.SetBackground(wxBrush(canvasBg));
    dc.Clear();

    if (!m_engine || !m_engine->IsLoaded() || m_pageRects.empty()) {
        dc.SetTextForeground(wxColour(160, 160, 165));
        const wxString msg = "Drop a PDF file here, double-click, or press Ctrl+O to open";
        const wxSize textSize = dc.GetTextExtent(msg);
        dc.DrawText(msg, std::max(20, (clientSize.GetWidth() - textSize.GetWidth()) / 2),
                         std::max(20, (clientSize.GetHeight() - textSize.GetHeight()) / 2));
        return;
    }

    int rateX = 16, rateY = 16;
    GetScrollPixelsPerUnit(&rateX, &rateY);

    int curX = 0, curY = 0;
    GetViewStart(&curX, &curY);

    const int viewTop = curY * rateY;
    const int viewBottom = viewTop + clientSize.GetHeight();

    // Render only visible pages (viewport virtualization)
    for (size_t i = 0; i < m_pageRects.size(); ++i) {
        const auto& r = m_pageRects[i];
        if (!r.Intersects(viewTop, viewBottom)) {
            continue;
        }

        // Soft subtle page outline shadow (symmetric, no right-side offset bias)
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(wxBrush(wxColour(16, 16, 18)));
        dc.DrawRectangle(r.x, r.y + 1, r.width, r.height + 1);

        // Fetch or render page bitmap
        wxBitmap bmp = GetOrRenderPageBitmap(static_cast<int>(i), r.width, r.height);
        if (bmp.IsOk()) {
            dc.DrawBitmap(bmp, r.x, r.y, false);
        } else {
            // Placeholder white page while loading/rendering
            dc.SetBrush(*wxWHITE_BRUSH);
            dc.SetPen(wxPen(wxColour(200, 200, 200)));
            dc.DrawRectangle(r.x, r.y, r.width, r.height);
        }

        // Crisp 1px subtle page border (Apple-like clean edge)
        dc.SetPen(wxPen(wxColour(50, 50, 55), 1));
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        dc.DrawRectangle(r.x, r.y, r.width, r.height);

        // Draw search highlights on visible page i
        if (!m_searchMatches.empty()) {
#ifdef __WXMSW__
            HDC hdc = static_cast<HDC>(dc.GetHandle());
#endif
            for (size_t m = 0; m < m_searchMatches.size(); ++m) {
                const auto& match = m_searchMatches[m];
                if (match.pageIndex != static_cast<int>(i)) {
                    continue;
                }
                const bool isActive = (static_cast<int>(m) == m_currentMatchIndex);
                // Active match: Amber/Orange (vivid highlight)
                // Other matches: Canary Yellow
                const COLORREF fillCol = isActive ? RGB(255, 140, 0) : RGB(255, 235, 59);
                const BYTE fillAlpha = isActive ? 130 : 80;
                const COLORREF borderCol = isActive ? RGB(230, 81, 0) : RGB(200, 175, 20);

                for (const auto& dRect : match.deviceRects) {
                    wxRect hr = dRect;
                    hr.Inflate(1, 1);
#ifdef __WXMSW__
                    DrawHighlightRect(hdc, hr, fillCol, fillAlpha, borderCol);
#else
                    dc.SetPen(wxPen(wxColour(borderCol), 1));
                    dc.SetBrush(wxBrush(wxColour(fillCol)));
                    dc.DrawRectangle(hr);
#endif
                }
            }
        }
    }
}

void PdfCanvas::OnSize(wxSizeEvent& event) {
    if (m_zoomMode == ZoomMode::FitWidth || m_zoomMode == ZoomMode::FitPage) {
        RecomputeContinuousLayout();
    } else if (m_engine && m_engine->IsLoaded()) {
        const int clientW = GetClientSize().GetWidth();
        const int virtualWidth = std::max(m_maxPageWidth + (PADDING_X * 2), clientW);
        for (auto& r : m_pageRects) {
            r.x = std::max(PADDING_X, (virtualWidth - r.width) / 2);
        }
        SetVirtualSize(virtualWidth, m_totalVirtualHeight);
    }
    Refresh();
    event.Skip();
}

void PdfCanvas::OnScroll(wxScrollWinEvent& event) {
    event.Skip();
    // Update active page indicator as user scrolls continuously
    UpdateCurrentPageFromScroll();
}

void PdfCanvas::OnLeftDClick(wxMouseEvent& event) {
    if (m_onDoubleClicked) {
        m_onDoubleClicked();
    }
    event.Skip();
}

void PdfCanvas::OnMouseWheel(wxMouseEvent& event) {
    if (event.ControlDown()) {
        // Zoom on Ctrl + Wheel
        if (event.GetWheelRotation() > 0) {
            ZoomIn();
        } else {
            ZoomOut();
        }
    } else {
        // Default smooth continuous scrolling through all pages
        event.Skip();
        wxPostEvent(this, wxScrollWinEvent(wxEVT_SCROLLWIN_THUMBTRACK));
    }
}

void PdfCanvas::OnKeyDown(wxKeyEvent& event) {
    const int keyCode = event.GetKeyCode();

    switch (keyCode) {
        case WXK_PAGEDOWN:
        case WXK_SPACE:
            if (!event.ShiftDown()) {
                NextPage();
            } else {
                PreviousPage();
            }
            break;
        case WXK_PAGEUP:
            PreviousPage();
            break;
        case WXK_HOME:
            FirstPage();
            break;
        case WXK_END:
            LastPage();
            break;
        case '+':
        case WXK_NUMPAD_ADD:
            if (event.ControlDown()) ZoomIn();
            break;
        case '-':
        case WXK_NUMPAD_SUBTRACT:
            if (event.ControlDown()) ZoomOut();
            break;
        case '0':
        case WXK_NUMPAD0:
            if (event.ControlDown()) ZoomActualSize();
            break;
        case '1':
            if (event.ControlDown()) SetZoomMode(ZoomMode::FitWidth);
            break;
        case '2':
            if (event.ControlDown()) SetZoomMode(ZoomMode::FitPage);
            break;
        case 'R':
            if (event.ControlDown()) {
                if (event.ShiftDown()) RotateCounterClockwise();
                else RotateClockwise();
            }
            break;
        default:
            event.Skip();
            break;
    }
}

void PdfCanvas::OnContextMenu(wxContextMenuEvent& /*event*/) {
    wxMenu menu;
    const bool hasDoc = (m_engine && m_engine->IsLoaded());

    menu.Append(wxID_OPEN, "Open PDF...\tCtrl+O");
    menu.AppendSeparator();

    auto* mZoomIn = menu.Append(wxID_ANY, "Zoom In\tCtrl++");
    auto* mZoomOut = menu.Append(wxID_ANY, "Zoom Out\tCtrl+-");
    auto* mActual = menu.Append(wxID_ANY, "Actual Size (100%)\tCtrl+0");
    auto* mFitW = menu.Append(wxID_ANY, "Fit Width\tCtrl+1");
    auto* mFitP = menu.Append(wxID_ANY, "Fit Page\tCtrl+2");
    menu.AppendSeparator();
    auto* mRot = menu.Append(wxID_ANY, "Rotate Clockwise\tCtrl+R");
    menu.AppendSeparator();
    auto* mWatermark = menu.AppendCheckItem(wxID_ANY, "Remove Watermarks\tCtrl+Shift+W");
    mWatermark->Check(IsWatermarkFilterEnabled());
    menu.AppendSeparator();
    auto* mProps = menu.Append(wxID_ANY, "Document Properties...\tAlt+Enter");

    mZoomIn->Enable(hasDoc);
    mZoomOut->Enable(hasDoc);
    mActual->Enable(hasDoc);
    mFitW->Enable(hasDoc);
    mFitP->Enable(hasDoc);
    mRot->Enable(hasDoc);
    mWatermark->Enable(hasDoc);
    mProps->Enable(hasDoc);

    menu.Bind(wxEVT_MENU, [this](wxCommandEvent&) {
        wxCommandEvent evt(wxEVT_MENU, wxID_OPEN);
        GetParent()->GetEventHandler()->ProcessEvent(evt);
    }, wxID_OPEN);

    menu.Bind(wxEVT_MENU, [this](wxCommandEvent&) { ZoomIn(); }, mZoomIn->GetId());
    menu.Bind(wxEVT_MENU, [this](wxCommandEvent&) { ZoomOut(); }, mZoomOut->GetId());
    menu.Bind(wxEVT_MENU, [this](wxCommandEvent&) { ZoomActualSize(); }, mActual->GetId());
    menu.Bind(wxEVT_MENU, [this](wxCommandEvent&) { SetZoomMode(ZoomMode::FitWidth); }, mFitW->GetId());
    menu.Bind(wxEVT_MENU, [this](wxCommandEvent&) { SetZoomMode(ZoomMode::FitPage); }, mFitP->GetId());
    menu.Bind(wxEVT_MENU, [this](wxCommandEvent&) { RotateClockwise(); }, mRot->GetId());
    menu.Bind(wxEVT_MENU, [this, mWatermark](wxCommandEvent&) {
        SetWatermarkFilter(mWatermark->IsChecked());
    }, mWatermark->GetId());
    menu.Bind(wxEVT_MENU, [this](wxCommandEvent&) {
        wxCommandEvent evt(wxEVT_MENU, wxID_HIGHEST + 1); // ID_PROPERTIES
        GetParent()->GetEventHandler()->ProcessEvent(evt);
    }, mProps->GetId());

    PopupMenu(&menu);
}

void PdfCanvas::SetSearchResults(const std::vector<SearchMatch>& matches) {
    m_searchMatches.clear();
    m_currentMatchIndex = -1;

    m_searchMatches.reserve(matches.size());
    for (size_t i = 0; i < matches.size(); ++i) {
        CanvasSearchMatch cm;
        cm.pageIndex = matches[i].pageIndex;
        cm.matchIndex = static_cast<int>(i);
        cm.rectsPoints = matches[i].rectsPoints;
        m_searchMatches.push_back(std::move(cm));
    }

    RecomputeSearchDeviceRects();

    if (!m_searchMatches.empty()) {
        m_currentMatchIndex = 0;
        ScrollToCurrentMatch();
    } else {
        Refresh();
    }
}

void PdfCanvas::ClearSearchResults() {
    m_searchMatches.clear();
    m_currentMatchIndex = -1;
    Refresh();
}

bool PdfCanvas::NextSearchMatch() {
    if (m_searchMatches.empty()) return false;
    m_currentMatchIndex = (m_currentMatchIndex + 1) % static_cast<int>(m_searchMatches.size());
    ScrollToCurrentMatch();
    return true;
}

bool PdfCanvas::PreviousSearchMatch() {
    if (m_searchMatches.empty()) return false;
    m_currentMatchIndex = (m_currentMatchIndex - 1 + static_cast<int>(m_searchMatches.size())) % static_cast<int>(m_searchMatches.size());
    ScrollToCurrentMatch();
    return true;
}

bool PdfCanvas::SelectSearchMatch(int index) {
    if (index < 0 || index >= static_cast<int>(m_searchMatches.size())) return false;
    m_currentMatchIndex = index;
    ScrollToCurrentMatch();
    return true;
}

void PdfCanvas::ScrollToCurrentMatch() {
    if (m_currentMatchIndex < 0 || m_currentMatchIndex >= static_cast<int>(m_searchMatches.size())) {
        return;
    }
    const auto& match = m_searchMatches[m_currentMatchIndex];
    if (match.boundingBox.IsEmpty()) return;

    int rateX = 16, rateY = 16;
    GetScrollPixelsPerUnit(&rateX, &rateY);

    const int clientH = GetClientSize().GetHeight();
    const int clientW = GetClientSize().GetWidth();

    int curX = 0, curY = 0;
    GetViewStart(&curX, &curY);

    const int targetY = match.boundingBox.y - (clientH / 2) + (match.boundingBox.height / 2);
    const int clampedY = std::clamp(targetY, 0, std::max(0, m_totalVirtualHeight - clientH));
    const int scrollUnitY = rateY > 0 ? (clampedY / rateY) : 0;

    int scrollUnitX = curX;
    if (rateX > 0 && match.boundingBox.width > 0) {
        const int targetX = match.boundingBox.x - (clientW / 2) + (match.boundingBox.width / 2);
        const int clampedX = std::clamp(targetX, 0, std::max(0, GetVirtualSize().GetWidth() - clientW));
        scrollUnitX = clampedX / rateX;
    }

    Scroll(scrollUnitX, scrollUnitY);

    if (match.pageIndex != m_currentPage) {
        m_currentPage = match.pageIndex;
        if (m_onPageChanged) {
            m_onPageChanged(m_currentPage + 1, GetTotalPages());
        }
    }
    Refresh();
}

void PdfCanvas::RecomputeSearchDeviceRects() {
    if (!m_engine || !m_engine->IsLoaded() || m_searchMatches.empty() || m_pageRects.empty()) {
        return;
    }

    for (auto& match : m_searchMatches) {
        match.deviceRects.clear();
        if (match.pageIndex >= 0 && match.pageIndex < static_cast<int>(m_pageRects.size())) {
            const auto& pr = m_pageRects[match.pageIndex];
            m_engine->ConvertPointsToDevice(match.pageIndex, pr.x, pr.y, pr.width, pr.height, m_rotation,
                                            match.rectsPoints, match.deviceRects);
            if (!match.deviceRects.empty()) {
                wxRect box = match.deviceRects[0];
                for (size_t r = 1; r < match.deviceRects.size(); ++r) {
                    box.Union(match.deviceRects[r]);
                }
                match.boundingBox = box;
            } else {
                match.boundingBox = wxRect();
            }
        }
    }
}

} // namespace pdf
