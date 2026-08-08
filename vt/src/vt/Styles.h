/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _CARTO_VT_STYLES_H_
#define _CARTO_VT_STYLES_H_

#include "Color.h"
#include "Transform.h"
#include "Bitmap.h"
#include "Font.h"
#include "ViewState.h"
#include "StrokeMap.h"
#include "GlyphMap.h"
#include "TextFormatter.h"
#include "UnaryFunction.h"

#include <optional>

#include <cglib/vec.h>
#include <cglib/mat.h>

namespace carto::vt {
    using FloatFunction = UnaryFunction<float, ViewState>;
    using ColorFunction = UnaryFunction<Color, ViewState>;

    enum class CompOp {
        SRC, SRC_OVER, SRC_IN, SRC_ATOP, 
        DST, DST_OVER, DST_IN, DST_ATOP,
        ZERO, PLUS, MINUS, MULTIPLY, SCREEN,
        DARKEN, LIGHTEN
    };
    
    // CALLOUT is a point label lifted away from its anchor in SCREEN space and joined back to it
    // by a leader line: the culler moves it until it is free instead of hiding it, so a dense set
    // of point features (summits in a panorama) is read as a stack of named lines rather than as
    // whichever few labels happened to win.
    enum class LabelOrientation {
        BILLBOARD_2D, BILLBOARD_3D, LINE_BILLBOARD_3D, POINT, LINE, CALLOUT
    };

    enum class RasterFilterMode {
        NONE, NEAREST, BILINEAR, BICUBIC
    };

    enum class LineJoinMode {
        NONE, BEVEL, MITER, ROUND
    };

    enum class LineCapMode {
        NONE, SQUARE, ROUND
    };

    struct PointStyle final {
        CompOp compOp;
        ColorFunction colorFunc;
        FloatFunction sizeFunc;
        std::shared_ptr<const BitmapImage> image;
        std::optional<Transform> transform;

        explicit PointStyle(CompOp compOp, ColorFunction colorFunc, FloatFunction sizeFunc, std::shared_ptr<const BitmapImage> image, const std::optional<Transform>& transform) : compOp(compOp), colorFunc(std::move(colorFunc)), sizeFunc(std::move(sizeFunc)), image(std::move(image)), transform(transform) { }
    };

    struct TextStyle final {
        CompOp compOp;
        ColorFunction colorFunc;
        FloatFunction sizeFunc;
        ColorFunction haloColorFunc;
        FloatFunction haloRadiusFunc;
        float angle;
        float backgroundScale;
        cglib::vec2<float> backgroundOffset;
        std::shared_ptr<const BitmapImage> backgroundImage;

        explicit TextStyle(CompOp compOp, ColorFunction colorFunc, FloatFunction sizeFunc, ColorFunction haloColorFunc, FloatFunction haloRadiusFunc, float angle, float backgroundScale, const cglib::vec2<float>& backgroundOffset, std::shared_ptr<const BitmapImage> backgroundImage) : compOp(compOp), colorFunc(std::move(colorFunc)), sizeFunc(std::move(sizeFunc)), haloColorFunc(std::move(haloColorFunc)), haloRadiusFunc(std::move(haloRadiusFunc)), angle(angle), backgroundScale(backgroundScale), backgroundOffset(backgroundOffset), backgroundImage(std::move(backgroundImage)) { }
    };

    struct LineStyle final {
        CompOp compOp;
        LineJoinMode joinMode;
        LineCapMode capMode;
        ColorFunction colorFunc;
        FloatFunction widthFunc;
        FloatFunction offsetFunc;
        float splitDotLimit;
        float miterDotLimit;
        std::shared_ptr<const BitmapPattern> strokePattern;
        std::optional<Transform> transform;

        explicit LineStyle(CompOp compOp, LineJoinMode joinMode, LineCapMode capMode, ColorFunction colorFunc, FloatFunction widthFunc, FloatFunction offsetFunc, float splitDotLimit, float miterDotLimit, std::shared_ptr<const BitmapPattern> strokePattern, const std::optional<Transform>& transform) : compOp(compOp), joinMode(joinMode), capMode(capMode), colorFunc(std::move(colorFunc)), widthFunc(std::move(widthFunc)), offsetFunc(std::move(offsetFunc)), splitDotLimit(splitDotLimit), miterDotLimit(miterDotLimit), strokePattern(std::move(strokePattern)), transform(transform) { }
    };

    struct PolygonStyle final {
        CompOp compOp;
        ColorFunction colorFunc;
        std::shared_ptr<const BitmapPattern> pattern;
        std::optional<Transform> transform;

        explicit PolygonStyle(CompOp compOp, ColorFunction colorFunc, std::shared_ptr<const BitmapPattern> pattern, const std::optional<Transform>& transform) : compOp(compOp), colorFunc(std::move(colorFunc)), pattern(std::move(pattern)), transform(transform) { }
    };

    struct Polygon3DStyle final {
        ColorFunction colorFunc;
        std::optional<Transform> transform;

        explicit Polygon3DStyle(ColorFunction colorFunc, const std::optional<Transform>& transform) : colorFunc(std::move(colorFunc)), transform(transform) { }
    };

    struct PointLabelStyle final {
        LabelOrientation orientation;
        ColorFunction colorFunc;
        FloatFunction sizeFunc;
        bool autoflip;
        std::shared_ptr<const BitmapImage> image;
        std::optional<Transform> transform;
        float maxDistance; // meters from the camera beyond which the label is not placed; 0 = no limit

        explicit PointLabelStyle(LabelOrientation orientation, ColorFunction colorFunc, FloatFunction sizeFunc, bool autoflip, std::shared_ptr<const BitmapImage> image, const std::optional<Transform>& transform, float maxDistance = 0.0f) : orientation(orientation), colorFunc(std::move(colorFunc)), sizeFunc(std::move(sizeFunc)), autoflip(autoflip), image(std::move(image)), transform(transform), maxDistance(maxDistance) { }
    };

    struct TextLabelStyle final {
        LabelOrientation orientation;
        ColorFunction colorFunc;
        FloatFunction sizeFunc;
        ColorFunction haloColorFunc;
        FloatFunction haloRadiusFunc;
        bool autoflip;
        float angle;
        float backgroundScale;
        cglib::vec2<float> backgroundOffset;
        std::shared_ptr<const BitmapImage> backgroundImage;
        float maxDistance; // meters from the camera beyond which the label is not placed; 0 = no limit
        // CALLOUT orientation only, all in screen pixels except the anchor:
        float calloutScreenAnchor; // where the label band sits, as a fraction of the screen height from the top; < 0 stacks it from its own anchor instead
        float calloutOffset;       // minimum distance the label is lifted above its anchor
        float calloutStep;         // how much further up the next stacking row is
        int calloutMaxRows;        // how many rows may be tried before the label is hidden
        float calloutLineWidth;    // leader line width, 0 draws no line
        // A filled plate behind the text, sized to it. Any orientation, not just CALLOUT - a
        // classic map label reads over busy ground with one too. Alpha 0 draws nothing.
        Color backgroundColor;
        float backgroundRadius;    // corner radius, screen pixels
        cglib::vec2<float> backgroundPadding; // around the text, screen pixels

        explicit TextLabelStyle(LabelOrientation orientation, ColorFunction colorFunc, FloatFunction sizeFunc, ColorFunction haloColorFunc, FloatFunction haloRadiusFunc, bool autoflip, float angle, float backgroundScale, const cglib::vec2<float>& backgroundOffset, std::shared_ptr<const BitmapImage> backgroundImage, float maxDistance = 0.0f, float calloutScreenAnchor = -1.0f, float calloutOffset = 0.0f, float calloutStep = 0.0f, int calloutMaxRows = 8, float calloutLineWidth = 1.0f, const Color& backgroundColor = Color(), float backgroundRadius = 0.0f, const cglib::vec2<float>& backgroundPadding = cglib::vec2<float>(0, 0)) : orientation(orientation), colorFunc(std::move(colorFunc)), sizeFunc(std::move(sizeFunc)), haloColorFunc(std::move(haloColorFunc)), haloRadiusFunc(std::move(haloRadiusFunc)), autoflip(autoflip), angle(angle), backgroundScale(backgroundScale), backgroundOffset(backgroundOffset), backgroundImage(std::move(backgroundImage)), maxDistance(maxDistance), calloutScreenAnchor(calloutScreenAnchor), calloutOffset(calloutOffset), calloutStep(calloutStep), calloutMaxRows(calloutMaxRows), calloutLineWidth(calloutLineWidth), backgroundColor(backgroundColor), backgroundRadius(backgroundRadius), backgroundPadding(backgroundPadding) { }
    };
}

#endif
