#include "PdfEngine.h"
#include <wx/string.h>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <vector>
#include <pdfium/fpdf_edit.h>
#include <pdfium/fpdf_annot.h>
#include <pdfium/fpdf_save.h>

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
    int renderFlags = FPDF_LCD_TEXT;
    if (!m_watermarkFilterEnabled) {
        renderFlags |= FPDF_ANNOT;
    }
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
            uint8_t b = rowSrc[srcIdx + 0];
            uint8_t g = rowSrc[srcIdx + 1];
            uint8_t r = rowSrc[srcIdx + 2];

            if (m_watermarkFilterEnabled) {
                // Dark-hat watermark eradication:
                // Faint, translucent grey/colored watermark text or background stamps
                const uint32_t minChan = std::min({ r, g, b });
                const uint32_t maxChan = std::max({ r, g, b });
                const float lum = 0.299f * r + 0.587f * g + 0.114f * b;

                bool isWatermarkPixel = false;
                if (lum >= 235.0f) {
                    isWatermarkPixel = true;
                } else if (minChan >= 155 && lum >= 170.0f) {
                    isWatermarkPixel = true;
                } else if ((maxChan - minChan) <= 25 && lum >= 160.0f) {
                    isWatermarkPixel = true;
                }

                if (isWatermarkPixel) {
                    r = 255;
                    g = 255;
                    b = 255;
                } else if (lum < 140.0f) {
                    // Deepen genuine dark text to maximize reading contrast
                    r = static_cast<uint8_t>(r * 0.95f);
                    g = static_cast<uint8_t>(g * 0.95f);
                    b = static_cast<uint8_t>(b * 0.95f);
                }
            }

            rowDst[dstIdx + 0] = r; // Red
            rowDst[dstIdx + 1] = g; // Green
            rowDst[dstIdx + 2] = b; // Blue
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

PdfEngine::WatermarkPurgeReport PdfEngine::PurgeWatermarks() {
    WatermarkPurgeReport report{};
    if (!m_document) {
        m_lastError = "No document loaded";
        return report;
    }

    for (int pageIdx = 0; pageIdx < m_pageCount; ++pageIdx) {
        ScopedPage page(FPDF_LoadPage(m_document.get(), pageIdx));
        if (!page) continue;

        bool pageModified = false;

        // 1. Purge watermark and stamp annotations
        const int annotCount = FPDFPage_GetAnnotCount(page.get());
        for (int a = annotCount - 1; a >= 0; --a) {
            FPDF_ANNOTATION annot = FPDFPage_GetAnnot(page.get(), a);
            if (!annot) continue;

            const FPDF_ANNOTATION_SUBTYPE subtype = FPDFAnnot_GetSubtype(annot);
            const bool isWm = (subtype == FPDF_ANNOT_WATERMARK || subtype == FPDF_ANNOT_STAMP);
            FPDFPage_CloseAnnot(annot);

            if (isWm) {
                if (FPDFPage_RemoveAnnot(page.get(), a)) {
                    report.annotationsRemoved++;
                    pageModified = true;
                }
            }
        }

        // 2. Purge marked content (/Watermark, /Artifact) and translucent overlay objects
        const int objCount = FPDFPage_CountObjects(page.get());
        for (int o = objCount - 1; o >= 0; --o) {
            FPDF_PAGEOBJECT obj = FPDFPage_GetObject(page.get(), o);
            if (!obj) continue;

            bool isWatermark = false;

            // Check marked content tag
            const int markCount = FPDFPageObj_CountMarks(obj);
            for (int m = 0; m < markCount; ++m) {
                FPDF_PAGEOBJECTMARK mark = FPDFPageObj_GetMark(obj, m);
                if (!mark) continue;

                unsigned long bufLen = 0;
                if (FPDFPageObjMark_GetName(mark, nullptr, 0, &bufLen) && bufLen > 0) {
                    std::vector<FPDF_WCHAR> nameBuf((bufLen / sizeof(FPDF_WCHAR)) + 2, 0);
                    if (FPDFPageObjMark_GetName(mark, nameBuf.data(), bufLen, &bufLen)) {
                        std::wstring markName(reinterpret_cast<const wchar_t*>(nameBuf.data()));
                        std::string lower;
                        for (wchar_t wc : markName) {
                            if (wc == 0) break;
                            lower += static_cast<char>(std::tolower(static_cast<unsigned char>(wc)));
                        }
                        if (lower.find("watermark") != std::string::npos ||
                            lower.find("artifact") != std::string::npos) {
                            isWatermark = true;
                            break;
                        }
                    }
                }
            }

            // Check transparency/alpha on fill or stroke
            if (!isWatermark) {
                unsigned int r = 0, g = 0, b = 0, a = 255;
                if (FPDFPageObj_GetFillColor(obj, &r, &g, &b, &a)) {
                    if (a > 0 && a < 200) {
                        isWatermark = true;
                    }
                }
                if (!isWatermark && FPDFPageObj_GetStrokeColor(obj, &r, &g, &b, &a)) {
                    if (a > 0 && a < 200) {
                        isWatermark = true;
                    }
                }
            }

            if (isWatermark) {
                if (FPDFPage_RemoveObject(page.get(), obj)) {
                    FPDFPageObj_Destroy(obj);
                    report.objectsRemoved++;
                    pageModified = true;
                }
            }
        }

        if (pageModified) {
            FPDFPage_GenerateContent(page.get());
        }
        report.pagesProcessed++;
    }

    report.success = true;
    return report;
}

bool PdfEngine::SaveDocumentAs(const std::string& outputPath) const {
    if (!m_document) {
        m_lastError = "No document loaded";
        return false;
    }

    struct FileWriterContext : public FPDF_FILEWRITE {
        std::ofstream file;
        explicit FileWriterContext(const std::string& path) {
            version = 1;
            WriteBlock = &WriteBlockCallback;
            file.open(path, std::ios::binary | std::ios::trunc);
        }
        static int WriteBlockCallback(FPDF_FILEWRITE* self, const void* data, unsigned long size) {
            auto* ctx = static_cast<FileWriterContext*>(self);
            if (ctx->file.is_open()) {
                ctx->file.write(static_cast<const char*>(data), size);
                return ctx->file.good() ? 1 : 0;
            }
            return 0;
        }
    };

    FileWriterContext writer(outputPath);
    if (!writer.file.is_open()) {
        m_lastError = "Could not open output file for writing";
        return false;
    }

    if (!FPDF_SaveAsCopy(m_document.get(), &writer, FPDF_NO_INCREMENTAL)) {
        m_lastError = "Failed to serialize and save PDF";
        return false;
    }

    return true;
}

std::vector<SearchMatch> PdfEngine::SearchText(const std::wstring& query, bool matchCase) const {
    std::vector<SearchMatch> results;
    if (!m_document || query.empty() || m_pageCount <= 0) {
        return results;
    }

    const unsigned long flags = matchCase ? FPDF_MATCHCASE : 0;
    const auto* findWhat = reinterpret_cast<FPDF_WIDESTRING>(query.c_str());

    for (int pageIdx = 0; pageIdx < m_pageCount; ++pageIdx) {
        ScopedPage page(FPDF_LoadPage(m_document.get(), pageIdx));
        if (!page) continue;

        ScopedTextPage textPage(FPDFText_LoadPage(page.get()));
        if (!textPage) continue;

        ScopedSearchContext sch(FPDFText_FindStart(textPage.get(), findWhat, flags, 0));
        if (sch) {
            while (FPDFText_FindNext(sch.get())) {
                const int charIdx = FPDFText_GetSchResultIndex(sch.get());
                const int charCnt = FPDFText_GetSchCount(sch.get());
                if (charCnt <= 0) continue;

                SearchMatch match;
                match.pageIndex = pageIdx;
                match.charIndex = charIdx;
                match.charCount = charCnt;

                const int rectCount = FPDFText_CountRects(textPage.get(), charIdx, charCnt);
                if (rectCount > 0) {
                    match.rectsPoints.reserve(rectCount);
                    for (int r = 0; r < rectCount; ++r) {
                        double left = 0, top = 0, right = 0, bottom = 0;
                        if (FPDFText_GetRect(textPage.get(), r, &left, &top, &right, &bottom)) {
                            match.rectsPoints.push_back({ left, top, right, bottom });
                        }
                    }
                } else {
                    for (int c = 0; c < charCnt; ++c) {
                        double left = 0, right = 0, bottom = 0, top = 0;
                        if (FPDFText_GetCharBox(textPage.get(), charIdx + c, &left, &right, &bottom, &top)) {
                            match.rectsPoints.push_back({ left, top, right, bottom });
                        }
                    }
                }

                if (!match.rectsPoints.empty()) {
                    results.push_back(std::move(match));
                }
            }
        }
    }

    return results;
}

void PdfEngine::ConvertPointsToDevice(int pageIndex, int startX, int startY, int sizeX, int sizeY, int rotation,
                                      const std::vector<SearchRect>& inPoints, std::vector<wxRect>& outRects) const {
    if (!m_document || pageIndex < 0 || pageIndex >= m_pageCount || inPoints.empty()) {
        return;
    }

    ScopedPage page(FPDF_LoadPage(m_document.get(), pageIndex));
    if (!page) return;

    outRects.reserve(outRects.size() + inPoints.size());
    for (const auto& pt : inPoints) {
        int devX1 = 0, devY1 = 0;
        int devX2 = 0, devY2 = 0;
        if (FPDF_PageToDevice(page.get(), startX, startY, sizeX, sizeY, rotation, pt.left, pt.top, &devX1, &devY1) &&
            FPDF_PageToDevice(page.get(), startX, startY, sizeX, sizeY, rotation, pt.right, pt.bottom, &devX2, &devY2)) {
            const int rx = std::min(devX1, devX2);
            const int ry = std::min(devY1, devY2);
            const int rw = std::max(2, std::abs(devX2 - devX1));
            const int rh = std::max(2, std::abs(devY2 - devY1));
            outRects.emplace_back(rx, ry, rw, rh);
        }
    }
}

} // namespace pdf
