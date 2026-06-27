# Pattern JSON System

## Intent

The pattern system is intentionally thin.

It exists to move raw scanner inputs out of code:

- byte patterns
- masks
- integer offsets
- assertion file strings
- assertion message strings
- integer line numbers
- integer scan ranges
- target sections

It does not attempt to encode all scanner behavior or build a general resolver DSL.

Files:

- `include/base/patterns.h`
- `src/base/patterns.cpp`
- `offsets/render.json`
- `offsets/camera.json`

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
    std::string assertion_file;
    std::string assertion_message;
    int offset;
    int line_number;
    int range;
    ScannerSection section;
};
```

The caller remains responsible for how those fields are used.

That includes assertion-driven scans and helper calls such as:

- `Find(...)`
- `FindAssertion(...)`
- `FindInRange(...)`
- `FindUseOfAddress(...)`
- `ToFunctionStart(...)`

The JSON owns raw scanner inputs. The caller still owns control flow and meaning.

## JSON Shape

Byte-pattern entries:

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

Assertion-backed entries:

```json
{
  "namespace": "camera",
  "patterns": {
    "camera_ptr_anchor": {
      "assertion_file": "GmCam.cpp",
      "assertion_message": "fov",
      "line_number": "0",
      "offset": "0x0F",
      "range": "0x0F",
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

### Integer Parsing

Integer fields are parsed from strings via `std::stoll(..., 0)`.

This applies to:

- `offset`
- `line_number`
- `range`

These forms work:

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

- displacement passed into `Scanner::Find(...)`
- assertion offset passed into `Scanner::FindAssertion(...)`
- displacement passed into `Scanner::FindInRange(...)`
- offset passed into `Scanner::FindUseOfAddress(...)`

Likewise, the same `range` field may mean:

- the end-window delta for `Scanner::FindInRange(...)`
- the scan range passed into `Scanner::ToFunctionStart(...)`

That is allowed by design. The JSON stores raw values, and the consumer decides how to interpret them.

## Current Consumers

Current consumers:

- `src/GW/render/render.cpp`
- `src/GW/camera/camera.cpp`

Examples:

- `render.window_handle_ptr`
  Used directly as `Scanner::Find(..., offset, section)`
- `render.reset_target`
  Its `offset` is currently used as a `ToFunctionStart(...)` scan range
- `render.end_scene_target`
  Its `offset` is currently used as the `Find(...)` displacement
- `render.get_transform_target`
  Used directly as `Scanner::Find(..., offset, section)` before `ToFunctionStart(...)`
- `camera.camera_ptr_anchor`
  Supplies the assertion file, assertion message, assertion offset, and `FindInRange(...)` window size for the anchor step
- `camera.camera_ptr_scan`
  Supplies the byte pattern and displacement passed into `Scanner::FindInRange(...)`
- `camera.fog_patch`
  Used directly as `Scanner::Find(..., offset, section)`
- `camera.camera_update_patch_vs2017`
  Used directly as `Scanner::Find(..., offset, section)`
- `camera.camera_update_patch_vs2022`
  Used directly as `Scanner::Find(..., offset, section)`

This asymmetry is real and should be expected until a stricter per-pattern convention is introduced.

## Reinforced Migration Rule

Anything that uses the scanner must source raw scan inputs from `Patterns::Get(...)`.

This includes:

- `Find(...)`
- `FindAssertion(...)`
- `FindInRange(...)`
- `FindUseOfAddress(...)`
- `ToFunctionStart(...)` helper ranges when the range is part of migrated scan data

That rule applies even when the legacy code used inline literals such as:

- raw byte strings
- masks
- assertion file names
- assertion messages
- assertion line numbers
- small helper offsets or scan windows

The point of the pattern system is to remove raw scanner inputs from code, not just direct `Find(...)` byte patterns.

The call site still owns:

- scan ordering
- branching logic
- pointer dereference steps
- semantic validation
