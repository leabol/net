# C++ Code Style (Project-wide)

This repository uses a pragmatic variant of Google C++ Style tailored for modern C++17:

- Indent: 4 spaces, no tabs; max line length 100
- Pointers: left-aligned (`Type* p`)
- Includes: sorted and regrouped; prefer forward declarations in headers
- Names:
  - Classes/Structs: `CamelCase` (e.g., `InetAddress`)
  - Functions: `lowerCamelCase` (e.g., `bindAddr`)
  - Variables: `lowerCamelCase`; private members suffixed with `_` (e.g., `socketfd_`)
  - Constants: `kCamelCase` (e.g., `kBufSize`)
  - Macros: `ALL_CAPS`
- Modern C++: RAII, `noexcept` where applicable, `std::string_view` for read-only strings,
  `enum class`, `= delete` / `= default`, and avoiding raw new/delete

## Tooling
- Formatting: `.clang-format` (Google-based with tweaks)
- Static analysis: `.clang-tidy` (modernize, readability, performance, bugprone)
- VS Code: `settings.json` enables format-on-save and clang-tidy

## Usage
- Format all files (optional):
  ```bash
  find include src -type f \( -name "*.hpp" -o -name "*.h" -o -name "*.cpp" -o -name "*.cc" -o -name "*.cxx" \) -print0 | xargs -0 clang-format -i
  ```
- Run clang-tidy (example):
  ```bash
  clang-tidy -p build $(git ls-files | grep -E "\.(cpp|cc|cxx)$")
  ```

## Notes
- Avoid `using namespace` in headers.
- Prefer forward declarations in headers; include heavy/system headers in `.cpp` files.
- Keep exception messages actionable; log with context.
