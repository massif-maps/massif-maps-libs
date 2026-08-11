/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _CARTO_VT_TILEGEOMETRY_H_
#define _CARTO_VT_TILEGEOMETRY_H_

#include "Bitmap.h"
#include "Color.h"
#include "StrokeMap.h"
#include "VertexArray.h"
#include "Styles.h"

#include <memory>
#include <optional>
#include <array>
#include <vector>
#include <algorithm>

#include <cglib/mat.h>

namespace carto::vt {
    class TileGeometry final {
    public:
        enum class Type {
            NONE, POINT, LINE, POLYGON, POLYGON3D
        };

        struct StyleParameters {
            static constexpr int MAX_PARAMETERS = 16;

            int parameterCount;
            std::array<ColorFunction, MAX_PARAMETERS> colorFuncs;
            std::array<FloatFunction, MAX_PARAMETERS> widthFuncs; // for lines, points
            std::array<FloatFunction, MAX_PARAMETERS> offsetFuncs; // for lines, points (stroke width in case of points)
            std::array<float, MAX_PARAMETERS> strokeScales; // for patterned lines
            std::shared_ptr<const BitmapPattern> pattern;
            std::optional<cglib::vec2<float>> translate;
            CompOp compOp;
            int glyphRenderSize;

            StyleParameters() : parameterCount(0), colorFuncs(), widthFuncs(), offsetFuncs(), strokeScales(), pattern(), translate(), compOp(CompOp::SRC_OVER), glyphRenderSize(64) { }
        };

        // Where one feature's vertices live, so its style slot can be repointed after the tile was
        // built - this is what lets a style parameter that selects a feature repaint instead of
        // re-decoding. Only recorded for geometries whose style asked for it.
        struct FeatureStyleRange {
            long long id;
            std::uint32_t firstVertex;
            std::uint32_t vertexCount;
            std::uint8_t styleIndex;
        };

        struct VertexGeometryLayoutParameters {
            int vertexSize;
            int dimensions;
            int coordOffset;
            int attribsOffset;
            int texCoordOffset;
            int normalOffset;
            int binormalOffset;
            int heightOffset;
            float coordScale;
            float texCoordScale;
            float binormalScale;
            float heightScale;

            VertexGeometryLayoutParameters() : vertexSize(0), dimensions(2), coordOffset(-1), attribsOffset(-1), texCoordOffset(-1), normalOffset(-1), binormalOffset(-1), heightOffset(-1), coordScale(0), texCoordScale(0), binormalScale(0), heightScale(0) { }
        };

        explicit TileGeometry(Type type, float geomScale, const StyleParameters& styleParameters, const VertexGeometryLayoutParameters& vertexGeometryLayoutParameters, VertexArray<std::uint8_t> vertexGeometry, VertexArray<std::uint16_t> indices, std::vector<std::pair<std::size_t, long long>> ids, std::vector<std::pair<std::size_t, std::uint16_t>> geoPosIndexes) : _type(type), _geomScale(geomScale), _styleParameters(styleParameters), _vertexGeometryLayoutParameters(vertexGeometryLayoutParameters), _indicesCount(static_cast<unsigned int>(indices.size())), _vertexGeometry(std::move(vertexGeometry)), _indices(std::move(indices)), _ids(std::move(ids)), _geoPosIndexes(std::move(geoPosIndexes)), _geoPosIndexesCount(static_cast<unsigned int>(indices.size())) { }

        Type getType() const { return _type; }
        float getGeometryScale() const { return _geomScale; }
        const StyleParameters& getStyleParameters() const { return _styleParameters; }
        const VertexGeometryLayoutParameters& getVertexGeometryLayoutParameters() const { return _vertexGeometryLayoutParameters; }
        unsigned int getIndicesCount() const { return _indicesCount; }
        unsigned int getGeoPosIndexesCount() const { return _geoPosIndexesCount; }

        const VertexArray<std::uint8_t>& getVertexGeometry() const { return _vertexGeometry; }
        const VertexArray<std::uint16_t>& getIndices() const { return _indices; }
        const std::vector<std::pair<std::size_t, long long>>& getIds() const { return _ids; }
        const std::vector<std::pair<std::size_t, std::uint16_t>>& getGeoPosIndexes() const { return _geoPosIndexes; }

        const std::vector<FeatureStyleRange>& getFeatureStyleRanges() const { return _featureStyleRanges; }

        void setFeatureStyleRanges(std::vector<FeatureStyleRange> featureStyleRanges) { _featureStyleRanges = std::move(featureStyleRanges); }

        // Repoints one feature at another of the geometry's style slots, in the vertex data that is
        // already uploaded. Returns true if anything changed, in which case the renderer re-uploads
        // the dirty byte range before the next draw.
        bool setFeatureStyleIndex(std::size_t featureIndex, int styleIndex) {
            FeatureStyleRange& range = _featureStyleRanges.at(featureIndex);
            if (range.styleIndex == styleIndex || _vertexGeometryLayoutParameters.attribsOffset < 0 || _vertexGeometry.empty()) {
                return false;
            }
            std::size_t vertexSize = _vertexGeometryLayoutParameters.vertexSize;
            std::size_t first = range.firstVertex * vertexSize + _vertexGeometryLayoutParameters.attribsOffset;
            for (std::uint32_t i = 0; i < range.vertexCount; i++) {
                _vertexGeometry[first + i * vertexSize] = static_cast<std::uint8_t>(styleIndex);
            }
            range.styleIndex = static_cast<std::uint8_t>(styleIndex);
            std::size_t last = first + (range.vertexCount > 0 ? (range.vertexCount - 1) * vertexSize : 0) + 1;
            _dirtyVertexBytes = (_dirtyVertexBytes ? std::make_pair(std::min(_dirtyVertexBytes->first, first), std::max(_dirtyVertexBytes->second, last)) : std::make_pair(first, last));
            return true;
        }

        const std::optional<std::pair<std::size_t, std::size_t>>& getDirtyVertexBytes() const { return _dirtyVertexBytes; }

        void clearDirtyVertexBytes() { _dirtyVertexBytes.reset(); }

        void releaseVertexArrays() {
            if (_featureStyleRanges.empty()) { // the vertex data is what a style slot change patches
                _vertexGeometry.clear();
                _vertexGeometry.shrink_to_fit();
            }
            _indices.clear();
            _indices.shrink_to_fit();
            _ids.clear();
            _ids.shrink_to_fit();
            _geoPosIndexes.clear();
            _geoPosIndexes.shrink_to_fit();
        }


        std::size_t getFeatureCount() const {
            switch (_type) {
            case Type::POINT:
            case Type::LINE:
                return _indicesCount / 6;
            case Type::POLYGON:
            case Type::POLYGON3D:
                return _indicesCount / 3;
            default:
                return 0;
            }
        }

        std::size_t getResidentSize() const {
            return 16 + _vertexGeometry.size() * sizeof(std::uint8_t) + _indices.size() * sizeof(std::uint16_t) + _ids.size() * sizeof(std::pair<std::size_t, long long>);
        }

    private:
        const Type _type;
        const float _geomScale;
        const StyleParameters _styleParameters;
        const VertexGeometryLayoutParameters _vertexGeometryLayoutParameters;
        const unsigned int _indicesCount; // real count, even if indices are released
        const unsigned int _geoPosIndexesCount;

        std::vector<FeatureStyleRange> _featureStyleRanges;
        std::optional<std::pair<std::size_t, std::size_t>> _dirtyVertexBytes; // byte range to re-upload

        VertexArray<std::uint8_t> _vertexGeometry;
        VertexArray<std::uint16_t> _indices;
        std::vector<std::pair<std::size_t, long long>> _ids; // vertex count, feature id
        std::vector<std::pair<std::size_t, std::uint16_t>> _geoPosIndexes; // vertex count, feature id
    };
}

#endif
