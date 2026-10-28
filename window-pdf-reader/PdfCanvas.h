#pragma once

#include <wx/wx.h>
#include <wx/scrolwin.h>
#include "PdfEngine.h"
#include <vector>
#include <unordered_map>
#include <deque>
#include <functional>

namespace pdf {

enum class ZoomMode {
    Manual,
    FitWidth,
    FitPage
};

struct PageRect {
    int x{0};
    int y{0};
    int width{0};
    int height{0};

    [[nodiscard]] bool Intersects(int top, int bottom) const noexcept {
        return (y + height >= top) && (y <= bottom);
    }
};

class PdfCanvas : public wxScrolledCanvas {
public:
    using PageChangedCallback = std::function<void(int currentPage, int totalPages)>;
    using ZoomChangedCallback = std::function<void(double zoomFactor, ZoomMode mode)>;

    explicit PdfCanvas(wxWindow* parent, std::shared_ptr<PdfEngine> engine);
    ~PdfCanvas() override = default;

    // Document operations
    void OnDocumentOpened();
    void OnDocumentClosed();

    // Page navigation (continuous scrolling supported)
    bool SetPage(int pageIndex, bool alignTop = true);
    [[nodiscard]] int GetCurrentPage() const noexcept { return m_currentPage; }
    [[nodiscard]] int GetTotalPages() const noexcept;
    void NextPage();
    void PreviousPage();
    void FirstPage();
    void LastPage();

    // Zoom controls
    void SetZoomMode(ZoomMode mode);
    [[nodiscard]] ZoomMode GetZoomMode() const noexcept { return m_zoomMode; }
    void SetZoomFactor(double factor, bool preserveCenter = true);
    [[nodiscard]] double GetZoomFactor() const noexcept { return m_zoomFactor; }
    void ZoomIn();
    void ZoomOut();
    void ZoomActualSize();

    // Rotation controls (0 = 0 deg, 1 = 90 deg, 2 = 180 deg, 3 = 270 deg)
    void RotateClockwise();
    void RotateCounterClockwise();
    [[nodiscard]] int GetRotation() const noexcept { return m_rotation; }

    // Callbacks for UI updates
    void SetOnPageChanged(PageChangedCallback cb) { m_onPageChanged = std::move(cb); }
    void SetOnZoomChanged(ZoomChangedCallback cb) { m_onZoomChanged = std::move(cb); }

    // Watermark operations
    void SetWatermarkFilter(bool enable);
    [[nodiscard]] bool IsWatermarkFilterEnabled() const;
    void ClearCacheAndRefresh();

    // Page metadata query
    [[nodiscard]] PageSize GetCurrentPageOriginalSize() const;

private:
    void OnPaint(wxPaintEvent& event);
    void OnSize(wxSizeEvent& event);
    void OnMouseWheel(wxMouseEvent& event);
    void OnKeyDown(wxKeyEvent& event);
    void OnScroll(wxScrollWinEvent& event);
    void OnContextMenu(wxContextMenuEvent& event);

    void RecomputeContinuousLayout();
    void UpdateCurrentPageFromScroll();
    wxBitmap GetOrRenderPageBitmap(int pageIndex, int targetWidth, int targetHeight);
    void ClearBitmapCache();

    double CalculateFitWidthZoom() const;
    double CalculateFitPageZoom() const;

    std::shared_ptr<PdfEngine> m_engine;
    int m_currentPage{0};
    int m_rotation{0}; // 0: 0°, 1: 90°, 2: 180°, 3: 270°
    double m_zoomFactor{1.0};
    ZoomMode m_zoomMode{ZoomMode::FitWidth};

    // Continuous layout page geometries
    std::vector<PageRect> m_pageRects;
    int m_totalVirtualHeight{0};
    int m_maxPageWidth{0};

    // LRU Bitmap Cache to keep RAM footprint strictly minimal (~20MB)
    static constexpr size_t MAX_CACHED_PAGES = 6;
    std::unordered_map<int, wxBitmap> m_bitmapCache;
    std::deque<int> m_lruOrder;

    PageChangedCallback m_onPageChanged;
    ZoomChangedCallback m_onZoomChanged;

    static constexpr int PADDING_X = 24;
    static constexpr int PADDING_Y = 18;
    static constexpr int PAGE_GAP = 16;
    static constexpr double MIN_ZOOM = 0.25;
    static constexpr double MAX_ZOOM = 5.0;
    static constexpr double ZOOM_STEP = 1.25;

    wxDECLARE_EVENT_TABLE();
};

} // namespace pdf
