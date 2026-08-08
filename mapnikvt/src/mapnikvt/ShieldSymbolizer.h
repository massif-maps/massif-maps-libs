/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _CARTO_MAPNIKVT_SHIELDSYMBOLIZER_H_
#define _CARTO_MAPNIKVT_SHIELDSYMBOLIZER_H_

#include "TextSymbolizer.h"

namespace carto::mvt {
    class ShieldSymbolizer : public TextSymbolizer {
    public:
        explicit ShieldSymbolizer(const Expression& text, std::vector<std::shared_ptr<FontSet>> fontSets, std::shared_ptr<Logger> logger) : TextSymbolizer(text, std::move(fontSets), std::move(logger)) {
            bindProperty("file", &_file);
            bindProperty("shield-dx", &_shieldDx);
            bindProperty("shield-dy", &_shieldDy);
            bindProperty("unlock-image", &_unlockImage);
            bindProperty("anchors", &_anchors);
            bindProperty("text-optional", &_textOptional);
            bindProperty("icon-name", &_iconText);
            bindProperty("icon-face-name", &_iconFaceName);
            bindProperty("icon-size", &_iconSize);
            bindProperty("icon-fill", &_iconFill);
            bindProperty("icon-opacity", &_iconOpacity);
            bindProperty("icon-dx", &_iconDx);
            bindProperty("icon-dy", &_iconDy);
        }

        virtual FeatureProcessor createFeatureProcessor(const ExpressionContext& exprContext, const SymbolizerContext& symbolizerContext) const override;

    protected:
        static constexpr float IMAGE_UPSAMPLING_SCALE = 2.5f;

        // 'right, left, top, bottom' and the four corners, in preference order - the culler takes
        // the first side that is free (see vt::LabelAnchor). Empty is one fixed layout, which is
        // what every shield did before the property existed.
        static std::vector<vt::LabelAnchor> parseAnchors(const std::string& anchors);
        // The icon run: the glyphs of 'icon-name' shaped from 'icon-face-name', scaled, centred on
        // the anchor and marked as the icon run so that they keep their place when the text moves.
        // The face is resolved as a FALLBACK of the label font, which is what puts its glyphs in
        // the label's own atlas (FontManagerFont::shapeGlyphs) - a font of its own has an atlas of
        // its own, and one label can only be drawn from one.
        std::vector<vt::Font::Glyph> buildIconGlyphs(const std::shared_ptr<const vt::Font>& font, const SymbolizerContext& symbolizerContext, const ExpressionContext& exprContext, float fontSize) const;

        StringProperty _file;
        BoolProperty _unlockImage = BoolProperty(false);
        FloatProperty _shieldDx = FloatProperty(0.0f);
        FloatProperty _shieldDy = FloatProperty(0.0f);
        StringProperty _anchors = StringProperty("");
        BoolProperty _textOptional = BoolProperty(false);
        StringProperty _iconText = StringProperty("");
        StringProperty _iconFaceName = StringProperty("");
        FloatProperty _iconSize = FloatProperty(0.0f); // pixels; 0 = the label's own size
        ColorFunctionProperty _iconFill = ColorFunctionProperty("#000000");
        FloatFunctionProperty _iconOpacity = FloatFunctionProperty(1.0f);
        FloatProperty _iconDx = FloatProperty(0.0f);
        FloatProperty _iconDy = FloatProperty(0.0f);

        ColorFunctionBuilder _iconFillFuncBuilder;
    };
}

#endif
