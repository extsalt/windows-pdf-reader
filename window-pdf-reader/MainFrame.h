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
    ~MainFrame() override;

    bool OpenDocument(const wxString& filePath);
    void CloseDocument();

    static bool RegisterShellContextMenu(wxString* errorMsg = nullptr);
    static bool UnregisterShellContextMenu(wxString* errorMsg = nullptr);
    static bool IsShellContextMenuRegistered();

#ifdef __WXMSW__
protected:
    WXLRESULT MSWWindowProc(WXUINT message, WXWPARAM wParam, WXLPARAM lParam) override;
#endif

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

    // Tools Menu Event Handlers
    void OnMenuToggleRemoveWatermark(wxCommandEvent& event);
    void OnMenuPurgeWatermarks(wxCommandEvent& event);
    void OnMenuSaveCleanedPdf(wxCommandEvent& event);
    void OnMenuRegisterShell(wxCommandEvent& event);
    void OnMenuUnregisterShell(wxCommandEvent& event);

    void OnMenuAbout(wxCommandEvent& event);

    // Edit / Search Menu Event Handlers
    void OnMenuFind(wxCommandEvent& event);
    void OnMenuFindNext(wxCommandEvent& event);
    void OnMenuFindPrev(wxCommandEvent& event);
    void OnEscape(wxCommandEvent& event);

    // Floating Bar Methods & Click Handlers
    void UpdateFloatingBarPosition();
    void ShowMenuBar(bool show);
    void ShowFloatingBar();
    void HideFloatingBar();
    void OnCanvasDoubleClicked();
    void OnSize(wxSizeEvent& event);
    void OnPageLabelClicked(wxMouseEvent& event);
    void OnZoomLabelClicked(wxMouseEvent& event);

    // Search Bar Methods & Handlers
    void SetupFloatingSearchBar();
    void UpdateSearchBarPosition();
    void ShowSearchBar();
    void HideSearchBar();
    void OnSearchTextChanged(wxCommandEvent& event);
    void OnSearchTextEnter(wxCommandEvent& event);
    void OnSearchPrev();
    void OnSearchNext();
    void OnSearchClose();

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
    wxButton* m_btnPrev{nullptr};
    wxStaticText* m_pageLabel{nullptr};
    wxButton* m_btnNext{nullptr};
    wxButton* m_btnZoomOut{nullptr};
    wxStaticText* m_zoomLabel{nullptr};
    wxButton* m_btnZoomIn{nullptr};
    wxButton* m_btnFit{nullptr};
    wxButton* m_btnRotate{nullptr};

    // Minimal floating search HUD bar
    wxPanel* m_searchBar{nullptr};
    wxTextCtrl* m_searchCtrl{nullptr};
    wxStaticText* m_searchCountLabel{nullptr};
    wxButton* m_btnSearchPrev{nullptr};
    wxButton* m_btnSearchNext{nullptr};
    wxButton* m_btnSearchClose{nullptr};

    // Tools menu item pointers
    wxMenuItem* m_menuItemRemoveWatermark{nullptr};
    wxMenuItem* m_menuItemPurgeWatermarks{nullptr};
    wxMenuItem* m_menuItemSaveCleaned{nullptr};

#ifdef __WXMSW__
    WXHMENU m_savedMenuHandle{nullptr};
#endif
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
        ID_TOOL_REMOVE_WATERMARK,
        ID_TOOL_PURGE_WATERMARKS,
        ID_TOOL_SAVE_CLEANED,
        ID_TOOL_REGISTER_SHELL,
        ID_TOOL_UNREGISTER_SHELL,
        ID_BTN_PREV,
        ID_BTN_NEXT,
        ID_BTN_ZOOM_OUT,
        ID_BTN_ZOOM_IN,
        ID_BTN_FIT,
        ID_BTN_ROTATE,
        ID_FIND,
        ID_FIND_NEXT,
        ID_FIND_PREV,
        ID_ESCAPE,
        ID_BTN_SEARCH_PREV,
        ID_BTN_SEARCH_NEXT,
        ID_BTN_SEARCH_CLOSE
    };

    wxDECLARE_EVENT_TABLE();
};

} // namespace pdf
