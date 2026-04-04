/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _CARTO_MAPNIKVT_CLUSTERSYMBOLIZER_H_
#define _CARTO_MAPNIKVT_CLUSTERSYMBOLIZER_H_

#include "TextSymbolizer.h"

namespace carto::mvt {
    class ClusterSymbolizer : public TextSymbolizer {
    public:
        explicit ClusterSymbolizer(const Expression& text, std::vector<std::shared_ptr<FontSet>> fontSets, std::shared_ptr<Logger> logger) : TextSymbolizer(text, std::move(fontSets), std::move(logger)) {
            bindProperty("cluster-file", &_clusterFile);
            bindProperty("cluster-dx", &_clusterDx);
            bindProperty("cluster-dy", &_clusterDy);
            bindProperty("cluster-distance", &_clusterDistance);
            bindProperty("unlock-image", &_unlockImage);
            bindProperty("cluster-allow-clustering", &_allowClustering);
        }

        virtual FeatureProcessor createFeatureProcessor(const ExpressionContext& exprContext, const SymbolizerContext& symbolizerContext, const std::shared_ptr<const Rule>& rule = nullptr) const override;

    protected:
        static constexpr float IMAGE_UPSAMPLING_SCALE = 2.5f;

        StringProperty _clusterFile;
        BoolProperty _unlockImage = BoolProperty(false);
        FloatProperty _clusterDx = FloatProperty(0.0f);
        FloatProperty _clusterDy = FloatProperty(0.0f);
        FloatProperty _clusterDistance = FloatProperty(0.0f);
        BoolProperty _allowClustering = BoolProperty(true);
    };
}

#endif
