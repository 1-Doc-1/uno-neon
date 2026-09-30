#pragma once

namespace uno::core::detail {

// Combines a set of callables into a single overload set, for use as a std::visit visitor over a
// closed std::variant (ADR 0006: exhaustiveness is checked at compile time). Example:
//   std::visit(Overloaded{
//                  [](const PlayCard& action) { ... },
//                  [](const DrawCard&) { ... },
//              },
//              action);
template <typename... Visitors>
// NOLINTNEXTLINE(misc-multiple-inheritance): the standard std::visit overload-set idiom.
struct Overloaded : Visitors... {
    using Visitors::operator()...;
};

template <typename... Visitors>
Overloaded(Visitors...) -> Overloaded<Visitors...>;

} // namespace uno::core::detail
