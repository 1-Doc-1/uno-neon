#pragma once

#include "uno/app/bot_level.hpp"
#include "uno/app/identifiers.hpp"
#include "uno/app/room_settings.hpp"
#include "uno/core/card.hpp"
#include "uno/core/player_action.hpp"
#include "uno/core/player_id.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <variant>

// What a client may ask for (SPEC §8.3): intentions only, already syntactically valid. The codec of
// uno_net builds them from JSON; the application layer decides whether they are allowed.
namespace uno::app::request {

enum class Emote : std::uint8_t { Gg, Wow, Lol, Ouch, Think, Fire };

struct Hello {
    std::optional<SessionToken> sessionToken;
    std::string clientVersion;

    bool operator==(const Hello&) const = default;
};

struct CreateRoom {
    std::string nickname; // raw: the application layer trims and validates it
    std::optional<RoomSettingsPatch> settings;

    bool operator==(const CreateRoom&) const = default;
};

// A game against bots without a waiting room: the room is created, filled with `botCount` bots and started at once
// (ADR 0030).
struct CreateBotGame {
    std::string nickname; // raw, like CreateRoom's
    std::uint8_t botCount{};
    BotLevel level{};
    std::optional<RoomSettingsPatch> settings;

    bool operator==(const CreateBotGame&) const = default;
};

struct JoinRoom {
    RoomCode code;
    std::string nickname;

    bool operator==(const JoinRoom&) const = default;
};

struct LeaveRoom {
    bool operator==(const LeaveRoom&) const = default;
};

struct UpdateSettings {
    RoomSettingsPatch settings;

    bool operator==(const UpdateSettings&) const = default;
};

struct SetReady {
    bool ready{};

    bool operator==(const SetReady&) const = default;
};

struct Kick {
    core::PlayerId playerId;

    bool operator==(const Kick&) const = default;
};

struct AddBot {
    BotLevel level{};

    bool operator==(const AddBot&) const = default;
};

struct StartMatch {
    bool operator==(const StartMatch&) const = default;
};

struct Rematch {
    bool operator==(const Rematch&) const = default;
};

struct ReadyForNextRound {
    bool operator==(const ReadyForNextRound&) const = default;
};

struct PlayCard {
    core::CardId cardId;
    std::optional<core::Color> chosenColor;
    std::optional<core::PlayerId> swapTargetId;
    std::optional<core::PlayerId> targetId; // the target of a Wild Draw Five (ADR 0028)

    bool operator==(const PlayCard&) const = default;
};

struct DrawCard {
    bool operator==(const DrawCard&) const = default;
};

struct Pass {
    bool operator==(const Pass&) const = default;
};

struct ChooseColor {
    core::Color color{};

    bool operator==(const ChooseColor&) const = default;
};

struct RespondPenalty {
    core::PenaltyResponse response{};

    bool operator==(const RespondPenalty&) const = default;
};

struct CallUno {
    bool operator==(const CallUno&) const = default;
};

struct CatchUno {
    core::PlayerId targetId;

    bool operator==(const CatchUno&) const = default;
};

struct SendReaction {
    Emote emote{};

    bool operator==(const SendReaction&) const = default;
};

using Body = std::variant<Hello, CreateRoom, CreateBotGame, JoinRoom, LeaveRoom, UpdateSettings, SetReady, Kick, AddBot,
                          StartMatch, Rematch, ReadyForNextRound, PlayCard, DrawCard, Pass, ChooseColor, RespondPenalty,
                          CallUno, CatchUno, SendReaction>;

// A message and the identifier the answer (`ack` or `error`) will echo in `replyTo`.
struct Envelope {
    std::string id;
    Body body;

    bool operator==(const Envelope&) const = default;
};

} // namespace uno::app::request
