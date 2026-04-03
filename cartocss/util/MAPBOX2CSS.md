# Mapbox to CartoCSS Converter (mapbox2css)

## Overview

`mapbox2css` is a utility tool that converts Mapbox GL style JSON files to CartoCSS (.mss) format. This tool extracts layer definitions from Mapbox styles and translates their layout and paint properties to equivalent CartoCSS properties.

## Usage

```bash
mapbox2css input-style.json output-style.mss
```

**Arguments:**
- `input-style.json` - Input Mapbox GL style JSON file
- `output-style.mss` - Output CartoCSS file

## Features

### Supported Layer Types

The converter supports the following Mapbox GL layer types:

1. **line** - Line/stroke layers
2. **fill** - Polygon/area layers
3. **symbol** - Text and icon layers
4. **circle** - Circle/point marker layers
5. **fill-extrusion** - 3D building layers

**Note:** `background` and `raster` layers are not converted as they don't have direct CartoCSS equivalents.

### Property Mapping

The converter maps Mapbox GL properties to their CartoCSS equivalents:

#### Line Layer Properties

| Mapbox Property | CartoCSS Property | Notes |
|----------------|-------------------|-------|
| `line-color` | `line-color` | Direct mapping |
| `line-opacity` | `line-opacity` | Direct mapping |
| `line-width` | `line-width` | Direct mapping |
| `line-offset` | `line-offset` | Direct mapping |
| `line-dasharray` | `line-dasharray` | Direct mapping |
| `line-pattern` | `line-pattern-file` | Wrapped in url() |
| `line-cap` | `line-cap` | Direct mapping |
| `line-join` | `line-join` | Direct mapping |
| `line-miter-limit` | `line-miterlimit` | Direct mapping |
| `line-gap-width` | - | Not supported |
| `line-blur` | - | Not supported |
| `line-round-limit` | - | Not supported |

#### Fill Layer Properties

| Mapbox Property | CartoCSS Property | Notes |
|----------------|-------------------|-------|
| `fill-color` | `polygon-fill` | Direct mapping |
| `fill-opacity` | `polygon-opacity` | Direct mapping |
| `fill-pattern` | `polygon-pattern-file` | Wrapped in url() |
| `fill-outline-color` | - | Use separate attachment |
| `fill-antialias` | - | Not supported |
| `fill-translate` | - | Not supported |

#### Symbol Layer Properties

| Mapbox Property | CartoCSS Property | Notes |
|----------------|-------------------|-------|
| **Text Properties** | | |
| `text-field` | `text-name` | {field} → [field] |
| `text-font` | `text-face-name` | First font used |
| `text-size` | `text-size` | Direct mapping |
| `text-color` | `text-fill` | Direct mapping |
| `text-opacity` | `text-opacity` | Direct mapping |
| `text-halo-color` | `text-halo-fill` | Direct mapping |
| `text-halo-width` | `text-halo-radius` | Direct mapping |
| `text-transform` | `text-transform` | Direct mapping |
| `text-allow-overlap` | `text-allow-overlap` | Direct mapping |
| `text-max-width` | `text-wrap-width` | Direct mapping |
| `text-letter-spacing` | `text-character-spacing` | Direct mapping |
| `text-justify` | `text-horizontal-alignment` | Direct mapping |
| `text-line-height` | `text-line-spacing` | Direct mapping |
| `text-rotate` | `text-orientation` | Direct mapping |
| `text-padding` | `text-min-distance` | Direct mapping |
| **Icon Properties** | | |
| `icon-image` | `point-file` | Wrapped in url() |
| `icon-opacity` | `point-opacity` | Direct mapping |
| `icon-allow-overlap` | `point-allow-overlap` | Direct mapping |
| `icon-ignore-placement` | `point-ignore-placement` | Direct mapping |
| `icon-size` | - | Use point-transform |
| `icon-rotate` | - | Use point-transform |
| `icon-offset` | - | Not supported |

#### Circle Layer Properties

| Mapbox Property | CartoCSS Property | Notes |
|----------------|-------------------|-------|
| `circle-radius` | `marker-width` | Diameter = radius * 2 |
| `circle-color` | `marker-fill` | Direct mapping |
| `circle-opacity` | `marker-fill-opacity` | Direct mapping |
| `circle-stroke-color` | `marker-line-color` | Direct mapping |
| `circle-stroke-width` | `marker-line-width` | Direct mapping |
| `circle-stroke-opacity` | `marker-line-opacity` | Direct mapping |
| `circle-blur` | - | Not supported |
| `circle-translate` | - | Not supported |

#### Fill-Extrusion Layer Properties

| Mapbox Property | CartoCSS Property | Notes |
|----------------|-------------------|-------|
| `fill-extrusion-color` | `building-fill` | Direct mapping |
| `fill-extrusion-opacity` | `building-fill-opacity` | Direct mapping |
| `fill-extrusion-height` | `building-height` | Direct mapping |
| `fill-extrusion-base` | `building-min-height` | Direct mapping |
| `fill-extrusion-pattern` | - | Not supported |

### Filter Conversion

The converter translates Mapbox GL filters to CartoCSS predicates:

| Mapbox Filter | CartoCSS Predicate | Example |
|--------------|-------------------|---------|
| `["==", "field", "value"]` | `[field = "value"]` | Equality |
| `["!=", "field", "value"]` | `[field != "value"]` | Inequality |
| `[">", "field", 100]` | `[field > 100]` | Greater than |
| `[">=", "field", 100]` | `[field >= 100]` | Greater or equal |
| `["<", "field", 100]` | `[field < 100]` | Less than |
| `["<=", "field", 100]` | `[field <= 100]` | Less or equal |
| `["all", filter1, filter2]` | Combined predicates | AND logic |
| `["in", "field", ...]` | - | Not supported |
| `["has", "field"]` | - | Not supported |
| `["any", ...]` | - | Not supported |
| `["none", ...]` | - | Not supported |

### Zoom Level Handling

The converter automatically translates `minzoom` and `maxzoom` properties to CartoCSS zoom predicates:

**Mapbox:**
```json
{
  "minzoom": 10,
  "maxzoom": 15
}
```

**CartoCSS:**
```css
[zoom >= 10][zoom < 15]
```

### Layer Naming

- If `source-layer` is present, it's used as the layer name (selector)
- Otherwise, the layer `id` is used
- Layer names are prefixed with `#` in CartoCSS

## Limitations

### Not Supported

The following Mapbox GL features are **not supported** in the conversion:

1. **Source definitions** - Ignored as specified in requirements
2. **Background layers** - No CartoCSS equivalent
3. **Raster layers** - No direct mapping
4. **Complex expressions** - Only simple values are converted
5. **Data-driven styling** (stops/expressions) - Not fully supported
6. **Some advanced properties**:
   - Translate properties
   - Some transform properties
   - Complex anchor/alignment options
   - Blend modes beyond basic comp-op

### Partial Support

1. **"all" filters** - Combined but may need manual adjustment
2. **Font arrays** - Only first font is used
3. **Icon transforms** - Need manual conversion to point-transform

## Example Conversion

### Input (Mapbox GL Style)

```json
{
  "layers": [
    {
      "id": "road",
      "type": "line",
      "source-layer": "roads",
      "minzoom": 10,
      "maxzoom": 20,
      "filter": ["==", "type", "primary"],
      "layout": {
        "line-cap": "round",
        "line-join": "round"
      },
      "paint": {
        "line-color": "#ff0000",
        "line-width": 3,
        "line-opacity": 0.8
      }
    },
    {
      "id": "cities",
      "type": "symbol",
      "source-layer": "places",
      "minzoom": 5,
      "layout": {
        "text-field": "{name}",
        "text-font": ["Open Sans Bold"],
        "text-size": 12
      },
      "paint": {
        "text-color": "#000000",
        "text-halo-color": "#ffffff",
        "text-halo-width": 2
      }
    }
  ]
}
```

### Output (CartoCSS)

```css
/* CartoCSS generated from Mapbox GL style */

#roads[type = "primary"][zoom >= 10][zoom < 20] {
  line-cap: round;
  line-join: round;
  line-color: #ff0000;
  line-width: 3;
  line-opacity: 0.8;
}

#places[zoom >= 5] {
  text-face-name: "Open Sans Bold";
  text-name: [name];
  text-size: 12;
  text-fill: #000000;
  text-halo-fill: #ffffff;
  text-halo-radius: 2;
}
```

## Building

The tool is built as part of the cartocss utilities. Make sure you have the required dependencies:

- CMake 3.5+
- C++17 compatible compiler
- mobile-external-libs (for picojson and other dependencies)

Build with CMake:

```bash
cd cartocss/util
mkdir build && cd build
cmake ..
make mapbox2css
```

## Post-Conversion Steps

After conversion, you may need to:

1. **Review the output** - Check for properties that weren't converted
2. **Add missing styles** - Some features may need manual addition
3. **Adjust filters** - Complex filters may need refinement
4. **Test rendering** - Verify the visual output matches expectations
5. **Add source definitions** - CartoCSS files need their own data source configuration

## Notes

- The converter focuses on layer definitions only, as specified
- Source properties are intentionally ignored
- The tool provides a starting point; manual adjustments may be needed for complex styles
- Not all Mapbox GL features have CartoCSS equivalents
- Some visual differences are expected due to rendering engine differences

## See Also

- [CartoCSS Documentation](https://cartocss.readthedocs.io/)
- [Mapbox GL Style Specification](https://docs.mapbox.com/mapbox-gl-js/style-spec/)
- Other utilities: `css2xml`, `mvt2xml`
