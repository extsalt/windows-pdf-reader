#pragma once

#include <wx/wx.h>
#include <wx/dnd.h>
#include <memory>
#include "PdfEngine.h"
#include "PdfCanvas.h"

namespace pdf {

class MainFrame : public wxFrame {
public:
    MainFrame(const wxString& title, const wxPoint& pos, const wxSize& size);
    ~MainFrame() override = default;

    bool OpenDocument(const wxString& filePath);
    void CloseDocument();

private:
    void SetupMenuBar();
    void SetupMinimalBottomBar();
    void SetupDropTarget();

    // Menu Event Handlers
    void OnMenuOpen(wxCommandEvent& event);
    void OnMenuClose(wxCommandEvent& event);
    void OnMenuProperties(wxCommandEvent& event);
    void OnMenuExit(wxCommandEvent& event);

    void OnMenuZoomIn(wxCommandEvent& event);
    void OnMenuZoomOut(wxCommandEvent& event);
    void OnMenuZoom100(wxCommandEvent& event);
    void OnMenuFitWidth(wxCommandEvent& event);
    void OnMenuFitPage(wxCommandEvent& event);
    void OnMenuRotateCW(wxCommandEvent& event);
    void OnMenuRotateCCW(wxCommandEvent& event);

    void OnMenuFirstPage(wxCommandEvent& event);
    void OnMenuPrevPage(wxCommandEvent& event);
    void OnMenuNextPage(wxCommandEvent& event);
    void OnMenuLastPage(wxCommandEvent& event);
    void OnMenuGoToPage(wxCommandEvent& event);

    void OnMenuAbout(wxCommandEvent& event);

    // Bottom Bar Click Handlers
    void OnPageLabelClicked(wxMouseEvent& event);
    void OnZoomLabelClicked(wxMouseEvent& event);

    // UI State updates
    void UpdateControlsState();
    void UpdateBottomBar(int currentPage, int totalPages, double zoomFactor, ZoomMode mode);

    // Drop Target Helper
    class FileDropTarget : public wxFileDropTarget {
    public:
        explicit FileDropTarget(MainFrame* frame) : m_frame(frame) {}
        bool OnDropFiles(wxCoord x, wxCoord y, const wxArrayString& filenames) override;

    private:
        MainFrame* m_frame;
    };

    std::shared_ptr<PdfEngine> m_engine;
    PdfCanvas* m_canvas{nullptr};

    // Minimal VLC-style bottom control bar
    wxPanel* m_bottomBar{nullptr};
    wxStaticText* m_fileLabel{nullptr};
    wxButton* m_btnPrev{nullptr};
    wxStaticText* m_pageLabel{nullptr};
    wxButton* m_btnNext{nullptr};
    wxButton* m_btnZoomOut{nullptr};
    wxStaticText* m_zoomLabel{nullptr};
    wxButton* m_btnZoomIn{nullptr};
    wxButton* m_btnFit{nullptr};
    wxButton* m_btnRotate{nullptr};

    wxString m_currentFilePath;

    enum MenuIds {
        ID_PROPERTIES = wxID_HIGHEST + 1,
        ID_ZOOM_IN,
        ID_ZOOM_OUT,
        ID_ZOOM_100,
        ID_FIT_WIDTH,
        ID_FIT_PAGE,
        ID_ROTATE_CW,
        ID_ROTATE_CCW,
        ID_FIRST_PAGE,
        ID_PREV_PAGE,
        ID_NEXT_PAGE,
        ID_LAST_PAGE,
        ID_GOTO_PAGE,
        ID_BTN_PREV,
        ID_BTN_NEXT,
        ID_BTN_ZOOM_OUT,
        ID_BTN_ZOOM_IN,
        ID_BTN_FIT,
        ID_BTN_ROTATE
    };

    wxDECLARE_EVENT_TABLE();
};

} // namespace pdf
