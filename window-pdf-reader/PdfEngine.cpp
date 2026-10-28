#include "PdfEngine.h"
#include <wx/string.h>
#include <cstdlib>
#include <cstring>

namespace pdf {

namespace {

std::string TranslatePdfError(unsigned long errorCode) {
    switch (errorCode) {
        case FPDF_ERR_SUCCESS:
            return "Success";
        case FPDF_ERR_UNKNOWN:
            return "Unknown error";
        case FPDF_ERR_FILE:
            return "File not found or could not be opened";
        case FPDF_ERR_FORMAT:
            return "File is not a valid PDF or is corrupted";
        case FPDF_ERR_PASSWORD:
            return "Password required or incorrect password";
        case FPDF_ERR_SECURITY:
            return "Unsupported security or encryption handler";
        case FPDF_ERR_PAGE:
            return "Page not found or content error";
        default:
            return "PDF processing error (" + std::to_string(errorCode) + ")";
    }
}

} // namespace

bool PdfEngine::LoadFromFile(const std::string& filePath, const std::string& password) {
    Close();

    const char* pwd = password.empty() ? nullptr : password.c_str();
    FPDF_DOCUMENT rawDoc = FPDF_LoadDocument(filePath.c_str(), pwd);

    if (!rawDoc) {
        const unsigned long err = FPDF_GetLastError();
        m_lastError = TranslatePdfError(err);
        return false;
    }

    m_document.reset(rawDoc);
    m_pageCount = FPDF_GetPageCount(m_document.get());
    m_currentPath = filePath;
    m_lastError.clear();

    m_pageSizes.clear();
    m_pageSizes.reserve(m_pageCount);
    for (int i = 0; i < m_pageCount; ++i) {
        FS_SIZEF sizeF{};
        if (FPDF_GetPageSizeByIndexF(m_document.get(), i, &sizeF)) {
            m_pageSizes.push_back({ sizeF.width, sizeF.height });
        } else {
            ScopedPage page(FPDF_LoadPage(m_document.get(), i));
            if (page) {
                m_pageSizes.push_back({ FPDF_GetPageWidthF(page.get()), FPDF_GetPageHeightF(page.get()) });
            } else {
                m_pageSizes.push_back({});
            }
        }
    }

    return true;
}

void PdfEngine::Close() {
    m_document.reset();
    m_pageCount = 0;
    m_pageSizes.clear();
    m_currentPath.clear();
    m_lastError.clear();
}

PageSize PdfEngine::GetPageSize(int pageIndex) const {
    if (pageIndex >= 0 && pageIndex < static_cast<int>(m_pageSizes.size())) {
        return m_pageSizes[pageIndex];
    }
    return {};
}

wxImage PdfEngine::RenderPageToImage(int pageIndex, int targetWidth, int targetHeight, int rotation) const {
    if (!m_document || pageIndex < 0 || pageIndex >= m_pageCount || targetWidth <= 0 || targetHeight <= 0) {
        return {};
    }

    ScopedPage page(FPDF_LoadPage(m_document.get(), pageIndex));
    if (!page) {
        m_lastError = "Failed to load page " + std::to_string(pageIndex + 1);
        return {};
    }

    // Create 32-bit BGRA bitmap buffer in PDFium
    ScopedBitmap bitmap(FPDFBitmap_CreateEx(targetWidth, targetHeight, FPDFBitmap_BGRA, nullptr, 0));
    if (!bitmap) {
        m_lastError = "Failed to allocate PDFium render buffer";
        return {};
    }

    // Fill with solid white (0xFFFFFFFF in BGRA is opaque white)
    FPDFBitmap_FillRect(bitmap.get(), 0, 0, targetWidth, targetHeight, 0xFFFFFFFF);

    // Render with anti-aliasing and subpixel LCD text for crisp display
    constexpr int renderFlags = FPDF_ANNOT | FPDF_LCD_TEXT;
    FPDF_RenderPageBitmap(bitmap.get(), page.get(), 0, 0, targetWidth, targetHeight, rotation, renderFlags);

    const auto* srcBuffer = static_cast<const uint8_t*>(FPDFBitmap_GetBuffer(bitmap.get()));
    const int stride = FPDFBitmap_GetStride(bitmap.get());

    if (!srcBuffer || stride <= 0) {
        m_lastError = "Invalid bitmap buffer";
        return {};
    }

    // Allocate wxImage 24-bit RGB buffer directly (wxImage takes ownership, 0 extra copies)
    const size_t rgbBufferSize = static_cast<size_t>(targetWidth) * targetHeight * 3;
    auto* rgbData = static_cast<unsigned char*>(std::malloc(rgbBufferSize));
    if (!rgbData) {
        m_lastError = "Out of memory allocating image buffer";
        return {};
    }

    // Fast scanline conversion from BGRA (PDFium) to RGB (wxWidgets)
    for (int y = 0; y < targetHeight; ++y) {
        const uint8_t* rowSrc = srcBuffer + (y * stride);
        unsigned char* rowDst = rgbData + (y * targetWidth * 3);

        for (int x = 0; x < targetWidth; ++x) {
            const int srcIdx = x * 4;
            const int dstIdx = x * 3;
            rowDst[dstIdx + 0] = rowSrc[srcIdx + 2]; // Red
            rowDst[dstIdx + 1] = rowSrc[srcIdx + 1]; // Green
            rowDst[dstIdx + 2] = rowSrc[srcIdx + 0]; // Blue
        }
    }

    // Construct wxImage taking direct ownership of rgbData without copying
    return wxImage(targetWidth, targetHeight, rgbData, false);
}

std::string PdfEngine::ExtractMetaText(const char* tag) const {
    if (!m_document) return "";

    const unsigned long length = FPDF_GetMetaText(m_document.get(), tag, nullptr, 0);
    if (length <= 2) return ""; // 2 bytes for null-terminator in UTF-16LE

    std::vector<unsigned char> buffer(length);
    FPDF_GetMetaText(m_document.get(), tag, buffer.data(), length);

    // Convert UTF-16LE to wxString and then UTF-8 std::string
    const auto* utf16Ptr = reinterpret_cast<const wchar_t*>(buffer.data());
    const size_t wcharCount = (length / 2) > 0 ? (length / 2) - 1 : 0;
    const wxString wxStr(utf16Ptr, wcharCount);
    return std::string(wxStr.ToUTF8());
}

DocumentMetadata PdfEngine::GetMetadata() const {
    DocumentMetadata meta{};
    if (!m_document) return meta;

    meta.title = ExtractMetaText("Title");
    meta.author = ExtractMetaText("Author");
    meta.subject = ExtractMetaText("Subject");
    meta.keywords = ExtractMetaText("Keywords");
    meta.creator = ExtractMetaText("Creator");
    meta.producer = ExtractMetaText("Producer");
    meta.creationDate = ExtractMetaText("CreationDate");
    meta.modDate = ExtractMetaText("ModDate");
    meta.pageCount = m_pageCount;

    int fileVersion = 0;
    if (FPDF_GetFileVersion(m_document.get(), &fileVersion)) {
        meta.version = fileVersion;
    }

    return meta;
}

} // namespace pdf
