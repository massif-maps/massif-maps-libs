# Quick Reference: Mapbox GL to CartoCSS Conversion

## New Features Quick Reference

### Background Layers

| Mapbox GL | CartoCSS |
|-----------|----------|
| `{ "type": "background", "paint": { "background-color": "#f00" } }` | `Map { background-color: #f00; }` |

### Expressions

| Mapbox GL | CartoCSS |
|-----------|----------|
| `{ "stops": [[10, 1], [20, 5]] }` | `linear([view::zoom], (10, 1), (20, 5))` |
| `["interpolate", ["linear"], ["zoom"], 10, 1, 20, 5]` | `linear([view::zoom], (10, 1), (20, 5))` |
| `["step", ["zoom"], 1, 15, 3, 20, 6]` | `step([view::zoom], 1, (15, 3), (20, 6))` |

### Filters

| Mapbox GL Filter | CartoCSS Output |
|------------------|-----------------|
| `["==", "type", "road"]` | `[type = "road"]` |
| `["!=", "type", "rail"]` | `[type != "rail"]` |
| `[">", "rank", 5]` | `[rank > 5]` |
| `["has", "name"]` | `[name != null]` |
| `["!has", "name"]` | `[name = null]` |
| `["in", "type", "a", "b", "c"]` | Multiple rules: `[type = "a"]`, `[type = "b"]`, `[type = "c"]` |
| `["any", ["==","x",1], ["==","y",2]]` | Multiple rules: `[x = 1]`, `[y = 2]` |

### Symbol Properties

| Mapbox GL | CartoCSS |
|-----------|----------|
| `symbol-placement: line` | `text-placement: line` |
| `symbol-spacing: 250` | `text-spacing: 250` |

### Pattern Opacity

| Mapbox GL | CartoCSS |
|-----------|----------|
| `fill-pattern + fill-opacity` | `polygon-pattern-file + polygon-pattern-opacity` |
| `line-pattern + line-opacity` | `line-pattern-file + line-pattern-opacity` |

## Interpolation Functions

CartoCSS supports these interpolation functions:

- `linear([view::zoom], ...)` - Linear interpolation
- `exp([view::zoom], ...)` - Exponential interpolation
- `cubic([view::zoom], ...)` - Cubic bezier interpolation
- `step([view::zoom], ...)` - Step function (no interpolation)
- `log([view::zoom], ...)` - Logarithmic interpolation

## Common Patterns

### Zoom-Based Width
```json
// Mapbox GL
{
  "line-width": {
    "stops": [[5, 0.5], [10, 1], [15, 2], [20, 4]]
  }
}
```
```css
/* CartoCSS */
line-width: linear([view::zoom], (5, 0.5), (10, 1), (15, 2), (20, 4));
```

### Multiple Type Filter
```json
// Mapbox GL
{
  "filter": ["in", "class", "primary", "secondary", "tertiary"]
}
```
```css
/* CartoCSS - generates 3 rules */
#roads[class = "primary"] { ... }
#roads[class = "secondary"] { ... }
#roads[class = "tertiary"] { ... }
```

### Conditional Styling
```json
// Mapbox GL
{
  "filter": ["any", 
    ["==", "amenity", "restaurant"],
    ["==", "amenity", "cafe"]
  ]
}
```
```css
/* CartoCSS - generates 2 rules */
#pois[amenity = "restaurant"] { ... }
#pois[amenity = "cafe"] { ... }
```

### Field Existence Check
```json
// Mapbox GL
{
  "filter": ["has", "name"]
}
```
```css
/* CartoCSS */
#layer[name != null] { ... }
```

### Background Map
```json
// Mapbox GL
{
  "type": "background",
  "paint": {
    "background-color": "#e0e0e0",
    "background-opacity": 0.8
  }
}
```
```css
/* CartoCSS */
Map {
  background-color: #e0e0e0;
  background-opacity: 0.8;
}
```

### Pattern Fill
```json
// Mapbox GL
{
  "fill-pattern": "dots",
  "fill-opacity": 0.6
}
```
```css
/* CartoCSS */
polygon-pattern-file: url("dots");
polygon-pattern-opacity: 0.6;
```

## Tips

1. **Zoom-based expressions** are fully supported via CartoCSS functions
2. **OR logic** (in/any filters) generates multiple rules automatically
3. **Null checks** work with has/!has filters
4. **Pattern opacity** is handled intelligently when pattern is present
5. **Background layers** become Map objects
6. **Symbol placement** properties map directly

## Limitations

- Feature-based expressions (based on data properties) not yet supported
- Match expressions detected but not fully converted
- Complex text anchors need manual conversion
- Only zoom-based interpolation currently supported (not feature-based)
