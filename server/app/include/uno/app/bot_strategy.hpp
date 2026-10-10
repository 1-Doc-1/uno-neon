#pragma once

#include "uno/app/bot_level.hpp"
#include "uno/core/player_action.hpp"
#include "uno/core/player_id.hpp"
#include "uno/core/player_view.hpp"
#include "uno/core/random_source.hpp"

#include <memory>
#include <span>
#include <vector>

namespace uno::app {

// Everything a bot is allowed to know, and nothing else (ADR 0030, security invariant 2): the PlayerView of its own
// seat, which holds no other hand, no draw pile order and no seed, plus the players it may catch with CatchUno right
// now (the ones whose UNO window is open and past its grace period: a time the engine does not know). A strategy has no
// way to reach the Round, so it cannot cheat.
struct BotView {
    const core::PlayerView& game; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members): a short-lived view
    std::span<const core::PlayerId> catchable;
};

// Strategy pattern (SPEC §7.3): a bot is a function from what it sees to what it does. The actions are the ones a human
// sends and the application validates them in exactly the same way; a bot that proposes an illegal move is refused like
// a client would be.
class BotStrategy {
public:
    BotStrategy() = default;
    BotStrategy(const BotStrategy&) = delete;
    BotStrategy& operator=(const BotStrategy&) = delete;
    BotStrategy(BotStrategy&&) = delete;
    BotStrategy& operator=(BotStrategy&&) = delete;
    virtual ~BotStrategy() = default;

    // What the bot does now, in order (announce UNO, then play...); empty when it has nothing to do. `random` is the
    // only source of chance, injected like everywhere else.
    [[nodiscard]] virtual std::vector<core::PlayerAction> decide(const BotView& view,
                                                                 core::RandomSource& random) const = 0;
};

[[nodiscard]] std::unique_ptr<BotStrategy> makeBotStrategy(BotLevel level);

} // namespace uno::app
