/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _CARTO_VT_FONT_H_
#define _CARTO_VT_FONT_H_

#include "Bitmap.h"
#include "GlyphMap.h"

#include <memory>
#include <string>
#include <cstdint>

#include <cglib/vec.h>

namespace carto::vt {
    inline constexpr int GLYPH_RENDER_SIZE = 27;
    // The SDF spread FreeType renders, and it must MATCH the range the field is encoded over
    // (BITMAP_SDF_SCALE): with a smaller spread the distance field is truncated mid-gradient at the
    // glyph bitmap's edge - the outermost texel lands at ~63/255 instead of 0 - so the quad's border
    // renders as a grey fringe hanging off every letter. Tangram ties the two together the same way
    // (core/src/text/fontContext.cpp: m_sdfRadius is the encode range, the atlas padding AND the
    // maximum stroke width).
    inline constexpr int GLYPH_RENDER_SPREAD = 8; // NOTE: keep it equal to BITMAP_SDF_SCALE

    // How much the sampled texture value (0..1) changes over one texel of signed distance:
    // FreeType spreads +-GLYPH_RENDER_SPREAD texels over +-127, and FontManager rescales that by
    // 4 / BITMAP_SDF_SCALE when it encodes the glyph (see addFreeTypeGlyph). The renderer needs it
    // to size the antialias ramp - one screen pixel of distance is this times the number of glyph
    // texels a screen pixel covers.
    inline constexpr float GLYPH_SDF_UNIT = 127.0f * (4.0f / BITMAP_SDF_SCALE) / GLYPH_RENDER_SPREAD / 255.0f;

    class Font {
    public:
        using CodePoint = unsigned int;

        enum : CodePoint {
            NULL_CODEPOINT  = 0x00000000U,
            SPACE_CODEPOINT = 0xffff0000U,
            CR_CODEPOINT    = 0xffff0001U
        };

        struct Metrics {
            float ascent;
            float descent;
            float height;

            explicit Metrics(float ascent, float descent, float height) : ascent(ascent), descent(descent), height(height) { }
        };

        struct Glyph {
            std::uint32_t utf32Char;
            CodePoint codePoint;
            GlyphMap::Glyph baseGlyph;
            cglib::vec2<float> size;
            cglib::vec2<float> offset;
            cglib::vec2<float> advance;

            explicit Glyph(std::uint32_t utf32Char, CodePoint codePoint, const GlyphMap::Glyph& baseGlyph, const cglib::vec2<float>& size, const cglib::vec2<float>& offset, const cglib::vec2<float>& advance) : utf32Char(utf32Char), codePoint(codePoint), baseGlyph(baseGlyph), size(size), offset(offset), advance(advance) { }
        };

        virtual ~Font() = default;

        virtual Metrics getMetrics(float size) const = 0;
        virtual std::vector<Glyph> shapeGlyphs(const std::uint32_t* utf32Text, std::size_t len, float size, bool rtl) const = 0;
        virtual std::shared_ptr<GlyphMap> getGlyphMap() const = 0;
        virtual int getGlyphRenderSize() const = 0;
    };
}

#endif
