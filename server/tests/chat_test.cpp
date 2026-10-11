#include "uno/app/chat.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <string>

// What may be said in the chat (ADR 0037): one visible line of 1 to 200 characters.

using uno::app::kMaxChatLength;
using uno::app::validateChatMessage;

TEST_CASE("A normal message is kept, trimmed of the spaces around it", "[app][chat]")
{
    REQUIRE(validateChatMessage("Salut !") == "Salut !");
    REQUIRE(validateChatMessage("   Salut  les  amis   ") == "Salut  les  amis");
    REQUIRE(validateChatMessage("Bien joué, Zoé ! 🎉") == "Bien joué, Zoé ! 🎉");
}

TEST_CASE("Markup is only text: it passes untouched and is never interpreted here", "[app][chat]")
{
    REQUIRE(validateChatMessage("<script>alert(1)</script>") == "<script>alert(1)</script>");
}

TEST_CASE("The length counts characters, not bytes", "[app][chat]")
{
    std::string accents;
    for (std::size_t index = 0; index < kMaxChatLength; ++index) {
        accents += "é"; // two bytes each
    }
    REQUIRE(validateChatMessage(accents).has_value());
    REQUIRE_FALSE(validateChatMessage(accents + "é").has_value());
    REQUIRE_FALSE(validateChatMessage("").has_value());
    REQUIRE_FALSE(validateChatMessage("     ").has_value());
}

TEST_CASE("Control characters, line breaks and tabs are refused", "[app][chat]")
{
    const std::string prefix = "a";
    const std::string suffix = "b";
    for (const char* control : {"\n", "\r", "\t", "\x01", "\x7f", "\xC2\x85"}) {
        CAPTURE(control);
        REQUIRE_FALSE(validateChatMessage(std::string(prefix).append(control).append(suffix)).has_value());
    }
    REQUIRE_FALSE(validateChatMessage(std::string("a\0b", 3)).has_value());
}

// NOLINTBEGIN(misc-misleading-bidirectional): the point of the test is to refuse them
TEST_CASE("Characters that hide or reorder text are refused", "[app][chat]")
{
    const std::string prefix = "a";
    const std::string suffix = "b";
    for (const char* hidden : {
             "\xE2\x80\x8B",     // zero-width space
             "\xE2\x80\x8F",     // right-to-left mark
             "\xE2\x80\xAE",     // right-to-left override
             "\xE2\x81\xA6",     // left-to-right isolate
             "\xEF\xBB\xBF",     // byte order mark
             "\xE2\x80\xA8",     // line separator
             "\xEE\x80\x80",     // private use
             "\xF3\xA0\x80\x81", // tag character
         }) {
        CAPTURE(hidden);
        REQUIRE_FALSE(validateChatMessage(std::string(prefix).append(hidden).append(suffix)).has_value());
    }
}
// NOLINTEND(misc-misleading-bidirectional)

TEST_CASE("Text that is not valid UTF-8 is refused", "[app][chat]")
{
    for (const char* text : {"\xC3\x28", "\xA0\xA1", "\xE2\x28\xA1", "\xF0\x28\x8C\xBC", "\xC0\xAF", "\xED\xA0\x80"}) {
        CAPTURE(text);
        REQUIRE_FALSE(validateChatMessage(text).has_value());
    }
}
