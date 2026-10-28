#include <wx/wx.h>
#include <wx/image.h>
#include <pdfium/fpdfview.h>
#include "MainFrame.h"

class PdfReaderApp : public wxApp {
public:
    bool OnInit() override {
        if (!wxApp::OnInit()) {
            return false;
        }

        // Initialize standard image handlers
        wxInitAllImageHandlers();

        // Initialize Google PDFium library
        FPDF_LIBRARY_CONFIG config{};
        config.version = 2;
        config.m_pUserFontPaths = nullptr;
        config.m_pIsolate = nullptr;
        config.m_v8EmbedderSlot = 0;
        FPDF_InitLibraryWithConfig(&config);

        // Create main application frame
        auto* frame = new pdf::MainFrame("Minimal PDF Reader (PDFium)",
                                         wxDefaultPosition,
                                         wxSize(1024, 768));
        frame->Center();
        frame->Show(true);

        // If a file was passed as a command-line argument, open it immediately
        if (argc > 1) {
            const wxString filePath = argv[1];
            frame->OpenDocument(filePath);
        }

        return true;
    }

    int OnExit() override {
        // Cleanly shut down PDFium library and free all allocated internal engine resources
        FPDF_DestroyLibrary();
        return wxApp::OnExit();
    }
};

wxIMPLEMENT_APP(PdfReaderApp);
