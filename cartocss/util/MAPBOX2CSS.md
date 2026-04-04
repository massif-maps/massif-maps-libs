# Mapbox to CartoCSS Converter (mapbox2css)

## Overview

`mapbox2css` is a utility tool that converts Mapbox GL style JSON files to CartoCSS (.mss) format. This tool extracts layer definitions from Mapbox styles and translates their layout and paint properties to equivalent CartoCSS properties.

## Recent Updates

### ✨ Newly Implemented Features (2026)

The following advanced features have been recently added:

1. **Background Layers → Map Object**
   - Background layers are now converted to `Map { }` definitions
   - Supports `background-color` and `background-opacity`

2. **Data-Driven Styling (Expressions)**
   - **Stops expressions** - Converted to CartoCSS functions
   - **Interpolate expressions** - Supports linear, exp, cubic interpolation
   - **Step expressions** - Converted to step functions
   - Format: `linear([view::zoom], (z1, v1), (z2, v2), ...)`

3. **Advanced Filters**
   - **"in" filters** - Generates multiple rules (OR logic)
   - **"any" filters** - Generates multiple rules (OR logic)
   - **"has"/"!has" filters** - Uses null comparison (`[field != null]`)

4. **Symbol Placement**
   - `symbol-placement` → `text-placement`
   - `symbol-spacing` → `text-spacing`

5. **Pattern Opacity**
   - When `fill-pattern` + `fill-opacity` present → `polygon-pattern-opacity`
   - When `line-pattern` + `line-opacity` present → `line-pattern-opacity`

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

1. **background** - Converted to Map object with background properties
2. **line** - Line/stroke layers
3. **fill** - Polygon/area layers
4. **symbol** - Text and icon layers
5. **circle** - Circle/point marker layers
6. **fill-extrusion** - 3D building layers

**Note:** `raster` layers are not converted as they don't have direct CartoCSS equivalents.

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
| **Placement Properties** | | |
| `symbol-placement` | `text-placement` | point, line, line-center |
| `symbol-spacing` | `text-spacing` | Direct mapping |
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

#### Background Layer Properties

| Mapbox Property | CartoCSS Property | Notes |
|----------------|-------------------|-------|
| `background-color` | `background-color` | Converted to Map object |
| `background-opacity` | `background-opacity` | Converted to Map object |

### Data-Driven Styling (Expressions)

The converter now supports Mapbox expressions for data-driven styling!

#### Stops Expression (Legacy Format)

**Mapbox:**
```json
{
  "line-width": {
    "stops": [[10, 1], [15, 3], [20, 6]]
  }
}
```

**CartoCSS:**
```css
line-width: linear([view::zoom], (10, 1), (15, 3), (20, 6));
```

#### Interpolate Expression

**Mapbox:**
```json
{
  "line-width": ["interpolate", ["linear"], ["zoom"], 10, 2, 15, 5, 20, 10]
}
```

**CartoCSS:**
```css
line-width: linear([view::zoom], (10, 2), (15, 5), (20, 10));
```

**Supported interpolation types:**
- `["linear"]` → `linear()`
- `["exponential", base]` → `exp()`  
- `["cubic-bezier", ...]` → `cubic()`

#### Step Expression

**Mapbox:**
```json
{
  "line-width": ["step", ["zoom"], 1, 12, 2, 15, 4, 18, 8]
}
```

**CartoCSS:**
```css
line-width: step([view::zoom], 1, (12, 2), (15, 4), (18, 8));
```

**Available CartoCSS functions:**
- `linear()` - Linear interpolation
- `exp()` - Exponential interpolation
- `cubic()` - Cubic bezier interpolation
- `step()` - Step function (no interpolation)
- `log()` - Logarithmic interpolation

### Pattern with Opacity

When both pattern and opacity are specified, the converter intelligently combines them:

**Mapbox:**
```json
{
  "fill-pattern": "water-pattern",
  "fill-opacity": 0.7
}
```

**CartoCSS:**
```css
polygon-pattern-file: url("water-pattern");
polygon-pattern-opacity: 0.7;
```

Same logic applies to `line-pattern` + `line-opacity` → `line-pattern-opacity`.

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
| `["has", "field"]` | `[field != null]` | ✅ **NEW** Field exists |
| `["!has", "field"]` | `[field = null]` | ✅ **NEW** Field missing |
| `["in", "field", v1, v2, v3]` | Multiple rules | ✅ **NEW** OR logic (see below) |
| `["any", filter1, filter2]` | Multiple rules | ✅ **NEW** OR logic (see below) |
| `["none", ...]` | - | Not supported |

#### "in" Filter - Multiple Rules

The "in" filter generates multiple rules with OR logic:

**Mapbox:**
```json
{
  "filter": ["in", "type", "residential", "commercial", "industrial"]
}
```

**CartoCSS (generates 3 separate rules):**
```css
#buildings[type = "residential"] {
  polygon-fill: #cccccc;
}

#buildings[type = "commercial"] {
  polygon-fill: #cccccc;
}

#buildings[type = "industrial"] {
  polygon-fill: #cccccc;
}
```

#### "any" Filter - Multiple Rules

The "any" filter also generates multiple rules:

**Mapbox:**
```json
{
  "filter": ["any", ["==", "category", "restaurant"], ["==", "category", "cafe"]]
}
```

**CartoCSS (generates 2 separate rules):**
```css
#pois[category = "restaurant"] {
  text-name: [name];
}

#pois[category = "cafe"] {
  text-name: [name];
}
```

#### "has" / "!has" Filters - Null Comparison

**Mapbox:**
```json
{
  "filter": ["has", "name"]
}
```

**CartoCSS:**
```css
#places[name != null] {
  text-name: [name];
}
```

**Mapbox:**
```json
{
  "filter": ["!has", "name"]
}
```

**CartoCSS:**
```css
#places[name = null] {
  point-file: url("dot");
}
```

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
- ✅ **in filter** - Generates multiple rules for OR logic
- ✅ **any filter** - Generates multiple rules for OR logic
- ✅ **has filter** - Converted to `[field != null]`
- ✅ **!has filter** - Converted to `[field = null]`

#### Zoom Handling
- ✅ **minzoom/maxzoom** - Converted to `[zoom >= N]` predicates

#### Field References
- ✅ **Field substitution** - `{fieldname}` → `[fieldname]`

#### Data-Driven Styling
- ✅ **Stops expressions** - Converted to CartoCSS functions (linear, exp, cubic, step, log)
- ✅ **Interpolate expressions** - Supports linear, exponential, cubic-bezier
- ✅ **Step expressions** - Converted to step() function

#### Symbol Placement
- ✅ **symbol-placement** - Converted to `text-placement`
- ✅ **symbol-spacing** - Converted to `text-spacing`

#### Pattern Opacity
- ✅ **fill-pattern + fill-opacity** - Converted to `polygon-pattern-opacity`
- ✅ **line-pattern + line-opacity** - Converted to `line-pattern-opacity`

#### Background Layers
- ✅ **Background to Map** - Background layers converted to Map { } object
- ✅ **background-color** - Converted to Map background-color
- ✅ **background-opacity** - Converted to Map background-opacity

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

#### 1. **Raster Layers**
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

#### 1. **Property Functions (Feature-Driven Styling)** ⭐ HIGH PRIORITY
**Complexity:** Medium-High
**Status:** Not yet implemented (zoom-based expressions are done, but not feature-based)

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

#### 2. **Case/Match Expressions** ⭐ MEDIUM PRIORITY
**Complexity:** Medium
**Status:** Partial - detect but not fully converted yet

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

**Why not fully implemented:** Match expressions are detected but need multi-rule generation like "in" filter.

**Implementation approach:**
1. Parse match/case expression arrays
2. Generate separate rules for each case
3. Handle default values

**Implementation complexity:** Medium - similar to "in" filter implementation

---

#### 3. **Complex Text Anchors** ⭐ LOW PRIORITY
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

#### 4. **Text Rotation Alignment** ⭐ LOW PRIORITY
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
1. ⭐ Property functions (feature-driven) - Very common in Mapbox styles

**Medium Priority** (Good impact, moderate complexity):
1. Case/match expressions - Common for categorical styling (partially done)
2. Complex text anchors - Improves text positioning

**Low Priority** (Less common or lower impact):
1. Text rotation alignment - Advanced feature

**Completed** (Recently implemented):
1. ✅ Data-driven styling (stops/expressions) - zoom-based
2. ✅ "in" filters - Multiple rules generated
3. ✅ "any" filters - Multiple rules generated  
4. ✅ "has"/"!has" filters - Null comparison
5. ✅ Pattern opacity - Intelligent handling
6. ✅ Symbol placement - Direct mapping

**Research Needed** (Before prioritizing):
1. Font fallbacks
2. Various "needs research" items above

---

### 🛠️ How to Contribute

If you want to implement any of these features:

1. **Easy wins** (start here):
   - Text anchor mapping
   - Complete match expression conversion
   
2. **Research tasks** (validate feasibility):
   - Test CartoCSS font fallback syntax
   - Document supported comp-op modes

3. **Complex features** (requires design):
   - Property function converter (feature-based styling)
   - Complete match expression handler

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
