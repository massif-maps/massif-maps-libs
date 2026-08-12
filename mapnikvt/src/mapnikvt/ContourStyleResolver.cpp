#include "ContourStyleResolver.h"
#include "Map.h"
#include "Layer.h"
#include "Style.h"
#include "Rule.h"
#include "Filter.h"
#include "Feature.h"
#include "Symbolizer.h"
#include "LineSymbolizer.h"
#include "TextSymbolizer.h"
#include "ExpressionContext.h"
#include "ExpressionUtils.h"
#include "PredicateUtils.h"
#include "ValueConverter.h"

#include <algorithm>

namespace carto::mvt {
    namespace {
        // The field a contour feature carries for its class, and the only one a filter may read:
        // the shader knows the elevation and nothing else about the feature.
        const std::string DIV_FIELD = "div";

        // Properties a line symbolizer may set for the rule to stay a plain elevation band. The
        // joins and caps are in the list because a contour band has neither - a fragment either
        // is within the band or is not - so setting them changes nothing the shader would miss.
        bool isShaderCapableLineProperty(const std::string& name) {
            return name == "stroke" || name == "stroke-width" || name == "stroke-opacity" ||
                   name == "stroke-linejoin" || name == "stroke-linecap";
        }
    }

    ResolvedContourStyle resolveContourStyle(const Map& map, const std::string& layerName, float viewZoom,
                                             const std::vector<float>& divisors,
                                             const std::shared_ptr<const NutiParameterStore>& nutiParameterStore) {
        ResolvedContourStyle result;

        const std::shared_ptr<Layer>& layer = map.getLayer(layerName);
        if (!layer) {
            result.rejectReason = "no '" + layerName + "' layer in the style";
            return result;
        }

        ExpressionContext exprContext;
        exprContext.setAdjustedZoom(static_cast<int>(viewZoom));
        if (nutiParameterStore) {
            exprContext.setNutiParameterStore(nutiParameterStore);
        }
        vt::ViewState viewState;
        viewState.zoom = viewZoom;

        // The line rules of the layer, in style order: a later rule wins, as it does when the
        // features are drawn.
        std::vector<std::pair<std::shared_ptr<const Rule>, std::shared_ptr<const LineSymbolizer>>> lineRules;
        for (const std::string& styleName : layer->getStyleNames()) {
            const std::shared_ptr<Style>& style = map.getStyle(styleName);
            if (!style) {
                continue;
            }
            for (const std::shared_ptr<const Rule>& rule : style->getZoomRules(exprContext.getAdjustedZoom())) {
                std::shared_ptr<const LineSymbolizer> lineSymbolizer;
                for (const std::shared_ptr<const Symbolizer>& symbolizer : rule->getSymbolizers()) {
                    if (std::dynamic_pointer_cast<const TextSymbolizer>(symbolizer)) {
                        continue; // labels stay features whatever happens to the lines
                    }
                    if (auto line = std::dynamic_pointer_cast<const LineSymbolizer>(symbolizer)) {
                        if (lineSymbolizer) {
                            result.rejectReason = "a rule draws two line symbolizers (casing)";
                            return result;
                        }
                        lineSymbolizer = line;
                        continue;
                    }
                    result.rejectReason = "a rule draws something other than a line or a label";
                    return result;
                }
                if (!lineSymbolizer) {
                    continue;
                }

                // The filter may only select the elevation class, and the properties may only
                // read the zoom and the parameters - a shader has no feature to read.
                for (const std::string& field : rule->getReferencedFilterFields()) {
                    if (field != DIV_FIELD) {
                        result.rejectReason = "a line rule filters on '" + field + "'";
                        return result;
                    }
                }
                if (!rule->getReferencedSymbolizerFields().empty()) {
                    result.rejectReason = "a line property reads a feature field";
                    return result;
                }
                for (const std::string& propertyName : lineSymbolizer->getPropertyNames()) {
                    const Property* property = lineSymbolizer->getProperty(propertyName);
                    if (property && property->isDefined() && !isShaderCapableLineProperty(propertyName)) {
                        result.rejectReason = "a line rule sets '" + propertyName + "'";
                        return result;
                    }
                }
                lineRules.emplace_back(rule, lineSymbolizer);
            }
        }

        result.shaderCapable = true;
        if (lineRules.empty()) {
            return result; // expressible, and it draws nothing at this zoom
        }

        // Which rule each divisor lands in, answered the way the decoder answers it: by evaluating
        // the filter against a feature that carries that div. No predicate is taken apart here, so
        // a filter this resolver does not understand cannot be silently mis-read - the fields check
        // above is what guarantees div is all it can be reading.
        PredicateEvaluator predEvaluator(exprContext, &viewState);
        std::vector<float> sortedDivisors = divisors;
        std::sort(sortedDivisors.begin(), sortedDivisors.end());
        for (float divisor : sortedDivisors) {
            if (!(divisor > 0.0f)) {
                continue;
            }
            auto featureData = std::make_shared<FeatureData>(0, FeatureData::GeometryType::LINE_GEOMETRY,
                                                             std::vector<std::pair<std::string, Value>> {
                                                                 { DIV_FIELD, Value(static_cast<long long>(divisor)) }
                                                             });
            ExpressionContext featureContext = exprContext;
            featureContext.setFeatureData(featureData);
            PredicateEvaluator featurePredEvaluator(featureContext, &viewState);

            std::shared_ptr<const LineSymbolizer> match;
            for (const auto& lineRule : lineRules) {
                if (const std::shared_ptr<const Filter>& filter = lineRule.first->getFilter()) {
                    if (filter->getType() == Filter::Type::FILTER && filter->getPredicate()) {
                        if (!std::visit(featurePredEvaluator, *filter->getPredicate())) {
                            continue;
                        }
                    }
                }
                match = lineRule.second; // later rule wins, as in the draw order
            }
            if (!match) {
                continue;
            }

            const auto* stroke = dynamic_cast<const ColorFunctionProperty*>(match->getProperty("stroke"));
            const auto* strokeWidth = dynamic_cast<const FloatFunctionProperty*>(match->getProperty("stroke-width"));
            const auto* strokeOpacity = dynamic_cast<const FloatFunctionProperty*>(match->getProperty("stroke-opacity"));
            if (!stroke || !strokeWidth || !strokeOpacity) {
                result.shaderCapable = false;
                result.rejectReason = "the line symbolizer is missing a stroke property";
                return result;
            }

            ContourLineClass contourClass;
            contourClass.divisor = divisor;
            contourClass.width = strokeWidth->getFunction(featureContext)(viewState);
            float opacity = strokeOpacity->getFunction(featureContext)(viewState);
            vt::Color color = stroke->getFunction(featureContext)(viewState);
            contourClass.color = vt::Color(color[0] * opacity, color[1] * opacity, color[2] * opacity, color[3] * opacity);
            if (contourClass.width > 0.0f && contourClass.color[3] > 0.0f) {
                result.classes.push_back(contourClass);
                result.visible = true;
            }
        }
        return result;
    }
}
