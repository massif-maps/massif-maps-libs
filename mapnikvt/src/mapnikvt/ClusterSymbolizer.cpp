#include "ClusterSymbolizer.h"
#include "Rule.h"

#include <vector>
#include <tuple>

namespace carto::mvt {
    ClusterSymbolizer::FeatureProcessor ClusterSymbolizer::createFeatureProcessor(const ExpressionContext& exprContext, const SymbolizerContext& symbolizerContext, const std::shared_ptr<const Rule>& rule) const {
        // ClusterSymbolizer works similarly to ShieldSymbolizer
        // The implementation is nearly identical, just using cluster-* properties instead of shield-*
        std::shared_ptr<const vt::Font> font = getFont(symbolizerContext, exprContext);
        if (!font) {
            std::string faceName = _faceName.getValue(exprContext);
            std::string fontSetName = _fontSetName.getValue(exprContext);
            _logger->write(Logger::Severity::ERROR, "Failed to load cluster font " + (!faceName.empty() ? faceName : fontSetName));
            return FeatureProcessor();
        }

        std::string file = _clusterFile.getValue(exprContext);
        std::shared_ptr<const vt::BitmapImage> backgroundImage;
        if (!file.empty()) {
            backgroundImage = symbolizerContext.getBitmapManager()->loadBitmapImage(file, IMAGE_UPSAMPLING_SCALE);
        }

        bool allowOverlap = _allowOverlap.getValue(exprContext);
        bool clip = _clip.isDefined() ? _clip.getValue(exprContext) : allowOverlap;
        bool allowOverlapSameFeatureId = _allowOverlapSameFeatureId.getValue(exprContext);
        bool sameFeatureIdDependent = _sameFeatureIdDependent.getValue(exprContext);
        float clusterDx = _clusterDx.getValue(exprContext);
        float clusterDy = _clusterDy.getValue(exprContext);
        
        // ClusterSymbolizer itself should only enable clustering on its own labels if marker-cluster-enabled is true
        bool markerClusterEnabled = _markerClusterEnabled.getValue(exprContext);
        bool clusterEnabled = markerClusterEnabled;
        float clusterDistance = _clusterDistance.getValue(exprContext);

        float tileSize = symbolizerContext.getSettings().getTileSize();
        float fontScale = symbolizerContext.getSettings().getFontScale();
        float bitmapSize = 0;
        if (backgroundImage && backgroundImage->bitmap) {
            bitmapSize = static_cast<float>(std::max(backgroundImage->bitmap->width, backgroundImage->bitmap->height)) * fontScale;
        }
        float minimumDistance = _minimumDistance.getValue(exprContext);
        float placementPriority = _placementPriority.getValue(exprContext);
        float orientationAngle = _orientationAngle.getValue(exprContext);
        float sizeStatic = _size.getStaticValue(exprContext);
        bool unlockImage = _unlockImage.getValue(exprContext);

        vt::TextFormatter textFormatter(font, sizeStatic, getFormatterOptions(symbolizerContext, exprContext));
        vt::TextFormatter::Options clusterFormatterOptions = textFormatter.getOptions();
        clusterFormatterOptions.offset = cglib::vec2<float>(clusterDx * fontScale, -clusterDy * fontScale);
        vt::TextFormatter clusterFormatter(font, sizeStatic, clusterFormatterOptions);
        vt::CompOp compOp = _compOp.getValue(exprContext);
        vt::LabelOrientation placement = getPlacement(exprContext);
        vt::LabelOrientation orientation = (placement != vt::LabelOrientation::LINE ? placement : vt::LabelOrientation::BILLBOARD_2D);
        
        vt::ColorFunction fillFunc = _fillFuncBuilder.createColorOpacityFunction(_fill.getFunction(exprContext), _opacity.getFunction(exprContext));
        vt::FloatFunction sizeFunc = _sizeFuncBuilder.createScaledFloatFunction(_size.getFunction(exprContext), fontScale);
        vt::ColorFunction haloFillFunc = _haloFillFuncBuilder.createColorOpacityFunction(_haloFill.getFunction(exprContext), _haloOpacity.getFunction(exprContext));
        vt::FloatFunction haloRadiusFunc = _haloRadiusFuncBuilder.createScaledFloatFunction(_haloRadius.getFunction(exprContext), fontScale);

        vt::TileId tileId = exprContext.getTileId();
        std::string text = getTransformedText(exprContext);
        std::size_t hash = std::hash<std::string>()(text);

        std::optional<long long> labelIdOverride;
        if (_featureId.isDefined()) {
            labelIdOverride = convertId(_featureId.getValue(exprContext));
        }

        float textSize = bitmapSize < 0 ? (placement == vt::LabelOrientation::LINE ? calculateTextSize(textFormatter.getFont(), text, textFormatter).size()(0) : 0) : bitmapSize;
        float spacing = _spacing.getValue(exprContext);
        long long groupId = (allowOverlap ? -1 : 0);
        if (!allowOverlap && minimumDistance > 0) {
            groupId = 1;
        }

        cglib::vec2<float> backgroundOffset(0, 0);
        vt::TextFormatter formatter = (unlockImage ? textFormatter : clusterFormatter);
        if (backgroundImage && backgroundImage->bitmap) {
            if (unlockImage) {
                backgroundOffset = cglib::vec2<float>(-backgroundImage->bitmap->width * fontScale * 0.5f + clusterFormatterOptions.offset(0), -backgroundImage->bitmap->height * fontScale * 0.5f + clusterFormatterOptions.offset(1));
            }
            else {
                backgroundOffset = cglib::vec2<float>(-backgroundImage->bitmap->width * fontScale * 0.5f, -backgroundImage->bitmap->height * fontScale * 0.5f);
            }
        }

        if (clip) {
            return [compOp, fillFunc, haloFillFunc, sizeFunc, haloRadiusFunc, fontScale, placement, text, orientationAngle, formatter, backgroundOffset, backgroundImage, spacing, textSize, tileSize, this](const FeatureCollection& featureCollection, vt::TileLayerBuilder& layerBuilder) {
                vt::TextStyle style(compOp, fillFunc, sizeFunc, haloFillFunc, haloRadiusFunc, orientationAngle, fontScale, backgroundOffset, backgroundImage);
                vt::TileLayerBuilder::TextProcessor textProcessor;
                for (std::size_t featureIndex = 0; featureIndex < featureCollection.size(); featureIndex++) {
                    if (!textProcessor) {
                        textProcessor = layerBuilder.createTextProcessor(style, formatter);
                        if (!textProcessor) {
                            return;
                        }
                    }

                    if (auto pointGeometry = std::get_if<PointGeometry>(featureCollection.getGeometry(featureIndex).get())) {
                        auto verticesList = pointGeometry->getVerticesList();
                        int index = 0;
                        for (const auto& vertices : verticesList) {
                            for (const auto &vertex: vertices) {
                                textProcessor(featureCollection.getLocalId(featureIndex), vertex, text, index);
                            }
                            index++;
                        }
                    }
                    else if (placement != vt::LabelOrientation::LINE) {
                        if (auto lineGeometry = std::get_if<LineGeometry>(featureCollection.getGeometry(featureIndex).get())) {
                            for (const auto& vertices : lineGeometry->getVerticesList()) {
                                for (const auto& vertex : vertices) {
                                    textProcessor(featureCollection.getLocalId(featureIndex), vertex, text, 0);
                                }
                            }
                        }
                        else if (auto polygonGeometry = std::get_if<PolygonGeometry>(featureCollection.getGeometry(featureIndex).get())) {
                            for (const auto& vertex : polygonGeometry->getSurfacePoints()) {
                                textProcessor(featureCollection.getLocalId(featureIndex), vertex, text, 0);
                            }
                        }
                    }
                }
            };
        }

        return [compOp, fillFunc, haloFillFunc, sizeFunc, haloRadiusFunc, fontScale, placement, orientation, text, hash, orientationAngle, formatter, backgroundOffset, backgroundImage, spacing, textSize, tileId, tileSize, labelIdOverride, groupId, placementPriority, minimumDistance, allowOverlapSameFeatureId, sameFeatureIdDependent, clusterEnabled, clusterDistance, this](const FeatureCollection& featureCollection, vt::TileLayerBuilder& layerBuilder) {
            vt::TextLabelStyle style(orientation, fillFunc, sizeFunc, haloFillFunc, haloRadiusFunc, true, orientationAngle, fontScale, backgroundOffset, backgroundImage);
            vt::TileLayerBuilder::TextLabelProcessor textProcessor;
            for (std::size_t featureIndex = 0; featureIndex < featureCollection.size(); featureIndex++) {
                if (!textProcessor) {
                    textProcessor = layerBuilder.createTextLabelProcessor(style, formatter);
                    if (!textProcessor) {
                        return;
                    }
                }

                long long localId = featureCollection.getLocalId(featureIndex);
                long long labelId = combineId(featureCollection.getFeatureId(featureIndex), hash);
                if (labelIdOverride) {
                    labelId = *labelIdOverride;
                    if (!labelId) {
                        labelId = generateId();
                    }
                }

                if (auto pointGeometry = std::get_if<PointGeometry>(featureCollection.getGeometry(featureIndex).get())) {
                    auto verticesList = pointGeometry->getVerticesList();
                    int index = 0;
                    for (const auto& vertices : verticesList) {
                        for (const auto &vertex: vertices) {
                            textProcessor(localId, 10 * labelId + index, groupId, vertex, vt::TileLayerBuilder::Vertices(), text, placementPriority, minimumDistance, allowOverlapSameFeatureId, sameFeatureIdDependent, index, clusterEnabled, clusterDistance);
                        }
                        index++;
                    }
                }
                else if (placement != vt::LabelOrientation::LINE) {
                    if (auto lineGeometry = std::get_if<LineGeometry>(featureCollection.getGeometry(featureIndex).get())) {
                        for (const auto& vertices : lineGeometry->getVerticesList()) {
                            textProcessor(localId, labelId, groupId, std::optional<vt::TileLayerBuilder::Vertex>(), vertices, text, placementPriority, minimumDistance, allowOverlapSameFeatureId, sameFeatureIdDependent, 0, clusterEnabled, clusterDistance);
                        }
                    }
                    else if (auto polygonGeometry = std::get_if<PolygonGeometry>(featureCollection.getGeometry(featureIndex).get())) {
                        for (const auto& vertex : polygonGeometry->getSurfacePoints()) {
                            textProcessor(localId, labelId, groupId, vertex, vt::TileLayerBuilder::Vertices(), text, placementPriority, minimumDistance, allowOverlapSameFeatureId, sameFeatureIdDependent, 0, clusterEnabled, clusterDistance);
                        }
                    }
                }
                else {
                    vt::TileLayerBuilder::VerticesList verticesList;
                    if (auto lineGeometry = std::get_if<LineGeometry>(featureCollection.getGeometry(featureIndex).get())) {
                        verticesList = lineGeometry->getVerticesList();
                    }
                    else if (auto polygonGeometry = std::get_if<PolygonGeometry>(featureCollection.getGeometry(featureIndex).get())) {
                        verticesList = polygonGeometry->getClosedOuterRings(true);
                    }

                    for (const auto& vertices : verticesList) {
                        if (spacing <= 0) {
                            textProcessor(localId, labelId, groupId, std::optional<vt::TileLayerBuilder::Vertex>(), vertices, text, placementPriority, minimumDistance, allowOverlapSameFeatureId, sameFeatureIdDependent, 0, false, 0.0f);
                            continue;
                        }

                        for (const auto& transformedPoints : generateLinePoints(vertices, spacing, textSize, tileSize, false)) {
                            int counter = 0;
                            for (const auto& vertex : transformedPoints.second) {
                                long long generatedLabelId = combineId(labelId, std::hash<vt::TileId>()(tileId) * 63 + counter);
                                textProcessor(localId, generatedLabelId, groupId, vertex, vertices, text, placementPriority, minimumDistance, allowOverlapSameFeatureId, sameFeatureIdDependent, 0, clusterEnabled, clusterDistance);
                                counter++;
                            }
                        }
                    }
                }
            }
        };
    }
}
