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

        // Process layers
        auto layersIt = styleObj.find("layers");
        if (layersIt != styleObj.end() && layersIt->second.is<picojson::array>()) {
            const picojson::array& layers = layersIt->second.get<picojson::array>();
            
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
    std::string convertLayer(const picojson::object& layer) {
        // Get layer properties
        std::string id = getStringProperty(layer, "id", "");
        std::string type = getStringProperty(layer, "type", "");
        std::string sourceLayer = getStringProperty(layer, "source-layer", "");
        
        // Skip background and raster layers
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
        
        // Get minzoom and maxzoom
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
        
        for (const auto& prop : props) {
            std::string propName = prop.first;
            const picojson::value& propValue = prop.second;
            
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
            
            // Handle expressions and other arrays (not fully supported yet)
            // For now, just skip complex expressions
            return "";
        }
        else if (value.is<picojson::object>()) {
            // Handle stops, expressions, etc. (not fully supported yet)
            // This would require more complex conversion
            return "";
        }
        
        return "";
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
            // NOTE: 'in' filters are not directly supported in CartoCSS predicates.
            // Would require multiple rule definitions or when() expressions.
            // Skipping for now - users need to handle manually.
            return "";
        }
        else if (op == "has") {
            if (filterArray.size() >= 2 && filterArray[1].is<std::string>()) {
                std::string key = filterArray[1].get<std::string>();
                // CartoCSS doesn't have a direct "has" operator
                // Could use [field != null] but that's not quite the same
                return "";
            }
        }
        else if (op == "!has") {
            // Similar to has, not directly supported
            return "";
        }
        else if (op == "all") {
            // Combine multiple filters with AND (implicit in CartoCSS)
            std::ostringstream oss;
            for (size_t i = 1; i < filterArray.size(); i++) {
                std::string subFilter = convertFilter(filterArray[i]);
                if (!subFilter.empty()) {
                    oss << subFilter;
                }
            }
            return oss.str();
        }
        else if (op == "any") {
            // Combine multiple filters with OR
            // Not directly supported in CartoCSS predicates
            // Would need multiple rule definitions
            return "";
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
