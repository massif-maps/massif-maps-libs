/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _MASSIF_VT_SPANGEOMETRY_H_
#define _MASSIF_VT_SPANGEOMETRY_H_

#include <algorithm>
#include <cmath>

#include <cglib/vec.h>

namespace massif::vt {
    /**
     * The pure geometry behind LineElevationMode::SPAN - a bridge deck or a tunnel bore laid
     * straight between its two portals instead of following the ground.
     *
     * Header-only and free of any renderer state so it can be tested on the host: every one of
     * these is silent when wrong (a deck sags, a label sits on the ground) rather than failing.
     */
    struct SpanGeometry final {
        /** How far inside the tile an end must be to count as the feature's own, in tile units. */
        static constexpr float TILE_CLIP_MARGIN = 0.002f;
        /** The narrowest a deck is ever matched at, in normalized world units (25 m). */
        static constexpr double MIN_MATCH_WIDTH = 25.0 / 40075017.0;
        /** ...growing with the span, because a long deck CURVES away from its own straight chord. */
        static constexpr double MATCH_CURVE_FRACTION = 0.02;
        /** cos of the angle two pieces may differ by and still be one structure (~25 degrees). */
        static constexpr double MIN_PARALLEL = 0.9;

        /**
         * Whether an end is the FEATURE's own or just where the tile cut it. Tested against the
         * tile, not the clip box: the source clips at its own buffer (mapbox: 1/64), so every cut
         * end lands well inside our 1/8 box and would read as a portal. The same point is inside
         * the NEIGHBOURING tile's copy, which is where its portal is seen.
         */
        static bool isPortal(const cglib::vec2<float>& p, float margin = TILE_CLIP_MARGIN) {
            return p(0) > margin && p(0) < 1.0f - margin
                && p(1) > margin && p(1) < 1.0f - margin;
        }

        /** Where a point falls along the chord, clamped to it: 0 at one portal, 1 at the other. */
        static double chordParam(const cglib::vec2<double>& pos, const cglib::vec2<double>& portal0, const cglib::vec2<double>& portal1) {
            cglib::vec2<double> chord = portal1 - portal0;
            double length2 = cglib::dot_product(chord, chord);
            if (length2 <= 0) {
                return 0;
            }
            return std::max(0.0, std::min(1.0, cglib::dot_product(pos - portal0, chord) / length2));
        }

        /** The deck height at that point - the whole purpose: straight, whatever the DEM does. */
        static double chordHeight(double height0, double height1, double t) {
            return height0 + (height1 - height0) * t;
        }

        /**
         * How far off the chord something may sit and still belong to it. A fixed radius is wrong:
         * Millau's deck curves on a ~20 km radius, putting its middle some 36 m off its own chord,
         * so a 25 m test missed exactly the labels that stand on the bridge.
         */
        static double matchAllowance(double chordLength) {
            return std::max(MIN_MATCH_WIDTH, chordLength * MATCH_CURVE_FRACTION);
        }

        /** Whether a point stands on the chord - within the allowance, and between the portals. */
        static bool isOnChord(const cglib::vec2<double>& pos, const cglib::vec2<double>& portal0, const cglib::vec2<double>& portal1) {
            cglib::vec2<double> chord = portal1 - portal0;
            double length2 = cglib::dot_product(chord, chord);
            if (length2 <= 0) {
                return false;
            }
            double t = cglib::dot_product(pos - portal0, chord) / length2;
            if (t < 0 || t > 1) {
                return false; // past an abutment: back on the ground
            }
            double allowance = matchAllowance(std::sqrt(length2));
            return cglib::norm(pos - (portal0 + chord * t)) <= allowance * allowance;
        }

        /**
         * Whether two pieces are the same structure. Only an end the tile CUT can continue into
         * another piece - a real portal ends the run - and the source's buffer makes neighbouring
         * copies overlap rather than touch, so this is proximity, not equality. The direction test
         * keeps a crossing structure out of the chain.
         */
        static bool piecesMeet(const cglib::vec2<double>& a0, const cglib::vec2<double>& a1, bool aPortal0, bool aPortal1,
                               const cglib::vec2<double>& b0, const cglib::vec2<double>& b1, bool bPortal0, bool bPortal1,
                               double tolerance2) {
            cglib::vec2<double> da = a1 - a0, db = b1 - b0;
            double lengthA2 = cglib::dot_product(da, da), lengthB2 = cglib::dot_product(db, db);
            if (lengthA2 <= 0 || lengthB2 <= 0) {
                return false;
            }
            double parallel = cglib::dot_product(da, db) / std::sqrt(lengthA2 * lengthB2);
            if (std::abs(parallel) < MIN_PARALLEL) {
                return false;
            }
            return (!aPortal0 && ((!bPortal0 && cglib::norm(a0 - b0) < tolerance2) || (!bPortal1 && cglib::norm(a0 - b1) < tolerance2)))
                || (!aPortal1 && ((!bPortal0 && cglib::norm(a1 - b0) < tolerance2) || (!bPortal1 && cglib::norm(a1 - b1) < tolerance2)));
        }
    };
}

#endif
