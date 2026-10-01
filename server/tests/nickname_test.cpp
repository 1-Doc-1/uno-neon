#include "uno/app/nickname.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>

using uno::app::nicknameKey;
using uno::app::validateNickname;

TEST_CASE("A nickname is trimmed and kept as typed", "[app][nickname]")
{
    REQUIRE(validateNickname("  Léa  ") == "Léa");
    REQUIRE(validateNickname("a b") == "a b");
}

TEST_CASE("A nickname counts characters, not bytes", "[app][nickname]")
{
    REQUIRE(validateNickname("éé").has_value());
    REQUIRE(validateNickname("é").has_value() == false);
    std::string sixteen;
    for (int count = 0; count < 16; ++count) {
        sixteen += "é";
    }
    REQUIRE(validateNickname(sixteen).has_value());
    REQUIRE_FALSE(validateNickname(sixteen + "é").has_value());
}

TEST_CASE("Invalid UTF-8 is never a nickname", "[app][nickname]")
{
    REQUIRE_FALSE(validateNickname("ab\xFF").has_value());
    REQUIRE_FALSE(validateNickname("\xC0\xAF").has_value());      // overlong
    REQUIRE_FALSE(validateNickname("a\xED\xA0\x80").has_value()); // surrogate
    REQUIRE_FALSE(validateNickname("ab\xE2\x82").has_value());    // truncated
}

TEST_CASE("Case differences do not make two nicknames different", "[app][nickname]")
{
    REQUIRE(nicknameKey("Léa") == nicknameKey("LÉA"));
    REQUIRE(nicknameKey("MAX") == nicknameKey("max"));
    REQUIRE(nicknameKey("Мария") == nicknameKey("МАРИЯ"));
    REQUIRE(nicknameKey("Zoé") != nicknameKey("Zoe"));
}
