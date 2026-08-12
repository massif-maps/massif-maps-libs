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
#include "LayerConfigSymbolizer.h"
#include "ExpressionContext.h"
#include "ExpressionUtils.h"
#include "PredicateUtils.h"
#include "ValueConverter.h"

#include <algorithm>
#include <set>
#include <cmath>

namespace carto::mvt {
    namespace {
        // The fields a contour feature carries that a filter may read. 'div' is the elevation
        // class, which the shader knows; 'stub' marks the short label carriers, and a line rule
        // that excludes them ([stub=0]) is answered here with a real line, since the bands ARE
        // the lines and no stub is ever painted. Anything else is a feature the shader cannot see.
        const std::string DIV_FIELD = "div";
        const std::string STUB_FIELD = "stub";

        // Properties a line symbolizer may set for the rule to stay a plain elevation band. The
        // joins and caps are in the list because a contour band has neither - a fragment either
        // is within the band or is not - so setting them changes nothing the shader would miss.
        bool isShaderCapableLineProperty(const std::string& name) {
            return name == "stroke" || name == "stroke-width" || name == "stroke-opacity" ||
                   name == "stroke-linejoin" || name == "stroke-linecap";
        }

        // A variable that is a FEATURE field, as opposed to the zoom, a view-state value or a nuti
        // parameter - those the resolver evaluates itself, so they are no obstacle. An empty name
        // is a computed one ('[' + field + ']'), which could be any field: not expressible.
        bool isFeatureField(const std::string& name) {
            return !ExpressionContext::isViewStateVariable(name) && !ExpressionContext::isNutiVariable(name) &&
                   !ExpressionContext::isMapnikVariable(name) && !ExpressionContext::isZoomVariable(name);
        }

        // The feature fields the symbolizer's own properties read. Rule::getReferencedSymbolizerFields
        // answers for the WHOLE rule, and a contour rule usually carries the text symbolizer beside
        // the line one - so it reports 'ele' from 'text-name: [ele]+" m"' and would reject a line
        // that reads nothing at all.
        std::set<std::string> referencedFields(const Symbolizer& symbolizer) {
            struct FieldExtractor {
                explicit FieldExtractor(std::set<std::string>& fields) : _fields(fields) { }

                void operator() (const std::shared_ptr<VariableExpression>& varExpr) {
                    if (auto val = std::get_if<Value>(&varExpr->getVariableExpression())) {
                        _fields.insert(ValueConverter<std::string>::convert(*val));
                    } else {
                        _fields.insert(std::string()); // a computed name: any field may be read
                    }
                }

            private:
                std::set<std::string>& _fields;
            };

            std::set<std::string> fields;
            for (const std::string& name : symbolizer.getPropertyNames()) {
                if (const Property* property = symbolizer.getProperty(name)) {
                    std::visit(ExpressionVariableVisitor(FieldExtractor(fields)), property->getExpression());
                }
            }
            return fields;
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
                    if (std::dynamic_pointer_cast<const LayerConfigSymbolizer>(symbolizer)) {
                        continue; // configures the SOURCE (interval, resolution), draws nothing
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
                    if (isFeatureField(field) && field != DIV_FIELD && field != STUB_FIELD) {
                        result.rejectReason = "a line rule filters on '" + field + "'";
                        return result;
                    }
                }
                for (const std::string& field : referencedFields(*lineSymbolizer)) {
                    if (isFeatureField(field) && field != DIV_FIELD && field != STUB_FIELD) {
                        result.rejectReason = "a line property reads the feature field '" + field + "'";
                        return result;
                    }
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
                                                                 { DIV_FIELD, Value(static_cast<long long>(divisor)) },
                                                                 { STUB_FIELD, Value(static_cast<long long>(0)) }
                                                             });
            ExpressionContext featureContext = exprContext;
            featureContext.setFeatureData(featureData);
            PredicateEvaluator featurePredEvaluator(featureContext, &viewState);

            // Every matching rule paints, in order, and a rule whose width or opacity has ramped
            // to zero paints nothing - so the class is the LAST match that would actually draw a
            // line, not simply the last match. A style's base rule ('line-width: 0' with the real
            // widths in nested [div>=N] blocks) is exactly this case: it matches every divisor and
            // must not blank the class the nested rule sets.
            ContourLineClass contourClass;
            contourClass.divisor = divisor;
            bool matched = false;
            for (const auto& lineRule : lineRules) {
                if (const std::shared_ptr<const Filter>& filter = lineRule.first->getFilter()) {
                    if (filter->getType() == Filter::Type::FILTER && filter->getPredicate()) {
                        if (!std::visit(featurePredEvaluator, *filter->getPredicate())) {
                            continue;
                        }
                    }
                }
                const LineSymbolizer& lineSymbolizer = *lineRule.second;
                const auto* stroke = dynamic_cast<const ColorFunctionProperty*>(lineSymbolizer.getProperty("stroke"));
                const auto* strokeWidth = dynamic_cast<const FloatFunctionProperty*>(lineSymbolizer.getProperty("stroke-width"));
                const auto* strokeOpacity = dynamic_cast<const FloatFunctionProperty*>(lineSymbolizer.getProperty("stroke-opacity"));
                if (!stroke || !strokeWidth || !strokeOpacity) {
                    result.shaderCapable = false;
                    result.rejectReason = "the line symbolizer is missing a stroke property";
                    return result;
                }
                float width = strokeWidth->getFunction(featureContext)(viewState);
                float opacity = strokeOpacity->getFunction(featureContext)(viewState);
                vt::Color color = stroke->getFunction(featureContext)(viewState);
                if (!(width > 0.0f) || !(opacity > 0.0f) || !(color[3] > 0.0f)) {
                    continue; // ramped to nothing: it draws no line, so it takes no class
                }
                contourClass.width = width;
                contourClass.color = vt::Color(color[0] * opacity, color[1] * opacity, color[2] * opacity, color[3] * opacity);
                matched = true;
            }
            if (!matched) {
                continue;
            }
            result.classes.push_back(contourClass);
            result.visible = true;
        }
        return result;
    }
}
