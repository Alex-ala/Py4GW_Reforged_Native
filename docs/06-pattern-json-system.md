# Pattern JSON System

## Intent

The pattern system is intentionally thin.

It exists to move raw scan constants out of code:

- byte patterns
- masks
- integer offsets
- target sections

It does not attempt to encode all scanner behavior or build a general resolver DSL.

Files:

- `include/base/patterns.h`
- `src/base/patterns.cpp`
- `offsets/render.json`

## Public API

`Patterns` exposes only two operations:

- `Patterns::Initialize(path)`
- `Patterns::Get(name)`

The return type is:

```cpp
struct PatternObject {
    std::string name;
    std::string pattern;
    std::string mask;
    int offset;
    ScannerSection section;
};
```

The caller remains responsible for how those fields are used.

## JSON Shape

Current intended format:

```json
{
  "namespace": "render",
  "patterns": {
    "window_handle_ptr": {
      "pattern": "\\x83\\xC4\\x04\\x83\\x3D\\x00\\x00\\x00\\x00\\x00\\x75\\x31",
      "mask": "xxxxx????xxx",
      "offset": "-0xC",
      "section": "text"
    }
  }
}
```

Fully qualified lookup name:

- namespace + `.` + key

Example:

- `render.window_handle_ptr`

## Loader Behavior

### Namespace Qualification

If the JSON file has:

- `"namespace": "render"`

and the key is:

- `"window_handle_ptr"`

the loader stores:

- `render.window_handle_ptr`

If the key already contains a dot, it is kept as-is.

### Pattern Decoding

The JSON stores byte sequences as escaped string literals such as:

- `\\x83\\xC4\\x04`

The loader converts them into the actual in-memory byte string before storing the `PatternObject`.

Supported escapes:

- `\\`
- `\0`
- `\n`
- `\r`
- `\t`
- `\xNN`

### Offset Parsing

Offsets are parsed from strings via `std::stoll(..., 0)`, so these forms work:

- `"0"`
- `"123"`
- `"-0xC"`
- `"0x0FFF"`

The final stored type is `int`.

### Section Parsing

Recognized section strings:

- `text`
- `rdata`
- `data`

Anything else currently falls back to `Text`.

### Initialization Path

If no directory is passed, the loader uses:

- `process_manager::GetModuleDirectory() / "offsets"`

It loads every `.json` file in that directory, sorted lexicographically.

## Important Design Characteristic

The loader is intentionally not semantic.

For example, the same `offset` field may mean different things at different call sites:

- displacement passed into `Scanner::Find`
- scan range passed into `Scanner::ToFunctionStart`

That is allowed by design. The JSON stores raw values, and the consumer decides how to interpret them.

## Current Consumer

The only current consumer is `src/GW/render/render.cpp`.

Examples:

- `render.window_handle_ptr`
  Used directly as `Scanner::Find(..., offset, section)`
- `render.reset_target`
  Its `offset` is currently used as a `ToFunctionStart` scan range
- `render.end_scene_target`
  Its `offset` is currently used as the `Find` displacement

This asymmetry is real and should be expected until a stricter per-pattern convention is introduced.

## Current File State Note

At the time of analysis, `offsets/render.json` ends with a trailing comma after the `patterns` object. In strict JSON this is invalid.

Because the project uses `nlohmann::json`, that file should be treated as needing cleanup before runtime loading can be relied on.

The documentation records that because it affects actual runtime behavior, not just style.



