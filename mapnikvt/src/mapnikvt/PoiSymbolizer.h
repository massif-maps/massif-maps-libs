/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _CARTO_MAPNIKVT_POISYMBOLIZER_H_
#define _CARTO_MAPNIKVT_POISYMBOLIZER_H_

#include "Symbolizer.h"
#include "FontSet.h"
#include "FunctionBuilder.h"

#include <vector>
#include <string>

namespace carto::mvt {
    class PoiSymbolizer : public Symbolizer {
    public:
        explicit PoiSymbolizer(std::vector<std::shared_ptr<FontSet>> fontSets, std::shared_ptr<Logger> logger) :
            Symbolizer(std::move(logger)), _fontSets(std::move(fontSets))
        {
            // Text label properties
            bindProperty("name",                  &_name);
            bindProperty("feature-id",            &_featureId);
            bindProperty("text-transform",        &_textTransform);
            bindProperty("face-name",             &_faceName);
            bindProperty("fontset-name",          &_fontSetName);
            bindProperty("placement",             &_placement);
            bindProperty("size",                  &_size);
            bindProperty("fill",                  &_fill);
            bindProperty("opacity",               &_opacity);
            bindProperty("halo-fill",             &_haloFill);
            bindProperty("halo-opacity",          &_haloOpacity);
            bindProperty("halo-radius",           &_haloRadius);
            bindProperty("wrap-character",        &_wrapCharacter);
            bindProperty("wrap-width",            &_wrapWidth);
            bindProperty("wrap-before",           &_wrapBefore);
            bindProperty("character-spacing",     &_characterSpacing);
            bindProperty("line-spacing",          &_lineSpacing);
            bindProperty("horizontal-alignment",  &_horizontalAlignment);
            bindProperty("vertical-alignment",    &_verticalAlignment);
            bindProperty("spacing",               &_spacing);
            bindProperty("minimum-distance",      &_minimumDistance);
            bindProperty("placement-priority",    &_placementPriority);
            bindProperty("allow-overlap",         &_allowOverlap);
            bindProperty("allow-overlap-same-feature-id", &_allowOverlapSameFeatureId);
            bindProperty("clip",                  &_clip);

            // Icon (bitmap or text glyph)
            bindProperty("poi-icon-file",             &_iconFile);
            bindProperty("poi-icon-name",             &_iconName);
            bindProperty("poi-icon-face-name",        &_iconFaceName);
            bindProperty("poi-icon-fontset-name",     &_iconFontSetName);
            bindProperty("poi-icon-size",             &_iconSize);
            bindProperty("poi-icon-fill",             &_iconFill);
            bindProperty("poi-icon-opacity",          &_iconOpacity);
            bindProperty("poi-icon-halo-fill",        &_iconHaloFill);
            bindProperty("poi-icon-halo-opacity",     &_iconHaloOpacity);
            bindProperty("poi-icon-halo-radius",      &_iconHaloRadius);

            // Variable anchor + hide-text control
            bindProperty("poi-variable-anchor",       &_variableAnchor);
            bindProperty("poi-text-margin",           &_textMargin);
            bindProperty("poi-can-hide-text",         &_canHideText);
        }

        virtual FeatureProcessor createFeatureProcessor(const ExpressionContext& exprContext, const SymbolizerContext& symbolizerContext) const override;

    protected:
        static constexpr float IMAGE_UPSAMPLING_SCALE = 2.5f;

        std::shared_ptr<const vt::Font> getTextFont(const SymbolizerContext& symbolizerContext, const ExpressionContext& exprContext) const;
        std::shared_ptr<const vt::Font> getIconFont(const SymbolizerContext& symbolizerContext, const ExpressionContext& exprContext) const;
        std::string getTransformedText(const ExpressionContext& exprContext) const;

        static std::vector<std::string> parseAnchorList(const std::string& anchorStr);
        static vt::TextFormatter::Options makeAnchorOptions(const std::string& anchor, float iconHalfW, float iconHalfH, float margin, float fontScale, const vt::TextFormatter::Options& baseOptions);
        static std::shared_ptr<vt::BitmapImage> tintBitmapImage(const std::shared_ptr<const vt::BitmapImage>& image, const vt::Color& tintColor);

        const std::vector<std::shared_ptr<FontSet>> _fontSets;

        // Text
        StringProperty _name;
        ValueProperty _featureId;
        TextTransformProperty _textTransform = TextTransformProperty("none");
        StringProperty _faceName;
        StringProperty _fontSetName;
        LabelOrientationProperty _placement = LabelOrientationProperty("point");
        FloatFunctionProperty _size = FloatFunctionProperty(10.0f);
        ColorFunctionProperty _fill = ColorFunctionProperty("#000000");
        FloatFunctionProperty _opacity = FloatFunctionProperty(1.0f);
        ColorFunctionProperty _haloFill = ColorFunctionProperty("#ffffff");
        FloatFunctionProperty _haloOpacity = FloatFunctionProperty(1.0f);
        FloatFunctionProperty _haloRadius = FloatFunctionProperty(0.0f);
        StringProperty _wrapCharacter = StringProperty(" ");
        FloatProperty _wrapWidth = FloatProperty(0.0f);
        BoolProperty _wrapBefore = BoolProperty(false);
        FloatProperty _characterSpacing = FloatProperty(0.0f);
        FloatProperty _lineSpacing = FloatProperty(0.0f);
        HorizontalAlignmentProperty _horizontalAlignment = HorizontalAlignmentProperty("auto");
        VerticalAlignmentProperty _verticalAlignment = VerticalAlignmentProperty("auto");
        FloatProperty _spacing = FloatProperty(0.0f);
        FloatProperty _minimumDistance = FloatProperty(0.0f);
        FloatProperty _placementPriority = FloatProperty(0.0f);
        BoolProperty _allowOverlap = BoolProperty(false);
        BoolProperty _allowOverlapSameFeatureId = BoolProperty(false);
        BoolProperty _clip = BoolProperty(false);

        // Icon
        StringProperty _iconFile;
        StringProperty _iconName;
        StringProperty _iconFaceName;
        StringProperty _iconFontSetName;
        FloatFunctionProperty _iconSize = FloatFunctionProperty(16.0f);
        ColorFunctionProperty _iconFill = ColorFunctionProperty("#ffffff");
        FloatFunctionProperty _iconOpacity = FloatFunctionProperty(1.0f);
        ColorFunctionProperty _iconHaloFill = ColorFunctionProperty("#ffffff");
        FloatFunctionProperty _iconHaloOpacity = FloatFunctionProperty(1.0f);
        FloatFunctionProperty _iconHaloRadius = FloatFunctionProperty(0.0f);

        // Placement
        StringProperty _variableAnchor = StringProperty("bottom");
        FloatProperty _textMargin = FloatProperty(4.0f);
        BoolProperty _canHideText = BoolProperty(false);

        ColorFunctionBuilder _fillFuncBuilder;
        FloatFunctionBuilder _sizeFuncBuilder;
        ColorFunctionBuilder _haloFillFuncBuilder;
        FloatFunctionBuilder _haloRadiusFuncBuilder;

        ColorFunctionBuilder _iconFillFuncBuilder;
        FloatFunctionBuilder _iconSizeFuncBuilder;
        ColorFunctionBuilder _iconHaloFillFuncBuilder;
        FloatFunctionBuilder _iconHaloRadiusFuncBuilder;
    };
}

#endif
