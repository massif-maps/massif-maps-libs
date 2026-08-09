/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _CARTO_VT_LABELCULLER_H_
#define _CARTO_VT_LABELCULLER_H_

#include "ViewState.h"
#include "Label.h"

#include <array>
#include <functional>
#include <vector>
#include <list>
#include <unordered_map>
#include <memory>
#include <mutex>

#include <cglib/vec.h>
#include <cglib/mat.h>
#include <cglib/bbox.h>

namespace carto::vt {
    class LabelCuller final {
    public:
        explicit LabelCuller(float scale);

        void setViewState(const ViewState& viewState);
        /**
         * Internal units per meter at the current view, so that a label style's max-distance
         * (which is in meters) can be compared against world-space distances. 0 disables the
         * test - a caller that does not set it gets the previous behaviour exactly.
         */
        void setMetersToInternal(double metersToInternal);
        void reset();
        bool process(const std::vector<std::shared_ptr<Label>>& labelList, std::mutex& labelMutex);

    private:
        static constexpr int GRID_RESOLUTION_X = 16;
        static constexpr int GRID_RESOLUTION_Y = 32;
        static constexpr float EXTRA_LABEL_BUFFER = 1.0f; // extra buffer for the label
        static constexpr float SCREEN_EDGE_MARGIN = 8.0f; // pixels a lifted callout keeps from the top edge


        struct CullRecord {
            cglib::bbox2<float> bounds;
            std::array<cglib::vec2<float>, 4> envelope;
            long long localId = 0;
            bool allowOverlapSameFeatureId = false;
            // Only set in the minimum grid: where the label sits in the placement order, and its
            // priority, so that a label can tell which reservations it must respect.
            int order = -1;
            float priority = 0.0f;

            CullRecord() = default;
        };

        using RecordGrid = std::array<std::array<std::vector<CullRecord>, GRID_RESOLUTION_X>, GRID_RESOLUTION_Y>;

        struct LabelInfo {
            bool valid;
            bool wasVisible;
            float priority;
            int layerIndex;
            float size;
            float opacity;
            std::shared_ptr<Label> label;
            CullRecord cullRecord;
            // One per side the label may take, in preference order; the last one is its smallest
            // (the icon alone for a 'text-optional' shield). A label with one fixed layout has one.
            std::vector<CullRecord> variants;
        };

        cglib::vec2<int> getGridIndex(const cglib::vec2<float>& pos) const;
        void clearGrid();
        void addGridRecord(RecordGrid& grid, const CullRecord& cullRecord) const;
        bool testGridOverlap(const LabelInfo& labelInfo) const;
        // Whether a candidate would take space that a LATER label of the same priority has no way
        // of giving up - its smallest layout. A label that can still shrink has to yield first.
        bool testPeerReservations(const CullRecord& candidate, int order, float priority) const;
        void projectEnvelope(const std::array<cglib::vec3<float>, 4>& worldEnvelope, std::array<cglib::vec2<float>, 4>& envelope) const;
        bool calculateScreenEnvelope(const std::shared_ptr<Label>& label, float size, std::array<cglib::vec2<float>, 4>& envelope) const;
        // A CALLOUT label is lifted away from its anchor until it finds free screen space instead
        // of being hidden. Returns false when it ran out of rows. Updates the label's offset and
        // the cull record in place.
        bool placeCalloutLabel(LabelInfo& labelInfo, const std::function<bool(const LabelInfo&)>& testGroupDistance);
        // A label whose style names several sides (TextLabelStyle::anchors) takes the first free
        // one - tangram's 'do { ... } while (isOccluded() && nextAnchor())' (labelManager.cpp).
        // Returns false when no side is free. Updates the label's variant and the cull record.
        bool placeAnchoredLabel(LabelInfo& labelInfo, int order, const std::function<bool(const LabelInfo&)>& testGroupDistance);

        cglib::mat4x4<float> _localCameraProjMatrix;
        ViewState _viewState;
        double _metersToInternal = 0;
        RecordGrid _recordGrid;
        // Smallest layout of every label of this pass, used only by the rule above.
        RecordGrid _minimumGrid;

        const float _scale;

        mutable std::mutex _mutex;
    };
}

#endif
