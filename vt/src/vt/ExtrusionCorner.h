/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _MASSIF_VT_EXTRUSIONCORNER_H_
#define _MASSIF_VT_EXTRUSIONCORNER_H_

#include <algorithm>
#include <cmath>

#include <cglib/vec.h>

namespace massif::vt {
    // The outward normal of a footprint edge, before any winding correction.
    inline cglib::vec2<float> extrusionEdgeNormal(const cglib::vec2<float>& a, const cglib::vec2<float>& b) {
        cglib::vec2<float> t = cglib::unit(b - a);
        return cglib::vec2<float>(t(1), -t(0));
    }

    // How far a wall stops short of a footprint corner so a chamfer can round the vertical edge
    // there: mapbox's `radius * tan(halfAngle)` (fill_extrusion_bucket's _getRoundedEdgeOffset),
    // capped at a third of either edge so the two corners of a short wall cannot cross. `radius`
    // is tile-local, as the result is. 0 where no chamfer fits - a collinear vertex, a spike, or
    // two edges that double back on each other.
    inline float extrusionCornerCutback(const cglib::vec2<float>& prev, const cglib::vec2<float>& p, const cglib::vec2<float>& next, float radius) {
        cglib::vec2<float> na = extrusionEdgeNormal(prev, p);
        cglib::vec2<float> nb = extrusionEdgeNormal(p, next);
        cglib::vec2<float> bisector = na + nb;
        float bisectorLen = cglib::length(bisector);
        if (!(bisectorLen > 0.0f)) {
            return 0.0f;
        }
        float cosHalfAngle = cglib::dot_product(na, bisector * (1.0f / bisectorLen));
        if (!(cosHalfAngle > 0.0f)) {
            return 0.0f;
        }
        float sinHalfAngle = std::sqrt(std::max(0.0f, 1.0f - cosHalfAngle * cosHalfAngle));
        float thirdEdge = std::min(cglib::length(p - prev), cglib::length(next - p)) * (1.0f / 3.0f);
        return std::max(0.0f, std::min(thirdEdge, radius * sinHalfAngle / cosHalfAngle));
    }
}

#endif
