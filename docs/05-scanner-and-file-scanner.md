# Scanner And File Scanner

## Role Split

The scanner layer has two levels:

- `FileScanner`
  Performs section-aware scanning against a mapped PE image on disk.
- `Scanner`
  Adapts those results into the in-memory address space of the loaded module.

Files:

- `include/base/file_scanner.h`
- `src/base/file_scanner.cpp`
- `include/base/scanner.h`
- `src/base/scanner.cpp`

## Why Two Layers Exist

The project does not scan the live process image byte-by-byte directly. Instead:

1. it maps the game executable from disk with `SEC_IMAGE_NO_EXECUTE`
2. records the section layout in the mapped file
3. records the live section layout of the loaded module in memory
4. computes the offset difference between disk-mapped and live `.text`
5. translates scan hits from disk-view addresses to live in-memory addresses

This approach gives deterministic scanning against a mapped PE image while still returning usable runtime addresses.

## `ScannerSection`

The scanner exposes three known sections:

- `Text`
- `RData`
- `Data`

The enum is small but central. It is used by:

- section selection in scans
- pointer validation
- pattern objects

## `FileScanner::CreateFromPath`

This function performs the PE mapping and structural validation.

Main steps:

1. open file with `CreateFileW`
2. create file mapping with `SEC_IMAGE_NO_EXECUTE | PAGE_READONLY`
3. map view with `MapViewOfFile`
4. query mapped region size with `VirtualQuery`
5. validate DOS header
6. validate NT signature
7. enforce `IMAGE_FILE_MACHINE_I386`
8. validate optional header size
9. locate section headers
10. build a `FileScanner` object from discovered image-base and sections

This is strong enough for the project’s current assumptions:

- PE file
- 32-bit image
- standard `.text`, `.rdata`, `.data` sections

## `FileScanner` Section Recording

The constructor walks PE section headers and records mapped address ranges for:

- `.text`
- `.rdata`
- `.data`

Any other sections are ignored.

## `Scanner::Initialize`

This is the bridge between on-disk and in-memory views.

It:

1. resolves the target module if one was not passed in
2. obtains the executable path
3. creates the backing `FileScanner`
4. reads live PE section headers from the loaded module
5. records live `.text`, `.rdata`, `.data` ranges
6. computes `section_offset_from_disk`

`section_offset_from_disk` is the critical conversion value used to translate addresses between both views.

## Pattern Search APIs

### `Find`

Searches one named section.

### `FindInRange`

Searches an arbitrary address span.

Important behavior:

- if `start > end`, `FileScanner::FindInRange` scans backward
- otherwise it scans forward

That backward-scan behavior is relied on by helpers like `ToFunctionStart()` and string-usage lookups.

### `FindAssertion`

Builds a synthetic byte pattern around:

- assertion line number
- assertion file string
- assertion message string

It is substantially more specialized than simple byte-pattern matching and exists because assertion signatures are common anchors in older reverse-engineering workflows.

## Address Translation

`Scanner::Find*` methods generally:

1. ask `FileScanner` to scan mapped-on-disk sections
2. subtract `section_offset_from_disk`
3. return a live address

This means the public `Scanner` API is intended to return in-memory addresses usable for hooks and dereferences.

## Near-Call Resolution

`Scanner::FunctionFromNearCall` supports:

- `E8`
- `E9`
- `EB`

It recursively resolves nested near calls/jumps and can optionally validate that the final target remains inside `.text`.

This is used by the hook layer before installing MinHook detours.

## Function Start Recovery

`Scanner::ToFunctionStart` scans backward for:

- `55 8B EC`

This is a classic 32-bit function prologue heuristic. It is useful, but fragile:

- it assumes frame-pointer based prologues
- it assumes MSVC-style 32-bit codegen
- it can fail on optimized or hand-written assembly paths

## String and Address Usage Helpers

### `FindUseOfAddress`

Treats the address as a 4-byte literal and searches for references to it in a chosen section.

### `FindUseOfString`

Two variants:

- narrow string
- wide string

They first find the string in `.rdata`, then locate the first null terminator after it, then search for references to that address.

These helpers are higher-level conveniences layered on top of the raw section scan primitives.

## Pointer Validation

`Scanner::IsValidPtr` only checks:

- non-zero
- greater than section start
- less than section end

It does not validate memory protection or pointer provenance. It is a lightweight section-bound check, not a full safety guarantee.

## Current Constraints

The scanner code is strongly tied to:

- 32-bit PE layout
- legacy function prolog assumptions
- the presence and meaning of `.text`, `.rdata`, `.data`

That is consistent with the project target, but it should not be mistaken for a generic process scanner.

