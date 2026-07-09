# Settings / INI Handling Design

Status: implemented (module `settings/`, embedded Python module
`PySettings`; first consumers wired in `console_host_ui` and
`console_ui`).
Owner module: `settings/` (top-level module, sibling of `system/`).

## Goals

- Per-account persistence anchored at `settings/<email>/` (the account
  anchor owned by `System`), rooted at the DLL directory, never the game
  process working directory.
- INI on disk: line-oriented, hand-editable, damage-local (a bad line
  loses one key, never the file).
- Python-first ergonomics: scripts use a document as a getter/setter with
  zero prior configuration.
- No third-party dependency; the parser/writer is project-owned.

## Module Layout

Standard module split:

- `include/settings/settings.h` - all module-owned declarations:
  `IniFile`, `SettingsManager`.
- `src/settings/settings.cpp` - internal machinery: parsing, atomic file
  IO, anchor binding, autosave pump internals.
- `src/settings/settings_methods.cpp` - public callable bodies.
- `src/settings/settings_bindings.cpp` - embedded Python module
  `PySettings`.

Dependency direction: `settings -> system` (for the account anchor and
console diagnostics). Nothing in `system` depends on `settings` except
optional consumers of stored values.

## Core Types

### IniFile (one document)

One INI document, fully synchronized behind its own mutex.

- Typed getters, default-driven; never throw. Missing key or
  unconvertible value returns the caller's default:
  - `GetString(section, key, default)`
  - `GetBool(section, key, default)` - accepts 1/0/true/false/yes/no/on/off
  - `GetInt(section, key, default)`
  - `GetFloat(section, key, default)`
- Typed setters mark the document dirty; in-memory effect is immediate:
  - `SetString/SetBool/SetInt/SetFloat(section, key, value)`
- Structure queries and edits:
  - `HasKey(section, key)`, `GetSections()`, `GetKeys(section)`
  - `DeleteKey(section, key)`, `DeleteSection(section)`
- Persistence:
  - `Save()` - explicit flush; `Reload()` - re-read from disk, discarding
    unsaved memory state; `IsDirty()`.

Round-trip fidelity: section order, key order, and comment lines
(`;`/`#`) are preserved across load/save. Unparseable lines are retained
verbatim (and skipped semantically) rather than destroyed on rewrite.

### SettingsManager (registry + anchor + autosave)

Process-wide singleton owning every `IniFile`.

- `Open(name, scope)` returns a reference to a named document. Names are
  sanitized to a safe **relative subpath**: folders are preserved
  (`bots/foo/config.ini` nests under the scope root), but `..`, `.`, and
  drive/absolute segments are stripped, so a document can never escape the
  settings tree. Opening the same (name, scope) twice returns the same
  document. Parent folders are created on bind.
- Scopes (multi-account environment):
  - `Scope::Account` (default): `settings/<email>/<name>`. Staged in
    memory until the account anchor resolves.
  - `Scope::Global`: `settings/<name>` - account-independent, shared by
    every client. Binds and loads immediately at injection (no anchor
    dependency, no staging).
  Global files are shared across processes; saves are last-writer-wins.
  They are intended for read-mostly configuration. Frequently written
  values belong in account scope. If cross-process contention becomes
  real, a named-mutex file lock can be added inside the save path
  without any API change (seam, not built).
- `Update()` - stepped from the runtime update loop:
  1. Anchor binding: while `System::HasAccountEmail()` is false, open
     account-scoped documents exist purely in memory (staged mode). When
     the anchor resolves, each binds to `settings/<email>/<name>`, loads
     whatever is on disk, then re-applies staged writes on top (staged
     writes win: they are newest) and flushes.
  2. Debounced autosave: a dirty document is flushed when quiet for
     ~2 seconds, or at most ~10 seconds after the first unsaved change.
- `FlushAll()` - wired into shutdown before the Python runtime tears
  down, so nothing is lost on exit.

## Write Safety

Every save is atomic: write to `<name>.tmp` in the same directory, then
`ReplaceFile`/rename over the target. Disk always holds either the
complete previous file or the complete new file. Save failures are
reported once to the console and the injection log, not per retry.

## Staging Semantics (pre-anchor)

Reads before the anchor resolves return the caller's default or a staged
write if one exists. Writes are never rejected; they wait in memory.
This mirrors the imgui.ini contract: nothing touches disk until the
account identity is stable.

## Threading

- Per-document mutex for all field access; manager mutex for the
  registry.
- Callers: update loop (autosave pump, C++ consumers), render thread
  (UI-driven writes), Python (any time; GIL is irrelevant to internal
  locking).
- No raw document structures ever escape the API.

## Python Surface (`PySettings`)

The primary requirement: there is NO file lifecycle in Python. No open,
no close, no mandatory save. Constructing the object is the entire
setup; persistence is automatic (manager-owned load, debounced autosave,
shutdown flush, atomic writes). Same name and scope -> same underlying
document, process-wide.

```python
from PySettings import settings

ini = settings("mybot.ini")       # done - account-scoped by default

ini.write("speed", 2.0)           # persisted automatically
v    = ini.read("speed", 1.5)     # default value = fallback + type
v    = ini.read("speed", int)     # type token = typed read
flag = ini.read("debug", False)

shared = settings("global.ini", scope="global")   # cross-account
```

- `settings(name, scope="account")` -> handle to the manager-owned
  document (nodelete). `scope` accepts "account" or "global".
- `write(key, value)` - value type (bool/int/float/str) selected by
  overload; marks dirty, autosaved.
- `read(key, default_or_type)`:
  - given a value: it is the fallback and declares the type;
  - given a Python type object (`bool`, `int`, `float`, `str`): typed
    read where a missing key returns that type's zero value
    (`False`, `0`, `0.0`, `""`). Real type objects, not strings, so a
    typo fails loudly instead of silently.
  Binding note: the bool overload/type check must be handled before int,
  because Python bool is a subclass of int.
- Keys are flat by default: stored under one default section on disk so
  the file remains valid INI. `"section/key"` addresses an explicit
  section when wanted; the C++ layer keeps full (section, key) form.
- Escape hatches only, never required: `save()`, `reload()`,
  `is_dirty()`, `has_key()`, `keys()`, `delete(key)`.
- Module-level helpers: `is_anchored()` (whether account documents are
  bound to disk yet), `get_settings_directory()` (mirrors `PySystem`).

## Document Conventions

- `Py4GW.ini` - core: `[console]` output_to_file and visibility
  preferences, `[settings]` autoexec_script, future core sections.
- `imgui.ini` - ImGui's own layout file; never opened through this
  system.
- Scripts open their own documents; core never reads script documents
  and vice versa. Corruption or misuse stays confined to one file.

## Error Semantics

- `get` never throws: bad type text -> default; missing file -> default.
- `set` never throws: memory always accepts; disk problems surface via
  the save path diagnostics.
- Malformed lines survive round-trips untouched; the parser skips them
  semantically without discarding them.

## Explicitly Out of Scope (seams left open)

- Env/profile layering (dynaconf-style): would become an additional
  lookup layer inside `Get`; not built now.
- File watching / hot reload of external edits: `reload()` is explicit;
  no background watcher.
- Non-INI documents: a future feature needing nested data may add a
  different backend behind the same manager without changing consumers.

## First Consumers (after implementation)

1. Autoexec script path (`[settings] autoexec_script`) - run once after
   the anchor resolves.
2. Console `output_to_file` toggle persistence.
3. Console visibility preferences (compact vs full on startup).
