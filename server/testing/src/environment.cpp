#include "uno/testing/environment.hpp"

#include <cstdlib>
#include <memory>
#include <optional>
#include <string>

namespace uno::testing {

std::optional<std::string> environmentVariable(const char* name)
{
#ifdef _MSC_VER
    // MSVC deprecates std::getenv (C4996); _dupenv_s returns an owned copy instead.
    char* buffer = nullptr;
    std::size_t length = 0;
    if (_dupenv_s(&buffer, &length, name) != 0 || buffer == nullptr) {
        return std::nullopt;
    }
    const std::unique_ptr<char, decltype(&std::free)> owner(buffer, &std::free);
    return std::string(owner.get());
#else
    // Read before any other thread exists: no concurrent setenv possible.
    const char* const value = std::getenv(name); // NOLINT(concurrency-mt-unsafe)
    return value == nullptr ? std::nullopt : std::optional<std::string>(value);
#endif
}

} // namespace uno::testing
