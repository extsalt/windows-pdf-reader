#include <wx/wx.h>
#include <wx/image.h>
#include <wx/cmdline.h>
#include <pdfium/fpdfview.h>
#include "MainFrame.h"

class PdfReaderApp : public wxApp {
public:
    void OnInitCmdLine(wxCmdLineParser& parser) override {
        parser.AddSwitch("", "register", "Register Windows Explorer context menu");
        parser.AddSwitch("", "register-silent", "Register Windows Explorer context menu silently");
        parser.AddSwitch("", "unregister", "Unregister Windows Explorer context menu");
        parser.AddSwitch("", "unregister-silent", "Unregister Windows Explorer context menu silently");
        parser.AddParam("file", wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL | wxCMD_LINE_PARAM_MULTIPLE);
        wxApp::OnInitCmdLine(parser);
    }

    bool OnInit() override {
        if (!wxApp::OnInit()) {
            return false;
        }

        // Initialize standard image handlers
        wxInitAllImageHandlers();

        // Handle command-line registration flags before GUI creation
        for (int i = 1; i < argc; ++i) {
            const wxString arg = argv[i];
            if (arg.IsSameAs("--register", false) || arg.IsSameAs("/register", false)) {
                wxString err;
                if (pdf::MainFrame::RegisterShellContextMenu(&err)) {
                    wxMessageBox("Windows Explorer context menu registered successfully!\n\n"
                                 "You can now right-click any file in Windows Explorer and choose\n"
                                 "'Open with Windows PDF Reader'.",
                                 "Windows PDF Reader", wxOK | wxICON_INFORMATION);
                } else {
                    wxMessageBox("Failed to register context menu:\n" + err, "Registration Error", wxOK | wxICON_ERROR);
                }
                return false;
            }
            if (arg.IsSameAs("--register-silent", false) || arg.IsSameAs("/register-silent", false)) {
                pdf::MainFrame::RegisterShellContextMenu();
                return false;
            }
            if (arg.IsSameAs("--unregister", false) || arg.IsSameAs("/unregister", false)) {
                wxString err;
                if (pdf::MainFrame::UnregisterShellContextMenu(&err)) {
                    wxMessageBox("Windows Explorer context menu removed successfully.",
                                 "Windows PDF Reader", wxOK | wxICON_INFORMATION);
                } else {
                    wxMessageBox("Failed to unregister context menu:\n" + err, "Unregistration Error", wxOK | wxICON_ERROR);
                }
                return false;
            }
            if (arg.IsSameAs("--unregister-silent", false) || arg.IsSameAs("/unregister-silent", false)) {
                pdf::MainFrame::UnregisterShellContextMenu();
                return false;
            }
        }

        // Initialize Google PDFium library
        FPDF_LIBRARY_CONFIG config{};
        config.version = 2;
        config.m_pUserFontPaths = nullptr;
        config.m_pIsolate = nullptr;
        config.m_v8EmbedderSlot = 0;
        FPDF_InitLibraryWithConfig(&config);
        m_pdfiumInitialized = true;

        // Create main application frame
        auto* frame = new pdf::MainFrame("Minimal PDF Reader (PDFium)",
                                         wxDefaultPosition,
                                         wxSize(1024, 768));
        frame->Center();
        frame->Show(true);

        // If a file was passed as a command-line argument, open it immediately
        for (int i = 1; i < argc; ++i) {
            const wxString arg = argv[i];
            if (!arg.StartsWith("-") && !arg.StartsWith("/")) {
                frame->OpenDocument(arg);
                break;
            }
        }

        return true;
    }

    int OnExit() override {
        // Cleanly shut down PDFium library if initialized
        if (m_pdfiumInitialized) {
            FPDF_DestroyLibrary();
            m_pdfiumInitialized = false;
        }
        return wxApp::OnExit();
    }

private:
    bool m_pdfiumInitialized{false};
};

wxIMPLEMENT_APP(PdfReaderApp);
