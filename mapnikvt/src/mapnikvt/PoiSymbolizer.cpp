#include "PoiSymbolizer.h"
#include "ParserUtils.h"
#include "FontSet.h"
#include "Expression.h"
#include "StringUtils.h"
#include "vt/FontManager.h"

#include <vector>
#include <tuple>
#include <algorithm>
#include <cmath>

#include <boost/algorithm/string.hpp>
#include <boost/math/constants/constants.hpp>
#include <utf8.h>

namespace carto::mvt {
    PoiSymbolizer::FeatureProcessor PoiSymbolizer::createFeatureProcessor(const ExpressionContext& exprContext, const SymbolizerContext& symbolizerContext) const {
        float fontScale = symbolizerContext.getSettings().getFontScale();

        // ---- Text properties ----
        std::string text = getTransformedText(exprContext);
        float textSizeStatic = _size.getStaticValue(exprContext);
        vt::LabelOrientation placement = vt::LabelOrientation::BILLBOARD_2D;
        {
            if (_placement.isDefined()) {
                placement = _placement.getValue(exprContext);
                if (placement == vt::LabelOrientation::LINE) {
                    placement = vt::LabelOrientation::BILLBOARD_2D;
                }
            }
        }
        bool allowOverlap = _allowOverlap.getValue(exprContext);
        bool allowOverlapSameFeatureId = _allowOverlapSameFeatureId.getValue(exprContext);
        bool clip = _clip.isDefined() ? _clip.getValue(exprContext) : allowOverlap;
        bool canHideText = _canHideText.getValue(exprContext);
        float minimumDistance = _minimumDistance.getValue(exprContext);
        float placementPriority = _placementPriority.getValue(exprContext);
        float textMargin = _textMargin.getValue(exprContext);

        vt::ColorFunction textFillFunc  = _fillFuncBuilder.createColorOpacityFunction(_fill.getFunction(exprContext), _opacity.getFunction(exprContext));
        vt::FloatFunction textSizeFunc  = _sizeFuncBuilder.createScaledFloatFunction(_size.getFunction(exprContext), fontScale);
        vt::ColorFunction textHaloFill  = _haloFillFuncBuilder.createColorOpacityFunction(_haloFill.getFunction(exprContext), _haloOpacity.getFunction(exprContext));
        vt::FloatFunction textHaloRadius= _haloRadiusFuncBuilder.createScaledFloatFunction(_haloRadius.getFunction(exprContext), fontScale);

        long long groupId = allowOverlap ? -1 : 0;
        if (!allowOverlap && minimumDistance > 0) {
            groupId = 1;
        }

        // ---- Icon properties ----
        std::string iconFile = _iconFile.getValue(exprContext);
        std::string iconName = _iconName.getValue(exprContext);
        float iconSizeStatic = _iconSize.getStaticValue(exprContext);
        vt::ColorFunction iconFillFunc  = _iconFillFuncBuilder.createColorOpacityFunction(_iconFill.getFunction(exprContext), _iconOpacity.getFunction(exprContext));
        vt::FloatFunction iconSizeFunc  = _iconSizeFuncBuilder.createScaledFloatFunction(_iconSize.getFunction(exprContext), fontScale);
        vt::ColorFunction iconHaloFill  = _iconHaloFillFuncBuilder.createColorOpacityFunction(_iconHaloFill.getFunction(exprContext), _iconHaloOpacity.getFunction(exprContext));
        vt::FloatFunction iconHaloRadius= _iconHaloRadiusFuncBuilder.createScaledFloatFunction(_iconHaloRadius.getFunction(exprContext), fontScale);

        // ---- Anchor list ----
        std::string anchorStr = _variableAnchor.getValue(exprContext);
        std::vector<std::string> anchors = parseAnchorList(anchorStr);
        if (anchors.empty()) anchors = { "bottom" };
        int N = static_cast<int>(anchors.size());

        // ---- Feature ID override ----
        std::optional<long long> labelIdOverride;
        if (_featureId.isDefined()) {
            labelIdOverride = convertId(_featureId.getValue(exprContext));
        }
        std::size_t textHash = std::hash<std::string>()(text);

        // ---- Base formatter options for text ----
        float horizontalAlignment = _horizontalAlignment.getValue(exprContext).value_or(0.0f);
        float verticalAlignment   = _verticalAlignment.getValue(exprContext).value_or(0.0f);
        float wrapWidth   = _wrapWidth.getValue(exprContext);
        bool  wrapBefore  = _wrapBefore.getValue(exprContext);
        std::string wrapChar = _wrapCharacter.getValue(exprContext);
        float charSpacing = _characterSpacing.getValue(exprContext);
        float lineSpacing = _lineSpacing.getValue(exprContext);
        vt::TextFormatter::Options baseOptions(
            cglib::vec2<float>(horizontalAlignment, verticalAlignment),
            cglib::vec2<float>(0, 0),
            wrapChar, wrapBefore, wrapWidth * fontScale, charSpacing, lineSpacing);

        // =====================================================================
        // MODE 1: poi-icon-file (bitmap icon)
        // =====================================================================
        if (!iconFile.empty()) {
            std::shared_ptr<const vt::Font> textFont = getTextFont(symbolizerContext, exprContext);
            if (!textFont && !text.empty()) {
                _logger->write(Logger::Severity::ERROR, "PoiSymbolizer: failed to load text font");
                return FeatureProcessor();
            }

            std::shared_ptr<const vt::BitmapImage> backgroundImage = symbolizerContext.getBitmapManager()->loadBitmapImage(iconFile, IMAGE_UPSAMPLING_SCALE);
            if (!backgroundImage || !backgroundImage->bitmap) {
                _logger->write(Logger::Severity::ERROR, "PoiSymbolizer: failed to load icon file " + iconFile);
                return FeatureProcessor();
            }

            // Tint the bitmap with the static icon fill color
            vt::Color iconFillStatic = vt::Color(1, 1, 1, 1);
            {
                vt::ColorFunction iconFillFn = _iconFill.getFunction(exprContext);
                vt::FloatFunction iconOpFn   = _iconOpacity.getFunction(exprContext);
                // Evaluate at default ViewState
                vt::ViewState vs;
                vt::Color baseColor = iconFillFn(vs);
                float baseOpacity   = iconOpFn(vs);
                iconFillStatic = vt::Color(baseColor[0] * baseOpacity, baseColor[1] * baseOpacity, baseColor[2] * baseOpacity, baseColor[3] * baseOpacity);
            }
            std::shared_ptr<vt::BitmapImage> tintedImage = tintBitmapImage(backgroundImage, iconFillStatic);

            float bitmapW = static_cast<float>(tintedImage->bitmap->width);
            float bitmapH = static_cast<float>(tintedImage->bitmap->height);
            float iconHalfW = bitmapW * tintedImage->scale * 0.5f;
            float iconHalfH = bitmapH * tintedImage->scale * 0.5f;

            if (clip) {
                return FeatureProcessor();
            }

            // Build per-anchor processors lazily inside the returned lambda
            return [=, this](const FeatureCollection& featureCollection, vt::TileLayerBuilder& layerBuilder) {
                // N text+icon anchor processors (anchor 0 = highest priority) + optional icon-only fallback
                std::vector<vt::TileLayerBuilder::TextLabelProcessor> anchorProcs;
                vt::TileLayerBuilder::TextLabelProcessor iconOnlyProc;
                bool initialized = false;

                for (std::size_t featureIndex = 0; featureIndex < featureCollection.size(); featureIndex++) {
                    if (!initialized) {
                        cglib::vec2<float> bgOffset(-bitmapW * fontScale * 0.5f, -bitmapH * fontScale * 0.5f);
                        for (int i = 0; i < N; i++) {
                            if (!textFont) {
                                anchorProcs.push_back({});
                                continue;
                            }
                            vt::TextFormatter::Options anchorOpts = makeAnchorOptions(anchors[i], iconHalfW, iconHalfH, textMargin, fontScale, baseOptions);
                            vt::TextFormatter formatter(textFont, textSizeStatic, anchorOpts);
                            vt::TextLabelStyle style(placement, textFillFunc, textSizeFunc, textHaloFill, textHaloRadius, true, 0.0f, fontScale, bgOffset, tintedImage);
                            auto proc = layerBuilder.createTextLabelProcessor(style, formatter);
                            anchorProcs.push_back(std::move(proc));
                        }
                        if (canHideText && textFont) {
                            // icon-only fallback: empty text, icon as background, same style
                            vt::TextFormatter::Options centerOpts = baseOptions;
                            centerOpts.alignment = cglib::vec2<float>(0, 0);
                            centerOpts.offset    = cglib::vec2<float>(0, 0);
                            vt::TextFormatter formatter(textFont, textSizeStatic, centerOpts);
                            vt::TextLabelStyle style(placement, textFillFunc, textSizeFunc, textHaloFill, textHaloRadius, true, 0.0f, fontScale, bgOffset, tintedImage);
                            iconOnlyProc = layerBuilder.createTextLabelProcessor(style, formatter);
                        }
                        initialized = true;
                    }

                    long long localId = featureCollection.getLocalId(featureIndex);
                    long long baseId  = labelIdOverride ? *labelIdOverride : combineId(featureCollection.getFeatureId(featureIndex), textHash);
                    if (baseId == 0) baseId = generateId();
                    int slots = N + (canHideText ? 1 : 0);

                    auto processVertex = [&](const vt::TileLayerBuilder::Vertex& vertex, int geoPointIndex) {
                        // Anchor 0 gets the highest labelId (tried first by the culler)
                        for (int i = 0; i < N; i++) {
                            if (!anchorProcs[i]) continue;
                            long long labelId = static_cast<long long>(slots) * baseId + static_cast<long long>(N - i);
                            anchorProcs[i](localId, labelId, groupId, vertex, vt::TileLayerBuilder::Vertices(), text.empty() ? std::string() : text, placementPriority, minimumDistance, allowOverlapSameFeatureId, false, geoPointIndex, true);
                        }
                        if (canHideText && iconOnlyProc) {
                            // icon-only gets the lowest labelId; shown only when all text+icon anchors fail
                            long long labelId = static_cast<long long>(slots) * baseId + 0;
                            iconOnlyProc(localId, labelId, groupId, vertex, vt::TileLayerBuilder::Vertices(), std::string(), placementPriority, minimumDistance, allowOverlapSameFeatureId, false, geoPointIndex, true);
                        }
                    };

                    if (auto pointGeometry = std::get_if<PointGeometry>(featureCollection.getGeometry(featureIndex).get())) {
                        auto verticesList = pointGeometry->getVerticesList();
                        int geoIdx = 0;
                        for (const auto& vertices : verticesList) {
                            for (const auto& v : vertices) {
                                processVertex(v, geoIdx);
                            }
                            geoIdx++;
                        }
                    }
                    else {
                        vt::TileLayerBuilder::Vertices midPoints;
                        if (auto lineGeometry = std::get_if<LineGeometry>(featureCollection.getGeometry(featureIndex).get())) {
                            midPoints = lineGeometry->getMidPoints();
                        }
                        else if (auto polyGeometry = std::get_if<PolygonGeometry>(featureCollection.getGeometry(featureIndex).get())) {
                            midPoints = polyGeometry->getSurfacePoints();
                        }
                        for (const auto& v : midPoints) {
                            processVertex(v, 0);
                        }
                    }
                }
            };
        }

        // =====================================================================
        // MODE 2: poi-icon-name (text glyph icon)
        // =====================================================================
        if (!iconName.empty()) {
            std::shared_ptr<const vt::Font> iconFont = getIconFont(symbolizerContext, exprContext);
            // Build a combined font: text font with icon font as fallback
            // so both glyphs live in the same GlyphMap (text font's).
            std::shared_ptr<const vt::Font> textFont;
            {
                std::shared_ptr<const vt::Font> fallback = iconFont;
                std::string faceName   = _faceName.getValue(exprContext);
                std::string fontSetName= _fontSetName.getValue(exprContext);
                std::shared_ptr<const vt::Font> base = symbolizerContext.getSettings().getFallbackFont();
                if (!faceName.empty()) {
                    textFont = symbolizerContext.getFontManager()->getFont(faceName, fallback ? fallback : base);
                }
                else if (!fontSetName.empty()) {
                    std::shared_ptr<const vt::Font> f = fallback ? fallback : base;
                    for (const auto& fs : _fontSets) {
                        if (fs->getName() == fontSetName) {
                            for (auto it = fs->getFaceNames().rbegin(); it != fs->getFaceNames().rend(); it++) {
                                auto candidate = symbolizerContext.getFontManager()->getFont(it->getValue(exprContext), f);
                                if (candidate) f = candidate;
                            }
                            break;
                        }
                    }
                    textFont = f;
                }
                else {
                    textFont = fallback ? fallback : base;
                }
            }
            if (!textFont) {
                _logger->write(Logger::Severity::ERROR, "PoiSymbolizer: failed to load font for poi-icon-name");
                return FeatureProcessor();
            }

            // Pre-shape the icon glyph using the combined font at creation time
            // (iconName is a static string, not per-feature)
            std::vector<std::uint32_t> iconUtf32;
            iconUtf32.reserve(iconName.size());
            utf8::utf8to32(iconName.begin(), iconName.end(), std::back_inserter(iconUtf32));

            float iconSizeNorm = (textSizeStatic > 0) ? (iconSizeStatic / textSizeStatic) : 1.0f;
            std::vector<vt::Font::Glyph> iconGlyphs = textFont->shapeGlyphs(iconUtf32.data(), iconUtf32.size(), iconSizeNorm, false);

            // Center each icon glyph at (0,0), zero advance
            cglib::vec2<float> iconBBoxMin(0, 0), iconBBoxMax(0, 0);
            for (auto& g : iconGlyphs) {
                cglib::vec2<float> bboxMin = g.offset;
                cglib::vec2<float> bboxMax = g.offset + g.size;
                cglib::vec2<float> center  = (bboxMin + bboxMax) * 0.5f;
                g.offset  -= center;
                g.advance  = cglib::vec2<float>(0, 0);
                iconBBoxMin = cglib::vec2<float>(std::min(iconBBoxMin(0), g.offset(0)), std::min(iconBBoxMin(1), g.offset(1)));
                iconBBoxMax = cglib::vec2<float>(std::max(iconBBoxMax(0), g.offset(0) + g.size(0)), std::max(iconBBoxMax(1), g.offset(1) + g.size(1)));
            }
            // Icon half-dimensions in CSS pixels (approx)
            float iconHalfW = (iconBBoxMax(0) - iconBBoxMin(0)) * 0.5f * textSizeStatic;
            float iconHalfH = (iconBBoxMax(1) - iconBBoxMin(1)) * 0.5f * textSizeStatic;
            // Clamp to at least iconSizeStatic/2 if glyph not found
            if (iconHalfW < 1.0f) iconHalfW = iconSizeStatic * 0.5f;
            if (iconHalfH < 1.0f) iconHalfH = iconSizeStatic * 0.5f;

            if (canHideText) {
                // COMBINED icon+text per anchor (variantLabel) + icon-only fallback (lowest priority).
                // The culler will show the best-fitting anchor first; if none fit, shows icon-only.
                // All variants share the same style so the icon looks identical with or without text.
                return [=, this](const FeatureCollection& featureCollection, vt::TileLayerBuilder& layerBuilder) {
                    std::vector<vt::TileLayerBuilder::GlyphTextLabelProcessor> anchorProcs;
                    vt::TileLayerBuilder::GlyphTextLabelProcessor iconOnlyProc;
                    bool initialized = false;

                    for (std::size_t featureIndex = 0; featureIndex < featureCollection.size(); featureIndex++) {
                        if (!initialized) {
                            vt::TextLabelStyle combinedStyle(placement, textFillFunc, textSizeFunc, textHaloFill, textHaloRadius, true, 0.0f, fontScale, cglib::vec2<float>(0, 0), nullptr);
                            for (int i = 0; i < N; i++) {
                                auto proc = layerBuilder.createGlyphTextLabelProcessor(combinedStyle, textFont);
                                anchorProcs.push_back(std::move(proc));
                            }
                            // icon-only fallback uses the same style as the combined labels
                            iconOnlyProc = layerBuilder.createGlyphTextLabelProcessor(combinedStyle, textFont);
                            initialized = true;
                        }

                        long long localId = featureCollection.getLocalId(featureIndex);
                        long long baseId  = labelIdOverride ? *labelIdOverride : combineId(featureCollection.getFeatureId(featureIndex), textHash);
                        if (baseId == 0) baseId = generateId();
                        int slots = N + 1; // N text+icon anchors + 1 icon-only

                        auto processVertex = [&](const vt::TileLayerBuilder::Vertex& vertex, int geoIdx) {
                            // Anchor 0 gets the highest labelId (highest culler priority)
                            for (int i = 0; i < N; i++) {
                                if (!anchorProcs[i]) continue;
                                long long labelId = static_cast<long long>(slots) * baseId + static_cast<long long>(N - i);
                                std::vector<vt::Font::Glyph> glyphs = iconGlyphs;
                                if (!text.empty()) {
                                    vt::TextFormatter::Options anchorOpts = makeAnchorOptions(anchors[i], iconHalfW, iconHalfH, textMargin, fontScale, baseOptions);
                                    vt::TextFormatter formatter(textFont, textSizeStatic, anchorOpts);
                                    auto textGlyphs = formatter.format(text, 1.0f);
                                    glyphs.insert(glyphs.end(), textGlyphs.begin(), textGlyphs.end());
                                }
                                anchorProcs[i](localId, labelId, groupId, vertex, vt::TileLayerBuilder::Vertices(), std::move(glyphs), placementPriority, minimumDistance, allowOverlapSameFeatureId, false, geoIdx, true);
                            }
                            // Icon-only fallback: just the icon glyphs, lowest labelId
                            if (iconOnlyProc) {
                                long long labelId = static_cast<long long>(slots) * baseId + 0;
                                iconOnlyProc(localId, labelId, groupId, vertex, vt::TileLayerBuilder::Vertices(), iconGlyphs, placementPriority, minimumDistance, allowOverlapSameFeatureId, false, geoIdx, true);
                            }
                        };

                        if (auto pointGeometry = std::get_if<PointGeometry>(featureCollection.getGeometry(featureIndex).get())) {
                            int geoIdx = 0;
                            for (const auto& verts : pointGeometry->getVerticesList()) {
                                for (const auto& v : verts) processVertex(v, geoIdx);
                                geoIdx++;
                            }
                        }
                        else {
                            vt::TileLayerBuilder::Vertices midPoints;
                            if (auto lineGeometry = std::get_if<LineGeometry>(featureCollection.getGeometry(featureIndex).get())) {
                                midPoints = lineGeometry->getMidPoints();
                            }
                            else if (auto polyGeometry = std::get_if<PolygonGeometry>(featureCollection.getGeometry(featureIndex).get())) {
                                midPoints = polyGeometry->getSurfacePoints();
                            }
                            for (const auto& v : midPoints) processVertex(v, 0);
                        }
                    }
                };
            }
            else {
                // COMBINED labels: icon glyphs + text glyphs in one TileLabel per anchor
                return [=, this](const FeatureCollection& featureCollection, vt::TileLayerBuilder& layerBuilder) {
                    std::vector<vt::TileLayerBuilder::GlyphTextLabelProcessor> anchorProcs;

                    for (std::size_t featureIndex = 0; featureIndex < featureCollection.size(); featureIndex++) {
                        if (anchorProcs.empty()) {
                            for (int i = 0; i < N; i++) {
                                vt::TextLabelStyle style(placement, textFillFunc, textSizeFunc, textHaloFill, textHaloRadius, true, 0.0f, fontScale, cglib::vec2<float>(0, 0), nullptr);
                                auto proc = layerBuilder.createGlyphTextLabelProcessor(style, textFont);
                                anchorProcs.push_back(std::move(proc));
                            }
                        }

                        long long localId = featureCollection.getLocalId(featureIndex);
                        long long baseId  = labelIdOverride ? *labelIdOverride : combineId(featureCollection.getFeatureId(featureIndex), textHash);
                        if (baseId == 0) baseId = generateId();

                        auto processVertex = [&](const vt::TileLayerBuilder::Vertex& vertex, int geoIdx) {
                            for (int i = 0; i < N; i++) {
                                if (!anchorProcs[i]) continue;
                                // Use slots=N+1 and offsets 1..N (consistent with Mode 1); offset 0 is reserved
                                long long labelId = static_cast<long long>(N + 1) * baseId + static_cast<long long>(N - i);
                                // Combined: icon at center + text at anchor
                                std::vector<vt::Font::Glyph> glyphs = iconGlyphs;
                                if (!text.empty()) {
                                    vt::TextFormatter::Options anchorOpts = makeAnchorOptions(anchors[i], iconHalfW, iconHalfH, textMargin, fontScale, baseOptions);
                                    vt::TextFormatter formatter(textFont, textSizeStatic, anchorOpts);
                                    auto textGlyphs = formatter.format(text, 1.0f);
                                    glyphs.insert(glyphs.end(), textGlyphs.begin(), textGlyphs.end());
                                }
                                anchorProcs[i](localId, labelId, groupId, vertex, vt::TileLayerBuilder::Vertices(), std::move(glyphs), placementPriority, minimumDistance, allowOverlapSameFeatureId, false, geoIdx, true);
                            }
                        };

                        if (auto pointGeometry = std::get_if<PointGeometry>(featureCollection.getGeometry(featureIndex).get())) {
                            int geoIdx = 0;
                            for (const auto& verts : pointGeometry->getVerticesList()) {
                                for (const auto& v : verts) processVertex(v, geoIdx);
                                geoIdx++;
                            }
                        }
                        else {
                            vt::TileLayerBuilder::Vertices midPoints;
                            if (auto lineGeometry = std::get_if<LineGeometry>(featureCollection.getGeometry(featureIndex).get())) {
                                midPoints = lineGeometry->getMidPoints();
                            }
                            else if (auto polyGeometry = std::get_if<PolygonGeometry>(featureCollection.getGeometry(featureIndex).get())) {
                                midPoints = polyGeometry->getSurfacePoints();
                            }
                            for (const auto& v : midPoints) processVertex(v, 0);
                        }
                    }
                };
            }
        }

        // =====================================================================
        // MODE 3: Text-only (no icon) — variable anchor text labels
        // =====================================================================
        {
            std::shared_ptr<const vt::Font> textFont = getTextFont(symbolizerContext, exprContext);
            if (!textFont) {
                _logger->write(Logger::Severity::ERROR, "PoiSymbolizer: failed to load text font");
                return FeatureProcessor();
            }
            if (text.empty()) {
                return FeatureProcessor();
            }

            return [=, this](const FeatureCollection& featureCollection, vt::TileLayerBuilder& layerBuilder) {
                std::vector<vt::TileLayerBuilder::TextLabelProcessor> anchorProcs;

                for (std::size_t featureIndex = 0; featureIndex < featureCollection.size(); featureIndex++) {
                    if (anchorProcs.empty()) {
                        for (int i = 0; i < N; i++) {
                            vt::TextFormatter::Options anchorOpts = baseOptions;
                            if (N > 1) {
                                // Multiple anchors: spread based on anchor string
                                anchorOpts = makeAnchorOptions(anchors[i], 0, 0, 0, fontScale, baseOptions);
                            }
                            vt::TextFormatter formatter(textFont, textSizeStatic, anchorOpts);
                            vt::TextLabelStyle style(placement, textFillFunc, textSizeFunc, textHaloFill, textHaloRadius, true, 0.0f, fontScale, cglib::vec2<float>(0, 0), nullptr);
                            auto proc = layerBuilder.createTextLabelProcessor(style, formatter);
                            anchorProcs.push_back(std::move(proc));
                        }
                    }

                    long long localId = featureCollection.getLocalId(featureIndex);
                    long long baseId  = labelIdOverride ? *labelIdOverride : combineId(featureCollection.getFeatureId(featureIndex), textHash);
                    if (baseId == 0) baseId = generateId();

                    auto processVertex = [&](const vt::TileLayerBuilder::Vertex& vertex, int geoIdx) {
                        for (int i = 0; i < N; i++) {
                            if (!anchorProcs[i]) continue;
                            long long labelId = static_cast<long long>(N + 1) * baseId + static_cast<long long>(N - i);
                            anchorProcs[i](localId, labelId, groupId, vertex, vt::TileLayerBuilder::Vertices(), text, placementPriority, minimumDistance, allowOverlapSameFeatureId, false, geoIdx, true);
                        }
                    };

                    if (auto pointGeometry = std::get_if<PointGeometry>(featureCollection.getGeometry(featureIndex).get())) {
                        int geoIdx = 0;
                        for (const auto& verts : pointGeometry->getVerticesList()) {
                            for (const auto& v : verts) processVertex(v, geoIdx);
                            geoIdx++;
                        }
                    }
                    else {
                        vt::TileLayerBuilder::Vertices midPoints;
                        if (auto lineGeometry = std::get_if<LineGeometry>(featureCollection.getGeometry(featureIndex).get())) {
                            midPoints = lineGeometry->getMidPoints();
                        }
                        else if (auto polyGeometry = std::get_if<PolygonGeometry>(featureCollection.getGeometry(featureIndex).get())) {
                            midPoints = polyGeometry->getSurfacePoints();
                        }
                        for (const auto& v : midPoints) processVertex(v, 0);
                    }
                }
            };
        }
    }

    std::shared_ptr<const vt::Font> PoiSymbolizer::getTextFont(const SymbolizerContext& symbolizerContext, const ExpressionContext& exprContext) const {
        std::shared_ptr<const vt::Font> font = symbolizerContext.getSettings().getFallbackFont();
        std::string faceName    = _faceName.getValue(exprContext);
        std::string fontSetName = _fontSetName.getValue(exprContext);
        if (!faceName.empty()) {
            font = symbolizerContext.getFontManager()->getFont(faceName, font);
        }
        else if (!fontSetName.empty()) {
            for (const auto& fontSet : _fontSets) {
                if (fontSet->getName() == fontSetName) {
                    for (auto it = fontSet->getFaceNames().rbegin(); it != fontSet->getFaceNames().rend(); it++) {
                        auto candidate = symbolizerContext.getFontManager()->getFont(it->getValue(exprContext), font);
                        if (candidate) font = candidate;
                    }
                    break;
                }
            }
        }
        return font;
    }

    std::shared_ptr<const vt::Font> PoiSymbolizer::getIconFont(const SymbolizerContext& symbolizerContext, const ExpressionContext& exprContext) const {
        std::shared_ptr<const vt::Font> font = symbolizerContext.getSettings().getFallbackFont();
        std::string faceName    = _iconFaceName.getValue(exprContext);
        std::string fontSetName = _iconFontSetName.getValue(exprContext);
        if (!faceName.empty()) {
            font = symbolizerContext.getFontManager()->getFont(faceName, font);
        }
        else if (!fontSetName.empty()) {
            for (const auto& fontSet : _fontSets) {
                if (fontSet->getName() == fontSetName) {
                    for (auto it = fontSet->getFaceNames().rbegin(); it != fontSet->getFaceNames().rend(); it++) {
                        auto candidate = symbolizerContext.getFontManager()->getFont(it->getValue(exprContext), font);
                        if (candidate) font = candidate;
                    }
                    break;
                }
            }
        }
        else {
            // Fall back to text font if no icon font specified
            font = getTextFont(symbolizerContext, exprContext);
        }
        return font;
    }

    std::string PoiSymbolizer::getTransformedText(const ExpressionContext& exprContext) const {
        std::string text = _name.getValue(exprContext);
        return _textTransform.getValue(exprContext)(text);
    }

    std::vector<std::string> PoiSymbolizer::parseAnchorList(const std::string& anchorStr) {
        std::vector<std::string> result;
        std::vector<std::string> parts;
        boost::split(parts, anchorStr, boost::is_any_of(","));
        for (auto& p : parts) {
            boost::trim(p);
            if (!p.empty()) {
                result.push_back(p);
            }
        }
        return result;
    }

    vt::TextFormatter::Options PoiSymbolizer::makeAnchorOptions(const std::string& anchor, float iconHalfW, float iconHalfH, float margin, float fontScale, const vt::TextFormatter::Options& baseOptions) {
        float alignX = 0.0f, alignY = 0.0f;
        float offX   = 0.0f, offY   = 0.0f;

        // Compute offset: positive offY = text above, negative offY = text below
        // positive offX = text to the right, negative offX = text to the left
        if (anchor == "top") {
            offY   = (iconHalfH + margin) * fontScale;
            alignY = 1.0f;   // bottom of text at offset line
        }
        else if (anchor == "bottom") {
            offY   = -(iconHalfH + margin) * fontScale;
            alignY = -1.0f;  // top of text at offset line
        }
        else if (anchor == "left") {
            offX   = -(iconHalfW + margin) * fontScale;
            alignX = 1.0f;   // right edge of text at offset line
        }
        else if (anchor == "right") {
            offX   = (iconHalfW + margin) * fontScale;
            alignX = -1.0f;  // left edge of text at offset line
        }
        else if (anchor == "top-left") {
            offX   = -(iconHalfW + margin) * fontScale;
            offY   = (iconHalfH + margin) * fontScale;
            alignX = 1.0f;  alignY = 1.0f;
        }
        else if (anchor == "top-right") {
            offX   = (iconHalfW + margin) * fontScale;
            offY   = (iconHalfH + margin) * fontScale;
            alignX = -1.0f; alignY = 1.0f;
        }
        else if (anchor == "bottom-left") {
            offX   = -(iconHalfW + margin) * fontScale;
            offY   = -(iconHalfH + margin) * fontScale;
            alignX = 1.0f;  alignY = -1.0f;
        }
        else if (anchor == "bottom-right") {
            offX   = (iconHalfW + margin) * fontScale;
            offY   = -(iconHalfH + margin) * fontScale;
            alignX = -1.0f; alignY = -1.0f;
        }
        // "center": all zeros (text centered on icon)

        vt::TextFormatter::Options opts = baseOptions;
        opts.alignment = cglib::vec2<float>(alignX, alignY);
        opts.offset    = cglib::vec2<float>(offX, offY);
        return opts;
    }

    std::shared_ptr<vt::BitmapImage> PoiSymbolizer::tintBitmapImage(const std::shared_ptr<const vt::BitmapImage>& image, const vt::Color& tintColor) {
        if (!image || !image->bitmap) {
            return nullptr;
        }
        // If tint is white/opaque-1 (r=g=b=a=1), return original
        if (tintColor[0] >= 1.0f && tintColor[1] >= 1.0f && tintColor[2] >= 1.0f && tintColor[3] >= 1.0f) {
            return std::make_shared<vt::BitmapImage>(image->scale, image->bitmap);
        }

        const vt::Bitmap& src = *image->bitmap;
        std::vector<std::uint32_t> data(src.data.begin(), src.data.end());

        uint8_t tr = static_cast<uint8_t>(std::clamp(tintColor[0] * 255.0f, 0.0f, 255.0f));
        uint8_t tg = static_cast<uint8_t>(std::clamp(tintColor[1] * 255.0f, 0.0f, 255.0f));
        uint8_t tb = static_cast<uint8_t>(std::clamp(tintColor[2] * 255.0f, 0.0f, 255.0f));
        uint8_t ta = static_cast<uint8_t>(std::clamp(tintColor[3] * 255.0f, 0.0f, 255.0f));

        for (std::uint32_t& pixel : data) {
            uint8_t r = (pixel >> 0)  & 0xFF;
            uint8_t g = (pixel >> 8)  & 0xFF;
            uint8_t b = (pixel >> 16) & 0xFF;
            uint8_t a = (pixel >> 24) & 0xFF;
            r = static_cast<uint8_t>((static_cast<unsigned>(r) * tr) / 255u);
            g = static_cast<uint8_t>((static_cast<unsigned>(g) * tg) / 255u);
            b = static_cast<uint8_t>((static_cast<unsigned>(b) * tb) / 255u);
            a = static_cast<uint8_t>((static_cast<unsigned>(a) * ta) / 255u);
            pixel = static_cast<std::uint32_t>(r) | (static_cast<std::uint32_t>(g) << 8) | (static_cast<std::uint32_t>(b) << 16) | (static_cast<std::uint32_t>(a) << 24);
        }

        auto tintedBitmap = std::make_shared<vt::Bitmap>(src.width, src.height, std::move(data));
        return std::make_shared<vt::BitmapImage>(image->scale, tintedBitmap);
    }
}
