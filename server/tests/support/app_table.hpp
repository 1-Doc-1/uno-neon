#pragma once

// A room of players driven through the application, for the scenario tests: the lobby is built, a match can be
// started, and requests are derived from the engine's own legal actions.

#include "uno/app/client_message.hpp"
#include "uno/app/error_code.hpp"
#include "uno/app/server_message.hpp"
#include "uno/core/match.hpp"
#include "uno/core/player_action.hpp"

#include "support/app_harness.hpp"
#include "support/require.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

namespace uno::testing {

namespace core = uno::core;
using namespace uno::app;

using Messages = std::vector<response::Message>;

template <typename T>
inline std::vector<T> ofType(const Messages& messages)
{
    std::vector<T> found;
    for (const auto& message : messages) {
        if (const auto* typed = std::get_if<T>(&message)) {
            found.push_back(*typed);
        }
    }
    return found;
}

inline std::optional<ErrorCode> refusal(const Messages& messages)
{
    const auto errors = ofType<response::Error>(messages);
    return errors.empty() ? std::nullopt : std::optional<ErrorCode>(errors.front().code);
}

// The detail of the first error among the messages, if it has one.
inline std::optional<IllegalMoveReason> refusalReason(const Messages& messages)
{
    const auto errors = ofType<response::Error>(messages);
    if (errors.begin() == errors.end()) {
        return std::nullopt;
    }
    return errors.begin()->reason;
}

// A room of `count` players, everyone ready, in the lobby.
struct Table {
    explicit Table(std::size_t count, std::uint64_t seed = 7, core::MatchLength length = core::MatchLength::SingleRound,
                   core::DrawRule drawRule = core::DrawRule::Guided, bool declareUnoToWin = false,
                   core::DrawAmount drawAmount = core::DrawAmount::One,
                   app::Timeouts timeouts = withoutPresentationDelay())
        : harness(seed, timeouts)
    {
        players.push_back(harness.helloPlayer());
        players.front().send(request::CreateRoom{
            .nickname = "Player0",
            .settings = patch(length, drawRule, declareUnoToWin, drawAmount),
        });
        code = players.front().room().code;
        for (std::size_t index = 1; index < count; ++index) {
            players.push_back(harness.helloPlayer());
            players.back().send(request::JoinRoom{.code = code, .nickname = "Player" + std::to_string(index)});
            players.back().send(request::SetReady{.ready = true});
        }
        clearInboxes();
    }

    static RoomSettingsPatch patch(core::MatchLength length, core::DrawRule drawRule = core::DrawRule::Guided,
                                   bool declareUnoToWin = false, core::DrawAmount drawAmount = core::DrawAmount::One)
    {
        RoomSettingsPatch settings;
        settings.matchLength = length;
        settings.drawRule = drawRule;
        settings.declareUnoToWin = declareUnoToWin;
        settings.drawAmount = drawAmount;
        return settings;
    }

    void clearInboxes()
    {
        for (auto& player : players) {
            static_cast<void>(player.received());
        }
    }

    void start() { players.front().send(request::StartMatch{}); }

    Room& room() { return *require(std::optional<Room*>(harness.rooms.find(code))); }

    TestPlayer& playerWithId(const core::PlayerId& id)
    {
        return *std::ranges::find_if(players, [&id](const TestPlayer& player) { return player.id() == id; });
    }

    TestPlayer& currentPlayer() { return playerWithId(room().match->round().currentPlayer()); }

    AppHarness harness;
    std::vector<TestPlayer> players;
    RoomCode code;
};

// The request that plays `card` (a Wild gets a colour, a Wild Draw Five also a target: the player after the actor).
inline request::PlayCard playRequest(const core::Card& card, const core::Round& round)
{
    request::PlayCard play{};
    play.cardId = card.id;
    if (core::isWild(card.rank)) {
        play.chosenColor = core::Color::Red;
    }
    if (card.rank == core::Rank::WildDrawFive) {
        const auto seats = round.seats();
        const auto current = static_cast<std::size_t>(std::ranges::find(seats, round.currentPlayer()) - seats.begin());
        play.targetId = *std::next(seats.begin(), static_cast<std::ptrdiff_t>((current + 1) % seats.size()));
    }
    return play;
}

inline request::Body toRequest(const core::PlayerAction& action)
{
    return std::visit(
        [](const auto& typed) -> request::Body {
            using Action = std::decay_t<decltype(typed)>;
            if constexpr (std::is_same_v<Action, core::PlayCard>) {
                return request::PlayCard{
                    .cardId = typed.cardId,
                    .chosenColor = typed.chosenColor,
                    .swapTargetId = std::nullopt,
                    .targetId = typed.target,
                };
            } else if constexpr (std::is_same_v<Action, core::DrawCard>) {
                return request::DrawCard{};
            } else if constexpr (std::is_same_v<Action, core::Pass>) {
                return request::Pass{};
            } else if constexpr (std::is_same_v<Action, core::ChooseColor>) {
                return request::ChooseColor{.color = typed.color};
            } else if constexpr (std::is_same_v<Action, core::RespondPenalty>) {
                return request::RespondPenalty{.response = typed.response};
            } else if constexpr (std::is_same_v<Action, core::CallUno>) {
                return request::CallUno{};
            } else {
                return request::CatchUno{.targetId = typed.target};
            }
        },
        action);
}

// Every card id the JSON of a message shows: every object that has the three fields of a card, plus the

} // namespace uno::testing
