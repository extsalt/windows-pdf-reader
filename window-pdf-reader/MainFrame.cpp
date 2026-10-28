#include "MainFrame.h"
#include <wx/numdlg.h>
#include <wx/filename.h>
#include <iomanip>
#include <sstream>

#ifdef __WXMSW__
#include <windows.h>
#include <uxtheme.h>
#include <dwmapi.h>
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "dwmapi.lib")

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#endif

namespace pdf {

wxBEGIN_EVENT_TABLE(MainFrame, wxFrame)
    EVT_MENU(wxID_OPEN, MainFrame::OnMenuOpen)
    EVT_MENU(wxID_CLOSE, MainFrame::OnMenuClose)
    EVT_MENU(MainFrame::ID_PROPERTIES, MainFrame::OnMenuProperties)
    EVT_MENU(wxID_EXIT, MainFrame::OnMenuExit)

    EVT_MENU(MainFrame::ID_ZOOM_IN, MainFrame::OnMenuZoomIn)
    EVT_MENU(MainFrame::ID_ZOOM_OUT, MainFrame::OnMenuZoomOut)
    EVT_MENU(MainFrame::ID_ZOOM_100, MainFrame::OnMenuZoom100)
    EVT_MENU(MainFrame::ID_FIT_WIDTH, MainFrame::OnMenuFitWidth)
    EVT_MENU(MainFrame::ID_FIT_PAGE, MainFrame::OnMenuFitPage)
    EVT_MENU(MainFrame::ID_ROTATE_CW, MainFrame::OnMenuRotateCW)
    EVT_MENU(MainFrame::ID_ROTATE_CCW, MainFrame::OnMenuRotateCCW)

    EVT_MENU(MainFrame::ID_FIRST_PAGE, MainFrame::OnMenuFirstPage)
    EVT_MENU(MainFrame::ID_PREV_PAGE, MainFrame::OnMenuPrevPage)
    EVT_MENU(MainFrame::ID_NEXT_PAGE, MainFrame::OnMenuNextPage)
    EVT_MENU(MainFrame::ID_LAST_PAGE, MainFrame::OnMenuLastPage)
    EVT_MENU(MainFrame::ID_GOTO_PAGE, MainFrame::OnMenuGoToPage)

    EVT_MENU(MainFrame::ID_TOOL_REMOVE_WATERMARK, MainFrame::OnMenuToggleRemoveWatermark)
    EVT_MENU(MainFrame::ID_TOOL_PURGE_WATERMARKS, MainFrame::OnMenuPurgeWatermarks)
    EVT_MENU(MainFrame::ID_TOOL_SAVE_CLEANED, MainFrame::OnMenuSaveCleanedPdf)

    EVT_MENU(wxID_ABOUT, MainFrame::OnMenuAbout)
wxEND_EVENT_TABLE()

MainFrame::MainFrame(const wxString& title, const wxPoint& pos, const wxSize& size)
    : wxFrame(nullptr, wxID_ANY, title, pos, size),
      m_engine(std::make_shared<PdfEngine>()) {

    SetMinSize(wxSize(640, 480));
    SetBackgroundColour(wxColour(24, 24, 27));

    // Setup canvas
    m_canvas = new PdfCanvas(this, m_engine);

    // Setup minimal UI
    SetupMenuBar();
    SetupMinimalBottomBar();
    SetupDropTarget();

    // Attach callbacks from canvas
    m_canvas->SetOnPageChanged([this](int cur, int tot) {
        UpdateBottomBar(cur, tot, m_canvas->GetZoomFactor(), m_canvas->GetZoomMode());
        UpdateControlsState();
    });

    m_canvas->SetOnZoomChanged([this](double factor, ZoomMode mode) {
        UpdateBottomBar(m_canvas->GetCurrentPage() + 1, m_canvas->GetTotalPages(), factor, mode);
    });

    // Layout: Canvas takes entire window, minimal 34px bar at bottom
    auto* rootSizer = new wxBoxSizer(wxVERTICAL);
    rootSizer->Add(m_canvas, 1, wxEXPAND);
    rootSizer->Add(m_bottomBar, 0, wxEXPAND);
    SetSizer(rootSizer);

#ifdef __WXMSW__
    HWND hwnd = static_cast<HWND>(GetHWND());
    if (hwnd) {
        BOOL darkMode = TRUE;
        DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkMode, sizeof(darkMode));
        SetWindowTheme(hwnd, L"DarkMode_Explorer", nullptr);
    }
    if (m_canvas) {
        HWND canvasHwnd = static_cast<HWND>(m_canvas->GetHWND());
        if (canvasHwnd) {
            SetWindowTheme(canvasHwnd, L"DarkMode_Explorer", nullptr);
        }
    }
#endif

    UpdateControlsState();
}

void MainFrame::SetupMenuBar() {
    auto* menuBar = new wxMenuBar();

    // File
    auto* menuFile = new wxMenu();
    menuFile->Append(wxID_OPEN, "&Open PDF...\tCtrl+O", "Open a PDF document");
    menuFile->Append(wxID_CLOSE, "&Close\tCtrl+W", "Close current PDF");
    menuFile->AppendSeparator();
    menuFile->Append(ID_PROPERTIES, "&Properties...\tAlt+Enter", "Display document metadata");
    menuFile->AppendSeparator();
    menuFile->Append(wxID_EXIT, "E&xit\tAlt+F4", "Exit application");
    menuBar->Append(menuFile, "&File");

    // View
    auto* menuView = new wxMenu();
    menuView->Append(ID_ZOOM_IN, "Zoom &In\tCtrl++", "Enlarge view");
    menuView->Append(ID_ZOOM_OUT, "Zoom &Out\tCtrl+-", "Shrink view");
    menuView->Append(ID_ZOOM_100, "&Actual Size (100%)\tCtrl+0", "View at 100%");
    menuView->AppendSeparator();
    menuView->Append(ID_FIT_WIDTH, "Fit &Width\tCtrl+1", "Fit page width to window");
    menuView->Append(ID_FIT_PAGE, "Fit &Page\tCtrl+2", "Fit whole page in window");
    menuView->AppendSeparator();
    menuView->Append(ID_ROTATE_CW, "Rotate &Clockwise\tCtrl+R", "Rotate page 90 degrees");
    menuView->Append(ID_ROTATE_CCW, "Rotate &Counter-Clockwise\tCtrl+Shift+R", "Rotate page 90 degrees counter-clockwise");
    menuBar->Append(menuView, "&View");

    // Go
    auto* menuGo = new wxMenu();
    menuGo->Append(ID_PREV_PAGE, "&Previous Page\tPageUp", "Go to previous page");
    menuGo->Append(ID_NEXT_PAGE, "&Next Page\tPageDown", "Go to next page");
    menuGo->AppendSeparator();
    menuGo->Append(ID_FIRST_PAGE, "&First Page\tHome", "Go to first page");
    menuGo->Append(ID_LAST_PAGE, "&Last Page\tEnd", "Go to last page");
    menuGo->AppendSeparator();
    menuGo->Append(ID_GOTO_PAGE, "&Go to Page...\tCtrl+G", "Jump to page number");
    menuBar->Append(menuGo, "&Go");

    // Tools
    auto* menuTools = new wxMenu();
    m_menuItemRemoveWatermark = menuTools->AppendCheckItem(ID_TOOL_REMOVE_WATERMARK, "&Remove Watermark\tCtrl+Shift+W", "Suppress and filter faint watermark overlays and annotations");
    menuTools->AppendSeparator();
    m_menuItemPurgeWatermarks = menuTools->Append(ID_TOOL_PURGE_WATERMARKS, "&Permanently Purge Watermark Objects\tCtrl+Alt+W", "Strip all watermark annotations, marked content, and translucent objects");
    m_menuItemSaveCleaned = menuTools->Append(ID_TOOL_SAVE_CLEANED, "&Save Cleaned PDF As...\tCtrl+Shift+S", "Export current cleaned PDF to a new file without watermarks");
    menuBar->Append(menuTools, "&Tools");

    // Help
    auto* menuHelp = new wxMenu();
    menuHelp->Append(wxID_ABOUT, "&About PDF Reader...\tF1", "About");
    menuBar->Append(menuHelp, "&Help");

    SetMenuBar(menuBar);

    // Keyboard Accelerators
    wxAcceleratorEntry entries[] = {
        { wxACCEL_CTRL, 'O', wxID_OPEN },
        { wxACCEL_CTRL, 'W', wxID_CLOSE },
        { wxACCEL_CTRL, 'G', ID_GOTO_PAGE },
        { wxACCEL_CTRL, '+', ID_ZOOM_IN },
        { wxACCEL_CTRL, '=', ID_ZOOM_IN },
        { wxACCEL_CTRL, '-', ID_ZOOM_OUT },
        { wxACCEL_CTRL, '0', ID_ZOOM_100 },
        { wxACCEL_CTRL, '1', ID_FIT_WIDTH },
        { wxACCEL_CTRL, '2', ID_FIT_PAGE },
        { wxACCEL_CTRL, 'R', ID_ROTATE_CW },
        { wxACCEL_CTRL | wxACCEL_SHIFT, 'W', ID_TOOL_REMOVE_WATERMARK },
        { wxACCEL_CTRL | wxACCEL_ALT, 'W', ID_TOOL_PURGE_WATERMARKS },
        { wxACCEL_CTRL | wxACCEL_SHIFT, 'S', ID_TOOL_SAVE_CLEANED },
        { wxACCEL_NORMAL, WXK_F1, wxID_ABOUT }
    };
    SetAcceleratorTable(wxAcceleratorTable(sizeof(entries) / sizeof(entries[0]), entries));
}

void MainFrame::SetupMinimalBottomBar() {
    // 34px height dark minimal bar (Apple / VLC style)
    m_bottomBar = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(-1, 34));
    m_bottomBar->SetBackgroundColour(wxColour(22, 22, 24));

    auto* barSizer = new wxBoxSizer(wxHORIZONTAL);

    // Left: Document Name
    m_fileLabel = new wxStaticText(m_bottomBar, wxID_ANY, "Ready", wxDefaultPosition, wxDefaultSize, wxST_ELLIPSIZE_END);
    m_fileLabel->SetForegroundColour(wxColour(140, 140, 145));
    wxFont infoFont = m_fileLabel->GetFont();
    infoFont.SetPointSize(9);
    m_fileLabel->SetFont(infoFont);

    // Center: Page navigation controls
    auto* centerSizer = new wxBoxSizer(wxHORIZONTAL);

    auto makeFlatButton = [this](const wxString& label, int id, const wxString& tip) {
        auto* btn = new wxButton(m_bottomBar, id, label, wxDefaultPosition, wxSize(28, 22), wxBORDER_NONE);
        btn->SetBackgroundColour(wxColour(34, 34, 38));
        btn->SetForegroundColour(wxColour(230, 230, 235));
        btn->SetToolTip(tip);
        return btn;
    };

    m_btnPrev = makeFlatButton("<", ID_BTN_PREV, "Previous Page (PageUp)");
    m_pageLabel = new wxStaticText(m_bottomBar, wxID_ANY, "- / -", wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE_HORIZONTAL);
    m_pageLabel->SetForegroundColour(wxColour(230, 230, 235));
    m_pageLabel->SetFont(infoFont.Bold());
    m_pageLabel->SetCursor(wxCursor(wxCURSOR_HAND));
    m_pageLabel->SetToolTip("Click to jump to page (Ctrl+G)");
    m_btnNext = makeFlatButton(">", ID_BTN_NEXT, "Next Page (PageDown)");

    centerSizer->Add(m_btnPrev, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    centerSizer->Add(m_pageLabel, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 6);
    centerSizer->Add(m_btnNext, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 6);

    // Right: Zoom and Rotate controls
    auto* rightSizer = new wxBoxSizer(wxHORIZONTAL);
    m_btnZoomOut = makeFlatButton("-", ID_BTN_ZOOM_OUT, "Zoom Out (Ctrl+-)");
    m_zoomLabel = new wxStaticText(m_bottomBar, wxID_ANY, "Fit Width", wxDefaultPosition, wxSize(68, -1), wxALIGN_CENTRE_HORIZONTAL);
    m_zoomLabel->SetForegroundColour(wxColour(180, 180, 185));
    m_zoomLabel->SetFont(infoFont);
    m_zoomLabel->SetCursor(wxCursor(wxCURSOR_HAND));
    m_zoomLabel->SetToolTip("Click to toggle Fit Width / 100%");
    m_btnZoomIn = makeFlatButton("+", ID_BTN_ZOOM_IN, "Zoom In (Ctrl++)");
    m_btnFit = makeFlatButton("[ ]", ID_BTN_FIT, "Fit Width / Fit Page");
    m_btnRotate = makeFlatButton("R", ID_BTN_ROTATE, "Rotate Clockwise (Ctrl+R)");

    rightSizer->Add(m_btnZoomOut, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
    rightSizer->Add(m_zoomLabel, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 4);
    rightSizer->Add(m_btnZoomIn, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 4);
    rightSizer->AddSpacer(8);
    rightSizer->Add(m_btnFit, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
    rightSizer->Add(m_btnRotate, 0, wxALIGN_CENTER_VERTICAL);

    // Assemble Bottom Bar: [Left Filename] --- [Center Page Navigation] --- [Right Zoom/Tools]
    barSizer->AddSpacer(12);
    barSizer->Add(m_fileLabel, 1, wxALIGN_CENTER_VERTICAL);
    barSizer->Add(centerSizer, 0, wxALIGN_CENTER);
    barSizer->AddStretchSpacer(1);
    barSizer->Add(rightSizer, 0, wxALIGN_CENTER_VERTICAL);
    barSizer->AddSpacer(12);

    m_bottomBar->SetSizer(barSizer);

    // Bind bottom bar events
    m_btnPrev->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { m_canvas->PreviousPage(); });
    m_btnNext->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { m_canvas->NextPage(); });
    m_btnZoomOut->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { m_canvas->ZoomOut(); });
    m_btnZoomIn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { m_canvas->ZoomIn(); });
    m_btnFit->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        if (m_canvas->GetZoomMode() == ZoomMode::FitWidth) {
            m_canvas->SetZoomMode(ZoomMode::FitPage);
        } else {
            m_canvas->SetZoomMode(ZoomMode::FitWidth);
        }
    });
    m_btnRotate->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { m_canvas->RotateClockwise(); });

    m_pageLabel->Bind(wxEVT_LEFT_UP, &MainFrame::OnPageLabelClicked, this);
    m_zoomLabel->Bind(wxEVT_LEFT_UP, &MainFrame::OnZoomLabelClicked, this);
}

void MainFrame::SetupDropTarget() {
    SetDropTarget(new FileDropTarget(this));
}

bool MainFrame::FileDropTarget::OnDropFiles(wxCoord, wxCoord, const wxArrayString& filenames) {
    if (!filenames.empty()) {
        return m_frame->OpenDocument(filenames[0]);
    }
    return false;
}

bool MainFrame::OpenDocument(const wxString& filePath) {
    if (filePath.empty()) return false;

    wxBeginBusyCursor();
    const bool success = m_engine->LoadFromFile(std::string(filePath.ToUTF8()));
    wxEndBusyCursor();

    if (!success) {
        wxMessageBox(wxString::Format("Could not open file:\n%s\n\nReason: %s",
                                      filePath, m_engine->GetLastError()),
                     "Error", wxOK | wxICON_ERROR, this);
        return false;
    }

    m_currentFilePath = filePath;
    wxFileName fn(filePath);
    SetTitle(wxString::Format("%s — PDF Reader", fn.GetFullName()));

    m_canvas->OnDocumentOpened();
    UpdateControlsState();
    return true;
}

void MainFrame::CloseDocument() {
    m_engine->Close();
    m_currentFilePath.clear();
    SetTitle("PDF Reader");
    m_canvas->OnDocumentClosed();
    UpdateControlsState();
    UpdateBottomBar(0, 0, 1.0, ZoomMode::FitWidth);
}

void MainFrame::OnMenuOpen(wxCommandEvent&) {
    wxFileDialog openFileDialog(this, "Open PDF", "", "",
                                "PDF Files (*.pdf)|*.pdf|All Files (*.*)|*.*",
                                wxFD_OPEN | wxFD_FILE_MUST_EXIST);

    if (openFileDialog.ShowModal() == wxID_CANCEL) {
        return;
    }

    OpenDocument(openFileDialog.GetPath());
}

void MainFrame::OnMenuClose(wxCommandEvent&) {
    CloseDocument();
}

void MainFrame::OnMenuProperties(wxCommandEvent&) {
    if (!m_engine || !m_engine->IsLoaded()) return;

    const DocumentMetadata meta = m_engine->GetMetadata();
    const PageSize pageSize = m_canvas->GetCurrentPageOriginalSize();

    std::ostringstream ss;
    ss << "File: " << m_currentFilePath.ToUTF8() << "\n\n";
    ss << "Title: " << (meta.title.empty() ? "(none)" : meta.title) << "\n";
    ss << "Author: " << (meta.author.empty() ? "(none)" : meta.author) << "\n";
    ss << "Subject: " << (meta.subject.empty() ? "(none)" : meta.subject) << "\n";
    ss << "Creator: " << (meta.creator.empty() ? "(none)" : meta.creator) << "\n";
    ss << "Producer: " << (meta.producer.empty() ? "(none)" : meta.producer) << "\n";
    ss << "Creation Date: " << (meta.creationDate.empty() ? "(none)" : meta.creationDate) << "\n";
    ss << "Page Count: " << meta.pageCount << "\n";
    if (meta.version > 0) {
        ss << "PDF Version: 1." << (meta.version % 10) << "\n";
    }
    if (pageSize.IsValid()) {
        const double wMm = (pageSize.widthPoints / 72.0) * 25.4;
        const double hMm = (pageSize.heightPoints / 72.0) * 25.4;
        ss << "\nCurrent Page Dimensions: "
           << std::fixed << std::setprecision(1)
           << pageSize.widthPoints << " x " << pageSize.heightPoints << " pt ("
           << wMm << " x " << hMm << " mm)\n";
    }

    wxMessageBox(wxString::FromUTF8(ss.str().c_str()), "Document Properties", wxOK | wxICON_INFORMATION, this);
}

void MainFrame::OnMenuExit(wxCommandEvent&) {
    Close(true);
}

void MainFrame::OnMenuZoomIn(wxCommandEvent&) {
    m_canvas->ZoomIn();
}

void MainFrame::OnMenuZoomOut(wxCommandEvent&) {
    m_canvas->ZoomOut();
}

void MainFrame::OnMenuZoom100(wxCommandEvent&) {
    m_canvas->ZoomActualSize();
}

void MainFrame::OnMenuFitWidth(wxCommandEvent&) {
    m_canvas->SetZoomMode(ZoomMode::FitWidth);
}

void MainFrame::OnMenuFitPage(wxCommandEvent&) {
    m_canvas->SetZoomMode(ZoomMode::FitPage);
}

void MainFrame::OnMenuRotateCW(wxCommandEvent&) {
    m_canvas->RotateClockwise();
}

void MainFrame::OnMenuRotateCCW(wxCommandEvent&) {
    m_canvas->RotateCounterClockwise();
}

void MainFrame::OnMenuFirstPage(wxCommandEvent&) {
    m_canvas->FirstPage();
}

void MainFrame::OnMenuPrevPage(wxCommandEvent&) {
    m_canvas->PreviousPage();
}

void MainFrame::OnMenuNextPage(wxCommandEvent&) {
    m_canvas->NextPage();
}

void MainFrame::OnMenuLastPage(wxCommandEvent&) {
    m_canvas->LastPage();
}

void MainFrame::OnMenuGoToPage(wxCommandEvent&) {
    if (!m_engine || !m_engine->IsLoaded()) return;

    const int total = m_engine->GetPageCount();
    const int current = m_canvas->GetCurrentPage() + 1;

    const long target = wxGetNumberFromUser("Enter page number:",
                                            "Page:", "Go to Page",
                                            current, 1, total, this);
    if (target > 0) {
        m_canvas->SetPage(static_cast<int>(target - 1));
    }
}

void MainFrame::OnPageLabelClicked(wxMouseEvent&) {
    wxCommandEvent evt;
    OnMenuGoToPage(evt);
}

void MainFrame::OnZoomLabelClicked(wxMouseEvent&) {
    if (m_canvas->GetZoomMode() == ZoomMode::FitWidth) {
        m_canvas->ZoomActualSize();
    } else {
        m_canvas->SetZoomMode(ZoomMode::FitWidth);
    }
}

void MainFrame::OnMenuToggleRemoveWatermark(wxCommandEvent& event) {
    if (!m_engine || !m_canvas) return;
    const bool enable = event.IsChecked();
    m_canvas->SetWatermarkFilter(enable);
    if (m_menuItemRemoveWatermark) {
        m_menuItemRemoveWatermark->Check(enable);
    }
    if (m_fileLabel && m_engine->IsLoaded()) {
        wxFileName fn(m_currentFilePath);
        m_fileLabel->SetLabel(fn.GetFullName() + (enable ? " [Watermark Suppressed]" : ""));
    }
}

void MainFrame::OnMenuPurgeWatermarks(wxCommandEvent&) {
    if (!m_engine || !m_engine->IsLoaded() || !m_canvas) return;

    wxBusyCursor busy;
    const auto report = m_engine->PurgeWatermarks();

    if (report.success) {
        m_canvas->ClearCacheAndRefresh();
        const wxString msg = wxString::Format(
            "Watermark Purge Completed:\n\n"
            "- Removed %d watermark/stamp annotation(s)\n"
            "- Removed %d marked content / translucent object(s)\n"
            "- Processed %d page(s)\n\n"
            "Use 'Tools -> Save Cleaned PDF As...' to export the clean document.",
            report.annotationsRemoved,
            report.objectsRemoved,
            report.pagesProcessed
        );
        wxMessageBox(msg, "Watermarks Purged", wxOK | wxICON_INFORMATION, this);
    } else {
        wxMessageBox("Failed to purge watermarks: " + wxString::FromUTF8(m_engine->GetLastError().c_str()),
                     "Error", wxOK | wxICON_ERROR, this);
    }
}

void MainFrame::OnMenuSaveCleanedPdf(wxCommandEvent&) {
    if (!m_engine || !m_engine->IsLoaded()) return;

    wxFileName currentFn(m_currentFilePath);
    wxString defaultCleanName = currentFn.GetName() + "_clean.pdf";

    wxFileDialog saveFileDialog(this, "Save Cleaned PDF As", currentFn.GetPath(), defaultCleanName,
                                "PDF Files (*.pdf)|*.pdf",
                                wxFD_SAVE | wxFD_OVERWRITE_PROMPT);

    if (saveFileDialog.ShowModal() == wxID_CANCEL) {
        return;
    }

    const wxString savePath = saveFileDialog.GetPath();
    wxBusyCursor busy;
    if (m_engine->SaveDocumentAs(savePath.ToStdString())) {
        wxMessageBox("Cleaned PDF saved successfully to:\n" + savePath,
                     "Save Complete", wxOK | wxICON_INFORMATION, this);
    } else {
        wxMessageBox("Failed to save cleaned PDF:\n" + wxString::FromUTF8(m_engine->GetLastError().c_str()),
                     "Error Saving File", wxOK | wxICON_ERROR, this);
    }
}

void MainFrame::OnMenuAbout(wxCommandEvent&) {
    wxMessageBox("Minimal PDF Reader\n\n"
                 "Powered by Google PDFium and wxWidgets.\n"
                 "Continuous page scrolling with subpixel rendering and minimal memory footprint.\n\n"
                 "Features built-in Watermark Suppression and DOM Object Purging tools.",
                 "About", wxOK | wxICON_INFORMATION, this);
}

void MainFrame::UpdateControlsState() {
    const bool hasDoc = (m_engine && m_engine->IsLoaded());
    const int cur = m_canvas ? m_canvas->GetCurrentPage() : 0;
    const int total = m_canvas ? m_canvas->GetTotalPages() : 0;

    auto* mb = GetMenuBar();
    if (mb) {
        mb->Enable(wxID_CLOSE, hasDoc);
        mb->Enable(ID_PROPERTIES, hasDoc);
        mb->Enable(ID_ZOOM_IN, hasDoc);
        mb->Enable(ID_ZOOM_OUT, hasDoc);
        mb->Enable(ID_ZOOM_100, hasDoc);
        mb->Enable(ID_FIT_WIDTH, hasDoc);
        mb->Enable(ID_FIT_PAGE, hasDoc);
        mb->Enable(ID_ROTATE_CW, hasDoc);
        mb->Enable(ID_ROTATE_CCW, hasDoc);
        mb->Enable(ID_FIRST_PAGE, hasDoc && cur > 0);
        mb->Enable(ID_PREV_PAGE, hasDoc && cur > 0);
        mb->Enable(ID_NEXT_PAGE, hasDoc && cur < total - 1);
        mb->Enable(ID_LAST_PAGE, hasDoc && cur < total - 1);
        mb->Enable(ID_GOTO_PAGE, hasDoc && total > 1);
        mb->Enable(ID_TOOL_REMOVE_WATERMARK, hasDoc);
        mb->Enable(ID_TOOL_PURGE_WATERMARKS, hasDoc);
        mb->Enable(ID_TOOL_SAVE_CLEANED, hasDoc);
    }

    if (m_menuItemRemoveWatermark) {
        m_menuItemRemoveWatermark->Check(m_engine ? m_engine->IsWatermarkFilterEnabled() : false);
    }

    if (m_btnPrev) m_btnPrev->Enable(hasDoc && cur > 0);
    if (m_btnNext) m_btnNext->Enable(hasDoc && cur < total - 1);
    if (m_btnZoomIn) m_btnZoomIn->Enable(hasDoc);
    if (m_btnZoomOut) m_btnZoomOut->Enable(hasDoc);
    if (m_btnFit) m_btnFit->Enable(hasDoc);
    if (m_btnRotate) m_btnRotate->Enable(hasDoc);
}

void MainFrame::UpdateBottomBar(int currentPage, int totalPages, double zoomFactor, ZoomMode mode) {
    if (totalPages > 0) {
        wxFileName fn(m_currentFilePath);
        if (m_fileLabel) m_fileLabel->SetLabel(fn.GetFullName());
        if (m_pageLabel) m_pageLabel->SetLabel(wxString::Format("%d / %d", currentPage, totalPages));
    } else {
        if (m_fileLabel) m_fileLabel->SetLabel("Ready");
        if (m_pageLabel) m_pageLabel->SetLabel("- / -");
    }

    if (m_zoomLabel) {
        if (mode == ZoomMode::FitWidth) {
            m_zoomLabel->SetLabel(wxString::Format("Fit (%d%%)", static_cast<int>(std::round(zoomFactor * 100.0))));
        } else if (mode == ZoomMode::FitPage) {
            m_zoomLabel->SetLabel(wxString::Format("Page (%d%%)", static_cast<int>(std::round(zoomFactor * 100.0))));
        } else {
            m_zoomLabel->SetLabel(wxString::Format("%d%%", static_cast<int>(std::round(zoomFactor * 100.0))));
        }
    }

    if (m_bottomBar) {
        m_bottomBar->Layout();
    }
}

} // namespace pdf
