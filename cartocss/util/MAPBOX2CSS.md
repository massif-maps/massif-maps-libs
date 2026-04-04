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

## Detailed Implementation Status

This section provides a comprehensive breakdown of what is implemented, what is not implemented, and what could potentially be added in the future.

### ✅ Fully Implemented Features

These features are fully converted from Mapbox GL to CartoCSS:

#### Layer Types
- ✅ **line layers** - Complete support
- ✅ **fill layers** - Complete support  
- ✅ **symbol layers** - Text and icon properties
- ✅ **circle layers** - Complete support
- ✅ **fill-extrusion layers** - 3D building properties

#### Basic Properties
- ✅ **Colors** - All color formats (hex, rgb, rgba, named colors)
- ✅ **Numeric values** - Integers, floats, percentages
- ✅ **String values** - Text content, font names, enums
- ✅ **Boolean values** - true/false flags

#### Filters
- ✅ **Comparison filters** - `==`, `!=`, `<`, `<=`, `>`, `>=`
- ✅ **all filter** - Combined predicates (AND logic)

#### Zoom Handling
- ✅ **minzoom/maxzoom** - Converted to `[zoom >= N]` predicates

#### Field References
- ✅ **Field substitution** - `{fieldname}` → `[fieldname]`

---

### ⚠️ Partially Implemented Features

These features have limited or incomplete support:

#### 1. **Font Arrays** (Partial - Implementable)
**Current behavior:** Only the first font in the array is used.

**Mapbox:**
```json
"text-font": ["Open Sans Bold", "Arial Unicode MS Bold", "Sans-serif"]
```

**CartoCSS output:**
```css
text-face-name: "Open Sans Bold";
```

**Why partial:** CartoCSS `text-face-name` accepts a single font string, not an array.

**Possible improvement:** Could concatenate fonts with commas if CartoCSS supports fallback fonts. Would need research into CartoCSS font fallback syntax.

**Implementation complexity:** Low - just string concatenation

---

#### 2. **"all" Filters** (Partial - Works but may need adjustment)
**Current behavior:** Multiple predicates are combined sequentially.

**Mapbox:**
```json
["all", ["==", "type", "road"], [">", "rank", 5]]
```

**CartoCSS output:**
```css
[type = "road"][rank > 5]
```

**Why partial:** Works for simple cases but complex nested `all` filters may not convert correctly.

**Possible improvement:** Better handling of nested filter arrays.

**Implementation complexity:** Medium - requires recursive filter parsing

---

#### 3. **Icon/Point Transforms** (Partial - Manual conversion needed)
**Properties affected:**
- `icon-size` - Scale transform
- `icon-rotate` - Rotation transform
- `icon-offset` - Translation (not directly supported in CartoCSS)

**Current behavior:** These properties are skipped.

**Mapbox:**
```json
"icon-size": 1.5,
"icon-rotate": 45
```

**Possible CartoCSS equivalent:**
```css
point-transform: "scale(1.5) rotate(45)";
```

**Why partial:** CartoCSS `point-transform` uses a different syntax. Direct conversion would require understanding the transform string format.

**Possible improvement:** Generate `point-transform` strings from `icon-size` and `icon-rotate`.

**Implementation complexity:** Medium - needs transform string generation logic

---

### ❌ Not Implemented - Cannot Be Implemented

These features have no CartoCSS equivalent and cannot be converted:

#### 1. **Background Layers**
**Reason:** CartoCSS doesn't have a background layer concept. Background is typically set in the Map object or rendering context.

**Mapbox:**
```json
{
  "type": "background",
  "paint": { "background-color": "#f0f0f0" }
}
```

**Alternative:** Set map background color in the CartoCSS `Map` object:
```css
Map { background-color: #f0f0f0; }
```

**Can implement?** No - architectural difference

---

#### 2. **Raster Layers**
**Reason:** CartoCSS is designed for vector tile rendering, not raster tiles.

**Mapbox:**
```json
{
  "type": "raster",
  "source": "satellite"
}
```

**Can implement?** No - fundamentally different rendering pipeline

---

#### 3. **Translate Properties** (Geometric offsets in pixels)
**Properties:**
- `line-translate` / `line-translate-anchor`
- `fill-translate` / `fill-translate-anchor`
- `circle-translate` / `circle-translate-anchor`
- `icon-translate` / `icon-translate-anchor`
- `text-translate` / `text-translate-anchor`

**Reason:** CartoCSS doesn't support pixel-based geometric translation of entire geometries. Some individual properties like `text-dx`/`text-dy` exist for text, but not general translate transforms.

**Mapbox:**
```json
"fill-translate": [5, 10],
"fill-translate-anchor": "viewport"
```

**Can implement?** Partially - text offsets could map to `text-dx`/`text-dy`, but other layer types have no equivalent.

**Implementation complexity:** Low for text, impossible for others

---

#### 4. **Blur Effects**
**Properties:**
- `line-blur`
- `circle-blur`
- `text-halo-blur`

**Reason:** CartoCSS doesn't support blur/feathering effects on rendering.

**Mapbox:**
```json
"line-blur": 2
```

**Can implement?** No - rendering engine limitation

---

#### 5. **Advanced Composite Operations**
**Properties:**
- Various blend modes beyond standard `comp-op`

**Reason:** Mapbox GL supports more advanced blending modes than CartoCSS/Mapnik.

**Mapbox:**
```json
"fill-color": "#ff0000",
"fill-blend-mode": "overlay"
```

**CartoCSS:** Has `comp-op` but with limited modes (src-over, multiply, screen, etc.)

**Can implement?** Partially - basic comp-op modes can be mapped, advanced ones cannot.

---

#### 6. **3D/Perspective Properties**
**Properties:**
- `fill-extrusion-translate`
- `fill-extrusion-vertical-gradient`
- Various pitch/bearing related properties

**Reason:** CartoCSS building symbolizer has limited 3D support compared to Mapbox GL.

**Can implement?** Partially - basic height mapping works, advanced 3D features cannot.

---

#### 7. **Heatmap Layers**
**Reason:** CartoCSS doesn't have heatmap rendering capabilities.

**Mapbox:**
```json
{
  "type": "heatmap",
  "paint": {
    "heatmap-radius": 30,
    "heatmap-intensity": 1
  }
}
```

**Can implement?** No - requires specialized rendering

---

#### 8. **Hillshade Layers**
**Reason:** Terrain-specific rendering not in CartoCSS.

**Mapbox:**
```json
{
  "type": "hillshade",
  "paint": {
    "hillshade-exaggeration": 0.5
  }
}
```

**Can implement?** No - specialized terrain rendering

---

#### 9. **Sky Layers**
**Reason:** Atmospheric rendering not in CartoCSS.

**Mapbox:**
```json
{
  "type": "sky",
  "paint": {
    "sky-type": "gradient"
  }
}
```

**Can implement?** No - 3D atmosphere effects

---

### ❌ Not Implemented - Complex But Potentially Implementable

These features are not implemented but could theoretically be added with significant effort:

#### 1. **Data-Driven Styling (Stops/Expressions)** ⭐ HIGH PRIORITY
**Complexity:** High

**Mapbox expressions:**
```json
"line-width": {
  "stops": [[10, 1], [15, 3], [20, 6]]
}
```
or
```json
"line-color": [
  "interpolate", ["linear"], ["zoom"],
  10, "#ff0000",
  15, "#00ff00"
]
```

**Possible CartoCSS equivalent:**
```css
[zoom >= 10][zoom < 15] { line-width: 1; }
[zoom >= 15][zoom < 20] { line-width: 3; }
[zoom >= 20] { line-width: 6; }
```

**Why not implemented:** Requires parsing Mapbox expression DSL and generating multiple CartoCSS rules with zoom predicates.

**Implementation approach:**
1. Parse `stops` array format
2. Parse expression array format (interpolate, step, etc.)
3. Generate multiple CSS rules with appropriate zoom predicates
4. Handle different interpolation types (linear, exponential, cubic-bezier)

**Implementation complexity:** High - requires expression parser and rule generator

**Benefits:** Would handle the most common advanced Mapbox styles

---

#### 2. **Property Functions (Feature-Driven Styling)** ⭐ MEDIUM PRIORITY
**Complexity:** Medium-High

**Mapbox:**
```json
"circle-radius": {
  "property": "population",
  "stops": [
    [0, 3],
    [100000, 6],
    [1000000, 12]
  ]
}
```

**Possible CartoCSS equivalent:**
```css
[population < 100000] { marker-width: 6; marker-height: 6; }
[population >= 100000][population < 1000000] { marker-width: 12; marker-height: 12; }
[population >= 1000000] { marker-width: 24; marker-height: 24; }
```

**Why not implemented:** Requires parsing property-based stops and generating conditional rules.

**Implementation approach:**
1. Detect property-based stops
2. Generate multiple rules with field predicates
3. Handle interpolation between stops

**Implementation complexity:** High - complex rule generation

---

#### 3. **Case/Match Expressions** ⭐ MEDIUM PRIORITY
**Complexity:** Medium

**Mapbox:**
```json
"line-color": [
  "match",
  ["get", "type"],
  "primary", "#ff0000",
  "secondary", "#00ff00",
  "tertiary", "#0000ff",
  "#cccccc"  // default
]
```

**Possible CartoCSS equivalent:**
```css
[type = "primary"] { line-color: #ff0000; }
[type = "secondary"] { line-color: #00ff00; }
[type = "tertiary"] { line-color: #0000ff; }
// Default would be a rule without the type predicate
```

**Why not implemented:** Requires parsing match expressions and generating multiple rules.

**Implementation approach:**
1. Parse match/case expression arrays
2. Generate separate rules for each case
3. Handle default values

**Implementation complexity:** Medium - pattern matching on expressions

---

#### 4. **"in" Filters** ⭐ LOW PRIORITY
**Complexity:** Low-Medium

**Mapbox:**
```json
["in", "type", "primary", "secondary", "tertiary"]
```

**Possible CartoCSS equivalent:**
Multiple rules or comma-separated selectors (needs research):
```css
#layer[type = "primary"],
#layer[type = "secondary"],
#layer[type = "tertiary"] {
  /* styles */
}
```

**Why not implemented:** CartoCSS doesn't have a direct "in" operator. Would need to generate multiple rules or use comma-separated selectors.

**Implementation approach:**
1. Parse "in" filter
2. Generate either multiple rules or comma-separated selectors
3. Test which approach works in CartoCSS

**Implementation complexity:** Low-Medium - syntax generation

---

#### 5. **"any" Filters (OR Logic)** ⭐ LOW PRIORITY
**Complexity:** Medium

**Mapbox:**
```json
["any", ["==", "type", "primary"], [">", "rank", 5]]
```

**Possible CartoCSS equivalent:**
```css
#layer[type = "primary"] { /* styles */ }
#layer[rank > 5] { /* styles */ }
```

**Why not implemented:** CartoCSS doesn't have OR logic in predicates. Would need to duplicate rules.

**Implementation approach:**
1. Parse "any" filter
2. Generate multiple separate rules
3. Each rule has the same styles but different predicates

**Implementation complexity:** Medium - rule duplication and style management

**Caveat:** This creates duplicate rules which could have specificity issues.

---

#### 6. **"has" / "!has" Filters** ⭐ LOW PRIORITY
**Complexity:** Low

**Mapbox:**
```json
["has", "name"]
["!has", "name"]
```

**Possible CartoCSS equivalent:**
```css
[name != null]  // for "has"
[name = null]   // for "!has" (if CartoCSS supports this)
```

**Why not implemented:** Unclear if CartoCSS supports null comparisons. Needs research.

**Implementation approach:**
1. Research CartoCSS null handling
2. If supported, map to null comparison
3. If not, skip these filters

**Implementation complexity:** Low - simple mapping if supported

---

#### 7. **Complex Text Anchors** ⭐ LOW PRIORITY
**Complexity:** Low-Medium

**Mapbox:**
```json
"text-anchor": "top-left",
"text-offset": [1.5, 0.5]
```

**Possible CartoCSS equivalent:**
```css
text-horizontal-alignment: left;
text-vertical-alignment: top;
text-dx: /* calculated value */;
text-dy: /* calculated value */;
```

**Why not implemented:** Requires mapping compound anchor values to separate alignment properties and calculating offsets.

**Implementation approach:**
1. Parse anchor values (top, bottom, left, right, center combinations)
2. Map to horizontal and vertical alignment
3. Convert offsets to dx/dy

**Implementation complexity:** Low-Medium - value mapping

---

#### 8. **Symbol Placement on Lines** ⭐ MEDIUM PRIORITY
**Complexity:** Low

**Mapbox:**
```json
"symbol-placement": "line",
"symbol-spacing": 250
```

**CartoCSS:**
```css
text-placement: line;
text-spacing: 250;
```

**Why not implemented:** Simple property mapping was overlooked.

**Implementation approach:**
1. Add mapping for `symbol-placement` → `text-placement` / `marker-placement`
2. Add mapping for `symbol-spacing` → `text-spacing` / `marker-spacing`

**Implementation complexity:** Very Low - simple property addition

---

#### 9. **Text Rotation Alignment** ⭐ LOW PRIORITY
**Complexity:** Low

**Mapbox:**
```json
"text-rotation-alignment": "viewport",
"text-pitch-alignment": "viewport"
```

**CartoCSS:** May have limited equivalents - needs research

**Why not implemented:** Unclear CartoCSS support.

**Implementation complexity:** Low if supported, none if not

---

#### 10. **Pattern/Image Fill Opacity** ⭐ LOW PRIORITY
**Complexity:** Very Low

**Mapbox:**
```json
"fill-pattern": "pattern-name",
"fill-opacity": 0.5
```

**CartoCSS:**
```css
polygon-pattern-file: url("pattern-name");
polygon-pattern-opacity: 0.5;
```

**Why not implemented:** Need to combine pattern file with opacity property.

**Implementation approach:**
1. When `fill-pattern` is present, also convert `fill-opacity` to `polygon-pattern-opacity`
2. Same for `line-pattern`

**Implementation complexity:** Very Low - conditional property mapping

---

### 🔍 Needs Research

These features might be implementable but require investigation of CartoCSS capabilities:

1. **Font fallbacks** - Does CartoCSS support comma-separated font lists?
2. **Null comparisons** - Can CartoCSS compare fields to null?
3. **Advanced comp-op modes** - Which blend modes are supported?
4. **Pattern opacity** - Confirmed support for pattern-specific opacity?
5. **Text upright** - Does `text-keep-upright` have a CartoCSS equivalent?
6. **Symbol avoid edges** - Is there a CartoCSS equivalent?
7. **Variable anchors** - Any support for multiple anchor fallbacks?

---

### 📊 Implementation Priority Summary

**High Priority** (Most impact, reasonable complexity):
1. ⭐ Data-driven styling (stops/expressions) - Most common advanced feature
2. ⭐ Property functions (feature-driven) - Very common in Mapbox styles

**Medium Priority** (Good impact, moderate complexity):
1. Case/match expressions - Common for categorical styling
2. Symbol placement on lines - Simple but useful
3. Complex text anchors - Improves text positioning

**Low Priority** (Less common or lower impact):
1. "in" filters - Can work around with multiple rules
2. "any" filters - Can work around with duplicate rules  
3. "has"/"!has" filters - Less commonly used
4. Pattern opacity - Edge case
5. Text rotation alignment - Advanced feature

**Research Needed** (Before prioritizing):
1. Font fallbacks
2. Null comparisons
3. Various "needs research" items above

---

### 🛠️ How to Contribute

If you want to implement any of these features:

1. **Easy wins** (start here):
   - Symbol placement properties
   - Pattern opacity handling
   - Text anchor mapping
   
2. **Research tasks** (validate feasibility):
   - Test CartoCSS font fallback syntax
   - Test null comparison support
   - Document supported comp-op modes

3. **Complex features** (requires design):
   - Stops/expression parser
   - Property function converter
   - Match expression handler

Each feature should:
- Update the property mapping in `mapPropertyName()`
- Update the value conversion in `convertPropertyValue()`
- Add tests with sample Mapbox styles
- Update this documentation

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
