#pragma once

#include <string>
#include <memory>
#include <vector>
#include <windows.h>
#include <wx/image.h>
#include <wx/bitmap.h>
#include <pdfium/fpdfview.h>
#include <pdfium/fpdf_doc.h>

namespace pdf {

// RAII Deleters for PDFium C handles
struct DocumentDeleter {
    void operator()(FPDF_DOCUMENT doc) const noexcept {
        if (doc) {
            FPDF_CloseDocument(doc);
        }
    }
};

struct PageDeleter {
    void operator()(FPDF_PAGE page) const noexcept {
        if (page) {
            FPDF_ClosePage(page);
        }
    }
};

struct BitmapDeleter {
    void operator()(FPDF_BITMAP bmp) const noexcept {
        if (bmp) {
            FPDFBitmap_Destroy(bmp);
        }
    }
};

using ScopedDocument = std::unique_ptr<std::remove_pointer_t<FPDF_DOCUMENT>, DocumentDeleter>;
using ScopedPage = std::unique_ptr<std::remove_pointer_t<FPDF_PAGE>, PageDeleter>;
using ScopedBitmap = std::unique_ptr<std::remove_pointer_t<FPDF_BITMAP>, BitmapDeleter>;

struct PageSize {
    double widthPoints{0.0};
    double heightPoints{0.0};

    [[nodiscard]] bool IsValid() const noexcept {
        return widthPoints > 0.0 && heightPoints > 0.0;
    }
};

struct DocumentMetadata {
    std::string title;
    std::string author;
    std::string subject;
    std::string keywords;
    std::string creator;
    std::string producer;
    std::string creationDate;
    std::string modDate;
    int pageCount{0};
    int version{0};
};

class PdfEngine {
public:
    PdfEngine() = default;
    ~PdfEngine() = default;

    PdfEngine(const PdfEngine&) = delete;
    PdfEngine& operator=(const PdfEngine&) = delete;
    PdfEngine(PdfEngine&&) noexcept = default;
    PdfEngine& operator=(PdfEngine&&) noexcept = default;

    // Load PDF from a local filesystem path (UTF-8 encoded)
    bool LoadFromFile(const std::string& filePath, const std::string& password = "");

    // Close the currently opened document
    void Close();

    // Check if a valid document is loaded
    [[nodiscard]] bool IsLoaded() const noexcept { return m_document != nullptr; }

    // Page query functions
    [[nodiscard]] int GetPageCount() const noexcept { return m_pageCount; }
    [[nodiscard]] PageSize GetPageSize(int pageIndex) const;

    // Render a page at given pixel dimensions and rotation (0: normal, 1: 90 deg, 2: 180 deg, 3: 270 deg)
    [[nodiscard]] wxImage RenderPageToImage(int pageIndex, int targetWidth, int targetHeight, int rotation = 0) const;

    // Document metadata
    [[nodiscard]] DocumentMetadata GetMetadata() const;

    // Last error message
    [[nodiscard]] const std::string& GetLastError() const noexcept { return m_lastError; }

private:
    std::string ExtractMetaText(const char* tag) const;

    ScopedDocument m_document{nullptr};
    int m_pageCount{0};
    std::vector<PageSize> m_pageSizes;
    mutable std::string m_lastError;
    std::string m_currentPath;
};

} // namespace pdf
