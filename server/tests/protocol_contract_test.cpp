#include "uno/app/client_message.hpp"
#include "uno/net/codec.hpp"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

// Contract tests (SPEC §8.1): protocol/examples is the shared truth between this server and the Angular
// client. Every valid client example must be accepted and re-encoded to the same JSON; every invalid
// one must be rejected with the error code written in its name.

namespace {

namespace fs = std::filesystem;
using nlohmann::json;

fs::path examplesDirectory()
{
    return {UNO_PROTOCOL_EXAMPLES_DIR};
}

std::string readFile(const fs::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

std::vector<fs::path> filesIn(const fs::path& directory, const std::string& prefix)
{
    std::vector<fs::path> files;
    for (const auto& entry : fs::directory_iterator(directory)) {
        if (entry.is_regular_file() && entry.path().filename().string().starts_with(prefix)) {
            files.push_back(entry.path());
        }
    }
    std::ranges::sort(files);
    return files;
}

} // namespace

TEST_CASE("Every valid client example is accepted and re-encoded to the same JSON", "[net][contract]")
{
    const auto files = filesIn(examplesDirectory() / "valid", "client.");
    REQUIRE_FALSE(files.empty());

    for (const auto& file : files) {
        DYNAMIC_SECTION(file.filename().string())
        {
            const std::string text = readFile(file);

            const auto decoded = uno::net::decodeClientMessage(text);

            REQUIRE(decoded.has_value());
            REQUIRE(json::parse(uno::net::encodeClientMessage(*decoded)) == json::parse(text));
        }
    }
}

TEST_CASE("Every ack and error example is accepted and re-encoded to the same JSON", "[net][contract]")
{
    // The other server messages (room.update, game.update...) join this test with their codec (step 2.4).
    std::vector<fs::path> files = filesIn(examplesDirectory() / "valid", "server.ack");
    for (auto& file : filesIn(examplesDirectory() / "valid", "server.error")) {
        files.push_back(std::move(file));
    }
    REQUIRE_FALSE(files.empty());

    for (const auto& file : files) {
        DYNAMIC_SECTION(file.filename().string())
        {
            const std::string text = readFile(file);

            const auto decoded = uno::net::decodeServerMessage(text);

            REQUIRE(decoded.has_value());
            REQUIRE(json::parse(uno::net::encodeServerMessage(*decoded)) == json::parse(text));
        }
    }
}

TEST_CASE("Every invalid example is rejected with the error code of its name", "[net][contract]")
{
    const auto files = filesIn(examplesDirectory() / "invalid", "");
    REQUIRE_FALSE(files.empty());

    for (const auto& file : files) {
        DYNAMIC_SECTION(file.filename().string())
        {
            const std::string expectedCode = file.filename().string().substr(0, file.filename().string().find('.'));

            const auto decoded = uno::net::decodeClientMessage(readFile(file));

            REQUIRE_FALSE(decoded.has_value());
            REQUIRE(uno::net::errorCodeName(decoded.error().code) == expectedCode);
        }
    }
}
