# IDL Generator Tool

The library includes `idlgen`, a command-line tool that generates IDL (Interface Definition Language) files from C++ interface declarations. This bridges the gap between rtcsdk's pure-C++ interface definitions and the broader COM ecosystem — enabling type library (`.tlb`) generation for cross-language interop, automation clients, and proxy/stub marshaling, all without maintaining separate IDL files.

## Why Use This Tool?

When you declare interfaces with `COM_INTERFACE`, the interface contract lives entirely in C++ headers. This is ideal for in-process C++ consumers, but other scenarios require IDL or type libraries:

* **.NET interop** — `tlbimp` needs a `.tlb` to generate managed wrappers
* **Scripting / automation** — VBScript, PowerShell, and late-binding clients discover interfaces through type libraries
* **Out-of-process COM** — proxy/stub DLLs are generated from IDL by MIDL
* **Documentation** — IDL serves as a language-neutral interface specification

`idlgen` lets you keep your single source of truth in C++ while generating IDL on demand.

## Building

```bash
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release --target idlgen
```

The resulting executable is at `build/tools/idlgen/Release/idlgen.exe`.

## Usage

```
idlgen [options] <input-files...>
```

| Option | Description |
|--------|-------------|
| `-o <dir>` | Output directory for generated `.idl` files (default: current directory) |
| `--library-name <name>` | Library name for the IDL library block (default: derived from first input filename) |
| `--library-uuid <uuid>` | Library UUID — if provided, a `library` block is generated for `.tlb` compilation |
| `--clang-format-path <path>` | Path to `clang-format` executable (default: search PATH) |
| `--no-clang-format` | Disable clang-format preprocessing even if available |
| `--midl` | Invoke MIDL compiler after generating `.idl` to produce `.tlb` |
| `--midl-path <path>` | Path to `midl.exe` (default: search PATH) |
| `--verbose` | Print parsing details, heuristic decisions, and warnings |
| `--help` | Show help |

## Examples

Generate IDL from a header:

```bash
idlgen myinterfaces.h -o output/
```

Generate IDL with a library block and compile to type library:

```bash
idlgen myinterfaces.h -o output/ \
    --library-name MyLib \
    --library-uuid "12345678-1234-1234-1234-123456789ABC" \
    --midl
```

Process multiple headers:

```bash
idlgen include/interfaces1.h include/interfaces2.h -o output/ --verbose
```

## Processing Pipeline

```
C++ Headers → Preprocessor → Parser → Type Mapper → IDL Generator → .idl [→ MIDL → .tlb]
```

1. **Preprocessor** — Normalizes the C++ source: optionally runs `clang-format` for syntactically-aware formatting, strips comments (preserving direction annotations), removes C++ attributes (`[[nodiscard]]`, etc.) and preprocessor directives, collapses whitespace.

2. **Parser** — Finds `COM_INTERFACE` and `COM_INTERFACE_BASE` macro invocations using regex matching, extracts interface bodies via brace matching, and parses individual virtual method declarations.

3. **Type Mapper** — Converts C++ types to their IDL equivalents (e.g., `int` → `long`, `DWORD` → `unsigned long`, `BSTR` → `BSTR`). Unknown types that look like COM interfaces (starting with `I` + uppercase) are passed through. Unmappable types like `std::wstring` generate warnings.

4. **IDL Generator** — Produces standard IDL output with `[object]`, `uuid()`, and `pointer_default(unique)` attributes. Optionally generates a `library` block for type library compilation.

## Automatic Return Type Transformation

Standard COM IDL expects methods to return `HRESULT` with actual results via `[out, retval]` parameters. Since rtcsdk allows non-HRESULT return types, the tool automatically transforms them:

```C++
// C++ input:
COM_INTERFACE(ISampleInterface, "{AB9A7AF1-6792-4D0A-83BE-8252A8432B45}")
{
    virtual int sum(int a, int b) const noexcept = 0;
    virtual int get_answer() const noexcept = 0;
};
```

```idl
// Generated IDL:
[
  object,
  uuid(AB9A7AF1-6792-4D0A-83BE-8252A8432B45),
  pointer_default(unique)
]
interface ISampleInterface : IUnknown
{
    HRESULT sum([in] long a, [in] long b, [out, retval] long* pResult);
    HRESULT get_answer([out, retval] long* pResult);
};
```

Methods that already return `HRESULT` or `void` are emitted as-is.

## Parameter Direction Annotations

Explicitly annotate parameters with IDL direction attributes using comments:

```C++
virtual HRESULT GetData(/*[in]*/ int id, /*[out,retval]*/ BSTR* result) = 0;
virtual HRESULT SetConfig(/*[in]*/ BSTR key, /*[in]*/ VARIANT value) = 0;
virtual HRESULT Transfer(/*[in,out]*/ LONG* pCount) = 0;
```

Supported: `/*[in]*/`, `/*[out]*/`, `/*[in,out]*/`, `/*[out,retval]*/`.

These annotations are preserved through preprocessing and take precedence over heuristics.

## Direction Heuristics

When annotations are omitted, the tool infers parameter direction from C++ type patterns:

| C++ Pattern | Inferred Direction | Rationale |
|---|---|---|
| `const T*`, `const T&` | `[in]` | Const implies read-only |
| Primitive by value (`int`, `float`, etc.) | `[in]` | Value types are input-only |
| `T**` | `[out]` | Standard COM output pattern |
| Last non-const `T*` with `HRESULT` return | `[out, retval]` | Convention: last output param is the logical return |
| Other non-const `T*` with `HRESULT` return | `[in, out]` | Non-last pointer params assumed bidirectional |
| Non-const `T*` with non-`HRESULT` return | `[in, out]` | Conservative default |

## Type Mapping Reference

| C++ Type | IDL Type | C++ Type | IDL Type |
|---|---|---|---|
| `int` | `long` | `HRESULT` | `HRESULT` |
| `unsigned int` | `unsigned long` | `BSTR` | `BSTR` |
| `short` | `short` | `VARIANT` | `VARIANT` |
| `long` | `long` | `VARIANT_BOOL` | `VARIANT_BOOL` |
| `DWORD` | `unsigned long` | `GUID` / `REFIID` | `GUID` |
| `BYTE` | `byte` | `float` | `float` |
| `BOOL` | `long` | `double` | `double` |
| `long long` / `__int64` | `hyper` | `void` | `void` |
| `LPWSTR` | `LPWSTR` | `LPCWSTR` | `LPCWSTR` |

COM interface types and `IFoo`-convention types pass through as-is. Pointer indirection is preserved. References are mapped to pointers. Unmappable types generate warnings.

## Preprocessing Details

**Phase A: clang-format (optional)** — If found on PATH, pipes through with:
* `ColumnLimit: 0` — no line wrapping
* `BinPackParameters: false` — one parameter per line
* `BreakBeforeBraces: Allman` — consistent braces

Use `--no-clang-format` to skip. Falls back to built-in whitespace normalizer if unavailable.

**Phase B: Custom cleanup (always)** — Strips comments (preserving direction annotations), removes C++ attributes and preprocessor directives, collapses whitespace.

## Library Block Generation

With `--library-uuid`, generates a library block for MIDL type library compilation:

```idl
[
  uuid(12345678-1234-1234-1234-123456789ABC),
  version(1.0)
]
library MyLib
{
    importlib("stdole2.tlb");
    interface ISampleInterface;
};
```

## Limitations

* Regex-based parser — handles standard `COM_INTERFACE` patterns but not complex template-heavy signatures
* Inheritance is determined by the macro used
* Only pure virtual methods (`= 0`) are extracted
* Generated IDL may differ from C++ when return type transformation is applied — the IDL is for type library generation, not direct vtable matching
