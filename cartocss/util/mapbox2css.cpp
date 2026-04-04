#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <set>

#include <picojson/picojson.h>

class MapboxToCartoCSS {
public:
    MapboxToCartoCSS() = default;

    std::string convert(const std::string& mapboxStyleJson) {
        picojson::value styleValue;
        std::string err = picojson::parse(styleValue, mapboxStyleJson);
        if (!err.empty()) {
            throw std::runtime_error("Failed to parse JSON: " + err);
        }

        if (!styleValue.is<picojson::object>()) {
            throw std::runtime_error("Invalid Mapbox style: root must be an object");
        }

        const picojson::object& styleObj = styleValue.get<picojson::object>();
        
        std::ostringstream css;
        
        // Write header comment
        css << "/* CartoCSS generated from Mapbox GL style */\n\n";

        // Process background layer if present and convert to Map properties
        auto layersIt = styleObj.find("layers");
        if (layersIt != styleObj.end() && layersIt->second.is<picojson::array>()) {
            const picojson::array& layers = layersIt->second.get<picojson::array>();
            
            // First pass: look for background layer
            for (const auto& layerValue : layers) {
                if (!layerValue.is<picojson::object>()) {
                    continue;
                }
                
                const picojson::object& layer = layerValue.get<picojson::object>();
                std::string type = getStringProperty(layer, "type", "");
                
                if (type == "background") {
                    std::string backgroundCSS = convertBackgroundLayer(layer);
                    if (!backgroundCSS.empty()) {
                        css << backgroundCSS << "\n";
                    }
                }
            }
            
            // Second pass: process all other layers
            for (const auto& layerValue : layers) {
                if (!layerValue.is<picojson::object>()) {
                    continue;
                }
                
                const picojson::object& layer = layerValue.get<picojson::object>();
                std::string layerCSS = convertLayer(layer);
                if (!layerCSS.empty()) {
                    css << layerCSS << "\n";
                }
            }
        }

        return css.str();
    }

private:
    std::string convertBackgroundLayer(const picojson::object& layer) {
        std::ostringstream css;
        css << "Map {\n";
        
        // Get paint properties
        auto paintIt = layer.find("paint");
        if (paintIt != layer.end() && paintIt->second.is<picojson::object>()) {
            const picojson::object& paint = paintIt->second.get<picojson::object>();
            
            // Convert background-color
            auto bgColorIt = paint.find("background-color");
            if (bgColorIt != paint.end()) {
                std::string cssValue = convertPropertyValue(bgColorIt->second, "background-color", "background");
                if (!cssValue.empty()) {
                    css << "  background-color: " << cssValue << ";\n";
                }
            }
            
            // Convert background-opacity if present
            auto bgOpacityIt = paint.find("background-opacity");
            if (bgOpacityIt != paint.end()) {
                std::string cssValue = convertPropertyValue(bgOpacityIt->second, "background-opacity", "background");
                if (!cssValue.empty()) {
                    css << "  background-opacity: " << cssValue << ";\n";
                }
            }
        }
        
        css << "}\n";
        return css.str();
    }

    std::string convertLayer(const picojson::object& layer) {
        // Get layer properties
        std::string id = getStringProperty(layer, "id", "");
        std::string type = getStringProperty(layer, "type", "");
        std::string sourceLayer = getStringProperty(layer, "source-layer", "");
        
        // Skip background (handled separately) and raster layers
        if (type == "background" || type == "raster") {
            return "";
        }

        // Skip if no valid id
        if (id.empty()) {
            return "";
        }

        std::ostringstream css;
        
        // Build selector - use source-layer if available, otherwise use id
        std::string selector = "#" + (!sourceLayer.empty() ? sourceLayer : id);
        
        // Get filter and convert to CartoCSS predicate
        std::string filterPredicate = "";
        auto filterIt = layer.find("filter");
        if (filterIt != layer.end()) {
            filterPredicate = convertFilter(filterIt->second);
        }
        
        // Check if filter contains OR logic (multiple rules needed)
        if (filterPredicate.find("|OR|") != std::string::npos) {
            // Split by OR marker and generate multiple rules
            std::ostringstream allRules;
            size_t start = 0;
            size_t end = filterPredicate.find("|OR|");
            
            while (end != std::string::npos || start < filterPredicate.length()) {
                std::string singlePredicate;
                if (end != std::string::npos) {
                    singlePredicate = filterPredicate.substr(start, end - start);
                    start = end + 4; // Length of "|OR|"
                    end = filterPredicate.find("|OR|", start);
                } else {
                    singlePredicate = filterPredicate.substr(start);
                    start = filterPredicate.length();
                }
                
                // Build selector with this single predicate
                std::string singleSelector = selector + singlePredicate + zoomPredicate;
                allRules << singleSelector << " {\n";
                
                // Convert layout properties
                auto layoutIt = layer.find("layout");
                if (layoutIt != layer.end() && layoutIt->second.is<picojson::object>()) {
                    const picojson::object& layout = layoutIt->second.get<picojson::object>();
                    std::string layoutCSS = convertProperties(layout, type, true);
                    if (!layoutCSS.empty()) {
                        allRules << layoutCSS;
                    }
                }
                
                // Convert paint properties
                auto paintIt = layer.find("paint");
                if (paintIt != layer.end() && paintIt->second.is<picojson::object>()) {
                    const picojson::object& paint = paintIt->second.get<picojson::object>();
                    std::string paintCSS = convertProperties(paint, type, false);
                    if (!paintCSS.empty()) {
                        allRules << paintCSS;
                    }
                }
                
                allRules << "}\n\n";
            }
            
            return allRules.str();
        }
        
        // Build complete selector
        double minzoom = getNumberProperty(layer, "minzoom", -1);
        double maxzoom = getNumberProperty(layer, "maxzoom", -1);
        
        std::string zoomPredicate = "";
        if (minzoom >= 0 && maxzoom >= 0) {
            zoomPredicate = "[zoom >= " + std::to_string(static_cast<int>(minzoom)) + 
                           "][zoom < " + std::to_string(static_cast<int>(maxzoom)) + "]";
        } else if (minzoom >= 0) {
            zoomPredicate = "[zoom >= " + std::to_string(static_cast<int>(minzoom)) + "]";
        } else if (maxzoom >= 0) {
            zoomPredicate = "[zoom < " + std::to_string(static_cast<int>(maxzoom)) + "]";
        }
        
        // Build complete selector
        css << selector << filterPredicate << zoomPredicate << " {\n";
        
        // Convert layout properties
        auto layoutIt = layer.find("layout");
        if (layoutIt != layer.end() && layoutIt->second.is<picojson::object>()) {
            const picojson::object& layout = layoutIt->second.get<picojson::object>();
            std::string layoutCSS = convertProperties(layout, type, true);
            if (!layoutCSS.empty()) {
                css << layoutCSS;
            }
        }
        
        // Convert paint properties
        auto paintIt = layer.find("paint");
        if (paintIt != layer.end() && paintIt->second.is<picojson::object>()) {
            const picojson::object& paint = paintIt->second.get<picojson::object>();
            std::string paintCSS = convertProperties(paint, type, false);
            if (!paintCSS.empty()) {
                css << paintCSS;
            }
        }
        
        css << "}\n";
        
        return css.str();
    }

    std::string convertProperties(const picojson::object& props, const std::string& layerType, bool isLayout) {
        std::ostringstream css;
        
        // Check if we have both pattern and opacity for fill/line layers
        bool hasPattern = false;
        bool hasOpacity = false;
        std::string patternPropName = "";
        std::string opacityPropName = "";
        
        if (layerType == "fill") {
            hasPattern = props.find("fill-pattern") != props.end();
            hasOpacity = props.find("fill-opacity") != props.end();
            if (hasPattern) {
                patternPropName = "fill-pattern";
                opacityPropName = "fill-opacity";
            }
        } else if (layerType == "line") {
            hasPattern = props.find("line-pattern") != props.end();
            hasOpacity = props.find("line-opacity") != props.end();
            if (hasPattern) {
                patternPropName = "line-pattern";
                opacityPropName = "line-opacity";
            }
        }
        
        for (const auto& prop : props) {
            std::string propName = prop.first;
            const picojson::value& propValue = prop.second;
            
            // Special handling for opacity when pattern is present
            if (hasPattern && hasOpacity && propName == opacityPropName) {
                // Convert to pattern-opacity instead
                std::string cssProperty = layerType == "fill" ? "polygon-pattern-opacity" : "line-pattern-opacity";
                std::string cssValue = convertPropertyValue(propValue, propName, layerType);
                if (!cssValue.empty()) {
                    css << "  " << cssProperty << ": " << cssValue << ";\n";
                }
                continue;
            }
            
            // Skip regular opacity/fill properties when pattern is present
            if (hasPattern && (propName == "fill-color" || propName == "fill-opacity" && layerType == "fill")) {
                if (propName == "fill-color") continue; // Skip fill-color when using pattern
            }
            
            // Map Mapbox property to CartoCSS property
            std::string cssProperty = mapPropertyName(propName, layerType, isLayout);
            if (cssProperty.empty()) {
                continue; // Property not supported or not mappable
            }
            
            // Convert property value
            std::string cssValue = convertPropertyValue(propValue, propName, layerType);
            if (cssValue.empty()) {
                continue;
            }
            
            css << "  " << cssProperty << ": " << cssValue << ";\n";
        }
        
        return css.str();
    }

    std::string mapPropertyName(const std::string& mapboxProp, const std::string& layerType, bool isLayout) {
        // Mapping table from Mapbox GL properties to CartoCSS properties
        
        if (layerType == "line") {
            // Line layer properties
            if (mapboxProp == "line-color") return "line-color";
            if (mapboxProp == "line-opacity") return "line-opacity";
            if (mapboxProp == "line-width") return "line-width";
            if (mapboxProp == "line-gap-width") return ""; // Not directly supported
            if (mapboxProp == "line-offset") return "line-offset";
            if (mapboxProp == "line-blur") return ""; // Not directly supported
            if (mapboxProp == "line-dasharray") return "line-dasharray";
            if (mapboxProp == "line-pattern") return "line-pattern-file";
            if (mapboxProp == "line-cap") return "line-cap";
            if (mapboxProp == "line-join") return "line-join";
            if (mapboxProp == "line-miter-limit") return "line-miterlimit";
            if (mapboxProp == "line-round-limit") return ""; // Not directly supported
        }
        else if (layerType == "fill") {
            // Fill/polygon layer properties
            if (mapboxProp == "fill-color") return "polygon-fill";
            if (mapboxProp == "fill-opacity") return "polygon-opacity";
            if (mapboxProp == "fill-outline-color") return ""; // Use separate attachment
            if (mapboxProp == "fill-pattern") return "polygon-pattern-file";
            if (mapboxProp == "fill-antialias") return ""; // Not directly supported
            if (mapboxProp == "fill-translate") return ""; // Not directly supported
        }
        else if (layerType == "symbol") {
            // Symbol placement properties
            if (mapboxProp == "symbol-placement") return "text-placement";
            if (mapboxProp == "symbol-spacing") return "text-spacing";
            
            // Text/symbol layer properties
            if (mapboxProp == "text-field") return "text-name";
            if (mapboxProp == "text-font") return "text-face-name";
            if (mapboxProp == "text-size") return "text-size";
            if (mapboxProp == "text-color") return "text-fill";
            if (mapboxProp == "text-opacity") return "text-opacity";
            if (mapboxProp == "text-halo-color") return "text-halo-fill";
            if (mapboxProp == "text-halo-width") return "text-halo-radius";
            if (mapboxProp == "text-halo-blur") return ""; // Not directly supported
            if (mapboxProp == "text-transform") return "text-transform";
            if (mapboxProp == "text-offset") return ""; // Use text-dx/text-dy
            if (mapboxProp == "text-allow-overlap") return "text-allow-overlap";
            if (mapboxProp == "text-ignore-placement") return ""; // Not directly supported
            if (mapboxProp == "text-optional") return ""; // Not directly supported
            if (mapboxProp == "text-rotation-alignment") return ""; // Not directly supported
            if (mapboxProp == "text-pitch-alignment") return ""; // Not directly supported
            if (mapboxProp == "text-anchor") return ""; // Use text-horizontal/vertical-alignment
            if (mapboxProp == "text-max-width") return "text-wrap-width";
            if (mapboxProp == "text-letter-spacing") return "text-character-spacing";
            if (mapboxProp == "text-justify") return "text-horizontal-alignment";
            if (mapboxProp == "text-line-height") return "text-line-spacing";
            if (mapboxProp == "text-max-angle") return ""; // Not directly supported
            if (mapboxProp == "text-rotate") return "text-orientation";
            if (mapboxProp == "text-padding") return "text-min-distance";
            if (mapboxProp == "text-keep-upright") return ""; // Not directly supported
            if (mapboxProp == "text-radial-offset") return ""; // Not directly supported
            if (mapboxProp == "text-variable-anchor") return ""; // Not directly supported
            
            // Icon properties
            if (mapboxProp == "icon-image") return "point-file";
            if (mapboxProp == "icon-size") return ""; // Use point-transform
            if (mapboxProp == "icon-rotate") return ""; // Use point-transform
            if (mapboxProp == "icon-opacity") return "point-opacity";
            if (mapboxProp == "icon-allow-overlap") return "point-allow-overlap";
            if (mapboxProp == "icon-ignore-placement") return "point-ignore-placement";
            if (mapboxProp == "icon-offset") return ""; // Not directly supported
        }
        else if (layerType == "circle") {
            // Circle/point layer properties
            if (mapboxProp == "circle-radius") return "marker-width"; // Also set marker-height
            if (mapboxProp == "circle-color") return "marker-fill";
            if (mapboxProp == "circle-opacity") return "marker-fill-opacity";
            if (mapboxProp == "circle-stroke-color") return "marker-line-color";
            if (mapboxProp == "circle-stroke-width") return "marker-line-width";
            if (mapboxProp == "circle-stroke-opacity") return "marker-line-opacity";
            if (mapboxProp == "circle-blur") return ""; // Not directly supported
            if (mapboxProp == "circle-translate") return ""; // Not directly supported
        }
        else if (layerType == "fill-extrusion") {
            // 3D building properties
            if (mapboxProp == "fill-extrusion-color") return "building-fill";
            if (mapboxProp == "fill-extrusion-opacity") return "building-fill-opacity";
            if (mapboxProp == "fill-extrusion-height") return "building-height";
            if (mapboxProp == "fill-extrusion-base") return "building-min-height";
            if (mapboxProp == "fill-extrusion-pattern") return ""; // Not directly supported
        }
        
        return ""; // Property not mapped
    }

    std::string convertPropertyValue(const picojson::value& value, const std::string& propName, const std::string& layerType) {
        // Handle different value types
        
        if (value.is<std::string>()) {
            std::string strValue = value.get<std::string>();
            
            // Handle color values
            if (propName.find("color") != std::string::npos || propName.find("fill") != std::string::npos) {
                return strValue; // Colors are already in correct format
            }
            
            // Handle text-field property - convert {field} to [field]
            if (propName == "text-field") {
                // Convert {field} syntax to [field] syntax
                std::string converted = strValue;
                size_t pos = 0;
                while ((pos = converted.find("{", pos)) != std::string::npos) {
                    converted[pos] = '[';
                    pos++;
                }
                pos = 0;
                while ((pos = converted.find("}", pos)) != std::string::npos) {
                    converted[pos] = ']';
                    pos++;
                }
                return converted;
            }
            
            // Handle font arrays - text-font in Mapbox is an array
            if (propName == "text-font") {
                return "\"" + strValue + "\"";
            }
            
            // Handle image/pattern references
            if (propName.find("pattern") != std::string::npos || propName.find("image") != std::string::npos) {
                return "url(\"" + strValue + "\")";
            }
            
            // Handle text-transform
            if (propName == "text-transform") {
                return strValue; // uppercase, lowercase, none
            }
            
            // Handle line-cap and line-join
            if (propName == "line-cap" || propName == "line-join") {
                return strValue; // round, butt, square / round, bevel, miter
            }
            
            // Handle symbol-placement
            if (propName == "symbol-placement") {
                return strValue; // point, line, line-center
            }
            
            return "\"" + strValue + "\"";
        }
        else if (value.is<double>()) {
            double numValue = value.get<double>();
            
            // Handle special conversions
            if (propName == "circle-radius") {
                // Circle radius needs to be doubled for marker-width and marker-height
                int diameter = static_cast<int>(numValue * 2);
                return std::to_string(diameter);
            }
            
            // Check if it's an integer value
            if (numValue == static_cast<int>(numValue)) {
                return std::to_string(static_cast<int>(numValue));
            }
            return std::to_string(numValue);
        }
        else if (value.is<bool>()) {
            return value.get<bool>() ? "true" : "false";
        }
        else if (value.is<picojson::array>()) {
            const picojson::array& arr = value.get<picojson::array>();
            
            // Handle dasharray
            if (propName == "line-dasharray") {
                std::ostringstream oss;
                for (size_t i = 0; i < arr.size(); i++) {
                    if (i > 0) oss << ",";
                    if (arr[i].is<double>()) {
                        oss << static_cast<int>(arr[i].get<double>());
                    }
                }
                return oss.str();
            }
            
            // Handle font arrays - take first font
            if (propName == "text-font" && !arr.empty() && arr[0].is<std::string>()) {
                return "\"" + arr[0].get<std::string>() + "\"";
            }
            
            // Check if this is a Mapbox expression (starts with operator string)
            if (!arr.empty() && arr[0].is<std::string>()) {
                std::string op = arr[0].get<std::string>();
                
                // Handle interpolate expressions (stops)
                if (op == "interpolate") {
                    return convertInterpolateExpression(arr, propName);
                }
                
                // Handle step expressions
                if (op == "step") {
                    return convertStepExpression(arr, propName);
                }
                
                // Handle match expressions
                if (op == "match") {
                    return convertMatchExpression(arr, propName);
                }
                
                // Handle get expressions
                if (op == "get") {
                    if (arr.size() >= 2 && arr[1].is<std::string>()) {
                        return "[" + arr[1].get<std::string>() + "]";
                    }
                }
            }
            
            // Handle expressions and other arrays (not fully supported yet)
            // For now, just skip complex expressions
            return "";
        }
        else if (value.is<picojson::object>()) {
            const picojson::object& obj = value.get<picojson::object>();
            
            // Check if this is a stops-based expression
            auto stopsIt = obj.find("stops");
            if (stopsIt != obj.end() && stopsIt->second.is<picojson::array>()) {
                return convertStopsExpression(obj, propName);
            }
            
            // Handle other object-based expressions (not fully supported yet)
            return "";
        }
        
        return "";
    }

    std::string convertStopsExpression(const picojson::object& obj, const std::string& propName) {
        auto stopsIt = obj.find("stops");
        if (stopsIt == obj.end() || !stopsIt->second.is<picojson::array>()) {
            return "";
        }
        
        const picojson::array& stops = stopsIt->second.get<picojson::array>();
        if (stops.empty()) {
            return "";
        }
        
        // Get the interpolation base (default is 1 for linear)
        std::string interpolationType = "linear";
        auto baseIt = obj.find("base");
        if (baseIt != obj.end() && baseIt->second.is<double>()) {
            double base = baseIt->second.get<double>();
            if (base == 2.0) {
                interpolationType = "exp";
            } else if (base != 1.0) {
                interpolationType = "exp"; // Use exp for any non-linear base
            }
        }
        
        // Build CartoCSS function: interpolationType([view::zoom], (zoom1, value1), (zoom2, value2), ...)
        std::ostringstream oss;
        oss << interpolationType << "([view::zoom]";
        
        for (const auto& stop : stops) {
            if (!stop.is<picojson::array>()) continue;
            const picojson::array& stopArr = stop.get<picojson::array>();
            if (stopArr.size() < 2) continue;
            
            std::string zoom = valueToString(stopArr[0]);
            std::string value = valueToString(stopArr[1]);
            
            oss << ", (" << zoom << ", " << value << ")";
        }
        
        oss << ")";
        return oss.str();
    }
    
    std::string convertInterpolateExpression(const picojson::array& arr, const std::string& propName) {
        // Format: ["interpolate", interpolationType, ["zoom"], zoom1, value1, zoom2, value2, ...]
        if (arr.size() < 5) return "";
        
        std::string interpolationType = "linear";
        
        // Parse interpolation type
        if (arr.size() >= 2 && arr[1].is<picojson::array>()) {
            const picojson::array& interpArr = arr[1].get<picojson::array>();
            if (!interpArr.empty() && interpArr[0].is<std::string>()) {
                std::string interpType = interpArr[0].get<std::string>();
                if (interpType == "linear") interpolationType = "linear";
                else if (interpType == "exponential") interpolationType = "exp";
                else if (interpType == "cubic-bezier") interpolationType = "cubic";
            }
        }
        
        // Check if it's based on zoom
        bool isZoomBased = false;
        if (arr.size() >= 3 && arr[2].is<picojson::array>()) {
            const picojson::array& inputArr = arr[2].get<picojson::array>();
            if (!inputArr.empty() && inputArr[0].is<std::string>()) {
                std::string input = inputArr[0].get<std::string>();
                if (input == "zoom") isZoomBased = true;
            }
        }
        
        if (!isZoomBased) return ""; // Only support zoom-based expressions for now
        
        // Build CartoCSS function
        std::ostringstream oss;
        oss << interpolationType << "([view::zoom]";
        
        // Parse stop pairs (starting from index 3)
        for (size_t i = 3; i + 1 < arr.size(); i += 2) {
            std::string zoom = valueToString(arr[i]);
            std::string value = valueToString(arr[i + 1]);
            oss << ", (" << zoom << ", " << value << ")";
        }
        
        oss << ")";
        return oss.str();
    }
    
    std::string convertStepExpression(const picojson::array& arr, const std::string& propName) {
        // Format: ["step", ["zoom"], defaultValue, stop1, value1, stop2, value2, ...]
        if (arr.size() < 4) return "";
        
        // Build CartoCSS step function
        std::ostringstream oss;
        oss << "step([view::zoom]";
        
        // Default value (index 2)
        if (arr.size() >= 3) {
            std::string defaultValue = valueToString(arr[2]);
            oss << ", " << defaultValue;
        }
        
        // Parse stop pairs (starting from index 3)
        for (size_t i = 3; i + 1 < arr.size(); i += 2) {
            std::string zoom = valueToString(arr[i]);
            std::string value = valueToString(arr[i + 1]);
            oss << ", (" << zoom << ", " << value << ")";
        }
        
        oss << ")";
        return oss.str();
    }
    
    std::string convertMatchExpression(const picojson::array& arr, const std::string& propName) {
        // Match expressions need to be converted to multiple rules
        // For now, we'll return empty and handle this at a higher level
        // This signals that the property needs special handling
        return ""; // Will be handled by generating multiple rules
    }

    std::string convertFilter(const picojson::value& filter) {
        if (!filter.is<picojson::array>()) {
            return "";
        }
        
        const picojson::array& filterArray = filter.get<picojson::array>();
        if (filterArray.empty()) {
            return "";
        }
        
        // Get the filter operator
        if (!filterArray[0].is<std::string>()) {
            return "";
        }
        
        std::string op = filterArray[0].get<std::string>();
        
        // Handle simple comparison operators
        if (op == "==") {
            if (filterArray.size() >= 3 && filterArray[1].is<std::string>()) {
                std::string key = filterArray[1].get<std::string>();
                std::string value = valueToString(filterArray[2]);
                return "[" + key + " = " + value + "]";
            }
        }
        else if (op == "!=") {
            if (filterArray.size() >= 3 && filterArray[1].is<std::string>()) {
                std::string key = filterArray[1].get<std::string>();
                std::string value = valueToString(filterArray[2]);
                return "[" + key + " != " + value + "]";
            }
        }
        else if (op == ">") {
            if (filterArray.size() >= 3 && filterArray[1].is<std::string>()) {
                std::string key = filterArray[1].get<std::string>();
                std::string value = valueToString(filterArray[2]);
                return "[" + key + " > " + value + "]";
            }
        }
        else if (op == ">=") {
            if (filterArray.size() >= 3 && filterArray[1].is<std::string>()) {
                std::string key = filterArray[1].get<std::string>();
                std::string value = valueToString(filterArray[2]);
                return "[" + key + " >= " + value + "]";
            }
        }
        else if (op == "<") {
            if (filterArray.size() >= 3 && filterArray[1].is<std::string>()) {
                std::string key = filterArray[1].get<std::string>();
                std::string value = valueToString(filterArray[2]);
                return "[" + key + " < " + value + "]";
            }
        }
        else if (op == "<=") {
            if (filterArray.size() >= 3 && filterArray[1].is<std::string>()) {
                std::string key = filterArray[1].get<std::string>();
                std::string value = valueToString(filterArray[2]);
                return "[" + key + " <= " + value + "]";
            }
        }
        else if (op == "in") {
            // ["in", "field", "value1", "value2", ...]
            // Convert to comma-separated rules (handled at layer level)
            if (filterArray.size() >= 3 && filterArray[1].is<std::string>()) {
                std::string key = filterArray[1].get<std::string>();
                std::ostringstream oss;
                
                // Generate multiple predicates separated by OR marker
                for (size_t i = 2; i < filterArray.size(); i++) {
                    if (i > 2) oss << "|OR|"; // Special separator for OR logic
                    std::string value = valueToString(filterArray[i]);
                    oss << "[" << key << " = " << value << "]";
                }
                return oss.str();
            }
        }
        else if (op == "has") {
            if (filterArray.size() >= 2 && filterArray[1].is<std::string>()) {
                std::string key = filterArray[1].get<std::string>();
                // CartoCSS supports null comparison
                return "[" + key + " != null]";
            }
        }
        else if (op == "!has") {
            if (filterArray.size() >= 2 && filterArray[1].is<std::string>()) {
                std::string key = filterArray[1].get<std::string>();
                // CartoCSS supports null comparison
                return "[" + key + " = null]";
            }
        }
        else if (op == "all") {
            // Combine multiple filters with AND (implicit in CartoCSS)
            std::ostringstream oss;
            for (size_t i = 1; i < filterArray.size(); i++) {
                std::string subFilter = convertFilter(filterArray[i]);
                if (!subFilter.empty()) {
                    // Check if subfilter contains OR logic
                    if (subFilter.find("|OR|") != std::string::npos) {
                        // This is complex - would need expansion at layer level
                        // For now, skip complex AND + OR combinations
                        return "";
                    }
                    oss << subFilter;
                }
            }
            return oss.str();
        }
        else if (op == "any") {
            // Combine multiple filters with OR
            std::ostringstream oss;
            for (size_t i = 1; i < filterArray.size(); i++) {
                if (i > 1) oss << "|OR|";
                std::string subFilter = convertFilter(filterArray[i]);
                if (!subFilter.empty()) {
                    oss << subFilter;
                }
            }
            return oss.str();
        }
        else if (op == "none") {
            // Not directly supported
            return "";
        }
        
        return "";
    }

    std::string valueToString(const picojson::value& value) {
        if (value.is<std::string>()) {
            return "\"" + value.get<std::string>() + "\"";
        }
        else if (value.is<double>()) {
            double num = value.get<double>();
            if (num == static_cast<int>(num)) {
                return std::to_string(static_cast<int>(num));
            }
            return std::to_string(num);
        }
        else if (value.is<bool>()) {
            return value.get<bool>() ? "true" : "false";
        }
        return "";
    }

    std::string getStringProperty(const picojson::object& obj, const std::string& key, const std::string& defaultValue) {
        auto it = obj.find(key);
        if (it != obj.end() && it->second.is<std::string>()) {
            return it->second.get<std::string>();
        }
        return defaultValue;
    }

    double getNumberProperty(const picojson::object& obj, const std::string& key, double defaultValue) {
        auto it = obj.find(key);
        if (it != obj.end() && it->second.is<double>()) {
            return it->second.get<double>();
        }
        return defaultValue;
    }
};

std::string loadFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + filePath);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void saveFile(const std::string& filePath, const std::string& content) {
    std::ofstream file(filePath);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to create file: " + filePath);
    }
    file << content;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: mapbox2css input-style.json output-style.mss" << std::endl;
        std::cerr << "\nConverts Mapbox GL style JSON to CartoCSS (.mss)" << std::endl;
        std::cerr << "Note: Only layer definitions are converted. Source definitions are ignored." << std::endl;
        return 1;
    }

    try {
        std::string inputFile = argv[1];
        std::string outputFile = argv[2];
        
        std::cout << "Converting Mapbox style: " << inputFile << std::endl;
        
        std::string mapboxJson = loadFile(inputFile);
        
        MapboxToCartoCSS converter;
        std::string cartoCSS = converter.convert(mapboxJson);
        
        saveFile(outputFile, cartoCSS);
        
        std::cout << "CartoCSS saved to: " << outputFile << std::endl;
        std::cout << "Conversion complete!" << std::endl;
        
        return 0;
    }
    catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }
}
