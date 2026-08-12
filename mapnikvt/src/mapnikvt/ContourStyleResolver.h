/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _CARTO_MAPNIKVT_CONTOURSTYLERESOLVER_H_
#define _CARTO_MAPNIKVT_CONTOURSTYLERESOLVER_H_

#include "NutiParameterStore.h"

#include <memory>
#include <string>
#include <vector>

#include <vt/Color.h>

namespace carto::mvt {
    class Map;

    /**
     * One elevation class of a contour style: the lines whose height is a multiple of 'divisor',
     * drawn with this colour and width. It is what a '#contour [div=N] { line-color: ...; }' rule
     * says, evaluated at one view zoom and one nuti parameter state.
     */
    struct ContourLineClass {
        float divisor = 0.0f;   // metres between the lines of this class
        vt::Color color;        // stroke colour, opacity folded in
        float width = 1.0f;     // stroke width in screen pixels
    };

    /**
     * What a '#contour' layer's LINE rules amount to, and whether they can be drawn as elevation
     * bands in the terrain fragment shader instead of as traced geometry.
     *
     * The lines are a pure function of the elevation, so a style that only asks for a colour, a
     * width and an opacity per 'div' class describes exactly what a fragment shader can paint -
     * and drawing them there costs nothing, where the traced tile set costs a whole second set of
     * tiles (see docs/rendering/07-hillshade-contours.md). Anything the shader cannot reproduce -
     * a dash pattern, an offset, an arrow, a filter on another field, a width that reads the
     * feature - makes 'shaderCapable' false, and the caller keeps the traced path.
     *
     * TEXT rules are ignored here: labels stay real features whatever happens to the lines.
     */
    struct ResolvedContourStyle {
        bool shaderCapable = false;
        bool visible = false;                    // at least one class draws at this zoom
        std::vector<ContourLineClass> classes;   // finest divisor first, coarsest last
        std::string rejectReason;                // why shaderCapable is false, for one log line
    };

    /**
     * Evaluate the line rules of the named layer for the given contour divisors, without decoding
     * a tile. Rule zoom ranges and filter predicates (zoom, nuti::, [div=N]) are honoured, and the
     * properties are evaluated at 'viewZoom' with the given parameter store - so a style that
     * ramps its width with the zoom or takes its opacity from a nuti parameter resolves here
     * exactly as it would in a decoded tile.
     *
     * @param map        The compiled style map.
     * @param layerName  The style layer name, e.g. "contour".
     * @param viewZoom   The fractional view zoom.
     * @param divisors   The divisors the contour source emits at this zoom, finest first. A
     *                   divisor with no matching rule simply produces no class.
     * @param nutiParameterStore The nuti parameter store (may be null).
     */
    ResolvedContourStyle resolveContourStyle(const Map& map,
                                             const std::string& layerName,
                                             float viewZoom,
                                             const std::vector<float>& divisors,
                                             const std::shared_ptr<const NutiParameterStore>& nutiParameterStore);
}

#endif
