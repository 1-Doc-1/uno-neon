#include "game_codec.hpp"

#include "field_parsers.hpp"
#include "wire_names.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace uno::net::detail {

using app::response::GameUpdate;
using app::response::GameView;
using app::response::SeatInfo;
using app::response::UnoWindowInfo;

namespace {

using namespace uno::app;
using namespace uno::core;

// ---- encoding ----

Json encodeCard(const Card& card)
{
    return Json{
        {"id", card.id.value},
        {"color", card.color ? Json(toWire(*card.color)) : Json(nullptr)},
        {"rank", toWire(card.rank)},
    };
}

Json encodeCards(const std::vector<Card>& cards)
{
    Json array = Json::array();
    for (const Card& card : cards) {
        array.push_back(encodeCard(card));
    }
    return array;
}

Json encodeColorOrNull(const std::optional<Color>& color)
{
    return color ? Json(toWire(*color)) : Json(nullptr);
}

struct EventEncoder {
    Json operator()(const RoundStartedEvent& event) const
    {
        return Json{{"kind", "roundStarted"}, {"round", event.round}, {"dealerId", event.dealerId.value}};
    }
    Json operator()(const CardPlayedEvent& event) const
    {
        return Json{
            {"kind", "cardPlayed"},           {"playerId", event.playerId.value},
            {"card", encodeCard(event.card)}, {"chosenColor", encodeColorOrNull(event.chosenColor)},
            {"isJumpIn", event.isJumpIn},
        };
    }
    Json operator()(const CardsDrawnEvent& event) const
    {
        Json json{{"kind", "cardsDrawn"}, {"playerId", event.playerId.value}, {"count", event.count}};
        if (event.cards) {
            json.emplace("cards", encodeCards(*event.cards));
        }
        return json;
    }
    Json operator()(const TurnChangedEvent& event) const
    {
        return Json{{"kind", "turnChanged"}, {"playerId", event.playerId.value}};
    }
    Json operator()(const PlayerSkippedEvent& event) const
    {
        return Json{{"kind", "playerSkipped"}, {"playerId", event.playerId.value}};
    }
    Json operator()(const DirectionChangedEvent& event) const
    {
        return Json{{"kind", "directionChanged"}, {"direction", toWire(event.direction)}};
    }
    Json operator()(const ColorChosenEvent& event) const
    {
        return Json{{"kind", "colorChosen"}, {"playerId", event.playerId.value}, {"color", toWire(event.color)}};
    }
    Json operator()(const PenaltyStackedEvent& event) const
    {
        return Json{{"kind", "penaltyStacked"}, {"playerId", event.playerId.value}, {"pendingDraw", event.pendingDraw}};
    }
    Json operator()(const ChallengeResolvedEvent& event) const
    {
        Json json{
            {"kind", "challengeResolved"},
            {"challengerId", event.challengerId.value},
            {"challengedId", event.challengedId.value},
            {"wasBluff", event.wasBluff},
            {"penalizedPlayerId", event.penalizedPlayerId.value},
            {"penaltyAmount", event.penaltyAmount},
        };
        if (event.revealedHand) {
            json.emplace("revealedHand", encodeCards(*event.revealedHand));
        }
        return json;
    }
    Json operator()(const UnoCalledEvent& event) const
    {
        return Json{{"kind", "unoCalled"}, {"playerId", event.playerId.value}};
    }
    Json operator()(const UnoCaughtEvent& event) const
    {
        return Json{
            {"kind", "unoCaught"},
            {"catcherId", event.catcherId.value},
            {"targetId", event.targetId.value},
            {"penaltyAmount", event.penaltyAmount},
        };
    }
    Json operator()(const HandsSwappedEvent& event) const
    {
        return Json{{"kind", "handsSwapped"}, {"playerId", event.playerId.value}, {"targetId", event.targetId.value}};
    }
    Json operator()(const HandsRotatedEvent& event) const
    {
        return Json{{"kind", "handsRotated"}, {"direction", toWire(event.direction)}};
    }
    Json operator()(const DeckReshuffledEvent& event) const
    {
        return Json{{"kind", "deckReshuffled"}, {"drawPileCount", event.drawPileCount}};
    }
    Json operator()(const RoundEndedEvent& event) const
    {
        return Json{{"kind", "roundEnded"}, {"winnerId", event.winnerId.value}, {"points", event.points}};
    }
    Json operator()(const MatchEndedEvent& event) const
    {
        return Json{{"kind", "matchEnded"}, {"winnerId", event.winnerId.value}};
    }
    Json operator()(const PlayerDisconnectedEvent& event) const
    {
        return Json{{"kind", "playerDisconnected"}, {"playerId", event.playerId.value}};
    }
    Json operator()(const PlayerReconnectedEvent& event) const
    {
        return Json{{"kind", "playerReconnected"}, {"playerId", event.playerId.value}};
    }
    Json operator()(const HostChangedEvent& event) const
    {
        return Json{{"kind", "hostChanged"}, {"playerId", event.playerId.value}};
    }
};

Json encodeMe(const MyState& me)
{
    Json hand = encodeCards(me.hand);
    Json playable = Json::array();
    for (const CardId id : me.playableCardIds) {
        playable.push_back(id.value);
    }
    Json penalty = nullptr;
    if (me.penaltyResponse) {
        penalty = Json{
            {"amount", me.penaltyResponse->amount},
            {"canChallenge", me.penaltyResponse->canChallenge},
            {"canStack", me.penaltyResponse->canStack},
        };
    }
    return Json{
        {"playerId", me.playerId.value},
        {"hand", std::move(hand)},
        {"playableCardIds", std::move(playable)},
        {"canDraw", me.canDraw},
        {"canPass", me.canPass},
        {"canCallUno", me.canCallUno},
        {"canChooseColor", me.canChooseColor},
        {"penaltyResponse", std::move(penalty)},
    };
}

Json encodePlayers(const GameView& view)
{
    Json players = Json::array();
    for (std::size_t index = 0; index < view.game.players.size(); ++index) {
        const SeatView& seat = view.game.players.at(index);
        const SeatInfo& info = view.seats.at(index);
        players.push_back(Json{
            {"playerId", seat.playerId.value},
            {"nickname", info.nickname},
            {"seat", seat.seat},
            {"cardCount", seat.cardCount},
            {"score", seat.score},
            {"isConnected", info.isConnected},
            {"isBot", info.isBot},
            {"isHost", info.isHost},
            {"hasCalledUno", seat.hasCalledUno},
            {"isReadyForNextRound", info.isReadyForNextRound},
        });
    }
    return players;
}

Json encodeRoundResult(const std::optional<RoundResult>& result)
{
    if (!result) {
        return nullptr;
    }
    Json hands = Json::object();
    for (const RevealedHand& revealed : result->revealedHands) {
        hands.emplace(revealed.playerId.value, encodeCards(revealed.cards));
    }
    return Json{{"winnerId", result->winnerId.value}, {"points", result->points}, {"revealedHands", std::move(hands)}};
}

template <typename T>
Json nullable(const std::optional<T>& value)
{
    return value ? Json(*value) : Json(nullptr);
}

Json encodeUnoWindows(const std::vector<UnoWindowInfo>& windows)
{
    Json encoded = Json::array();
    for (const UnoWindowInfo& window : windows) {
        encoded.push_back(Json{
            {"targetId", window.targetId.value},
            {"graceEndsAt", window.graceEndsAt},
            {"expiresAt", window.expiresAt},
        });
    }
    return encoded;
}

Json encodeView(const GameView& view)
{
    const PlayerView& game = view.game;
    return Json{
        {"stateVersion", view.stateVersion},
        {"phase", toWire(game.phase)},
        {"me", encodeMe(game.me)},
        {"players", encodePlayers(view)},
        {"currentPlayerId", game.currentPlayerId.value},
        {"direction", toWire(game.direction)},
        {"currentColor", encodeColorOrNull(game.currentColor)},
        {"discardTop", encodeCard(game.discardTop)},
        {"drawPileCount", game.drawPileCount},
        {"pendingDraw", game.pendingDraw},
        {"turnDeadline", nullable(view.turnDeadline)},
        {"nextRoundDeadline", nullable(view.nextRoundDeadline)},
        {"unoWindows", encodeUnoWindows(view.unoWindows)},
        {"round", game.round},
        {"settings", encodeSettings(view.settings)},
        {"roundResult", encodeRoundResult(game.roundResult)},
        {"matchWinnerId", game.matchWinnerId ? Json(game.matchWinnerId->value) : Json(nullptr)},
    };
}

// ---- decoding ----

Parsed<std::size_t> parseCount(const Json& value)
{
    const auto number = parseInteger(value, 0, kMaxJsonInteger);
    if (!number) {
        return std::unexpected(number.error());
    }
    return static_cast<std::size_t>(*number);
}

Parsed<std::uint32_t> parseUint32(const Json& value)
{
    const auto number = parseInteger(value, 0, std::numeric_limits<std::uint32_t>::max());
    if (!number) {
        return std::unexpected(number.error());
    }
    return static_cast<std::uint32_t>(*number);
}

Parsed<std::uint64_t> parseVersion(const Json& value)
{
    const auto number = parseInteger(value, 0, kMaxJsonInteger);
    if (!number) {
        return std::unexpected(number.error());
    }
    return static_cast<std::uint64_t>(*number);
}

Parsed<std::int64_t> parseEpochMillis(const Json& value)
{
    return parseInteger(value, 0, kMaxJsonInteger);
}

// A field that is either null or what `parse` reads.
template <typename T, typename Parser>
auto orNull(Parser parse)
{
    return [parse](const Json& value) -> Parsed<std::optional<T>> {
        if (value.is_null()) {
            return std::optional<T>{};
        }
        auto parsed = parse(value);
        if (!parsed) {
            return std::unexpected(parsed.error());
        }
        return std::optional<T>(std::move(*parsed));
    };
}

template <typename T, typename Parser>
Parsed<std::vector<T>> parseArray(const Json& value, Parser parse)
{
    if (!value.is_array()) {
        return std::unexpected("must be an array");
    }
    std::vector<T> items;
    for (const Json& element : value) {
        auto parsed = parse(element);
        if (!parsed) {
            return std::unexpected(parsed.error());
        }
        items.push_back(std::move(*parsed));
    }
    return items;
}

Parsed<std::optional<Color>> parseColorOrNull(const Json& value)
{
    return orNull<Color>(parseEnum<Color>)(value);
}

Parsed<Card> parseCard(const Json& value)
{
    ObjectReader reader(value, "card");
    Card card;
    card.id = reader.required<CardId>("id", parseCardId);
    card.color = reader.required<std::optional<Color>>("color", parseColorOrNull);
    card.rank = reader.required<Rank>("rank", parseEnum<Rank>);
    if (auto finished = reader.finish(); !finished) {
        return std::unexpected(finished.error());
    }
    return card;
}

Parsed<std::vector<Card>> parseCards(const Json& value)
{
    return parseArray<Card>(value, parseCard);
}

Parsed<std::vector<CardId>> parseCardIds(const Json& value)
{
    return parseArray<CardId>(value, parseCardId);
}

// Reads the fields shared by every event kind that has the shape {kind, playerId}.
template <typename Event>
Parsed<ClientEvent> parsePlayerEvent(const Json& value)
{
    ObjectReader reader(value, "event");
    static_cast<void>(reader.requiredRaw("kind"));
    Event event;
    event.playerId = reader.required<PlayerId>("playerId", parsePlayerId);
    if (auto finished = reader.finish(); !finished) {
        return std::unexpected(finished.error());
    }
    return event;
}

Parsed<ClientEvent> parseRoundStarted(const Json& value)
{
    ObjectReader reader(value, "event");
    static_cast<void>(reader.requiredRaw("kind"));
    RoundStartedEvent event;
    event.round = reader.required<std::uint32_t>("round", parseUint32);
    event.dealerId = reader.required<PlayerId>("dealerId", parsePlayerId);
    if (auto finished = reader.finish(); !finished) {
        return std::unexpected(finished.error());
    }
    return event;
}

Parsed<ClientEvent> parseCardPlayed(const Json& value)
{
    ObjectReader reader(value, "event");
    static_cast<void>(reader.requiredRaw("kind"));
    CardPlayedEvent event;
    event.playerId = reader.required<PlayerId>("playerId", parsePlayerId);
    event.card = reader.required<Card>("card", parseCard);
    event.chosenColor = reader.required<std::optional<Color>>("chosenColor", parseColorOrNull);
    event.isJumpIn = reader.required<bool>("isJumpIn", parseBool);
    if (auto finished = reader.finish(); !finished) {
        return std::unexpected(finished.error());
    }
    return event;
}

Parsed<ClientEvent> parseCardsDrawn(const Json& value)
{
    ObjectReader reader(value, "event");
    static_cast<void>(reader.requiredRaw("kind"));
    CardsDrawnEvent event;
    event.playerId = reader.required<PlayerId>("playerId", parsePlayerId);
    event.count = reader.required<std::size_t>("count", parseCount);
    event.cards = reader.optional<std::vector<Card>>("cards", parseCards);
    if (auto finished = reader.finish(); !finished) {
        return std::unexpected(finished.error());
    }
    return event;
}

Parsed<ClientEvent> parseDirectionChanged(const Json& value)
{
    ObjectReader reader(value, "event");
    static_cast<void>(reader.requiredRaw("kind"));
    DirectionChangedEvent event;
    event.direction = reader.required<Direction>("direction", parseEnum<Direction>);
    if (auto finished = reader.finish(); !finished) {
        return std::unexpected(finished.error());
    }
    return event;
}

Parsed<ClientEvent> parseColorChosen(const Json& value)
{
    ObjectReader reader(value, "event");
    static_cast<void>(reader.requiredRaw("kind"));
    ColorChosenEvent event;
    event.playerId = reader.required<PlayerId>("playerId", parsePlayerId);
    event.color = reader.required<Color>("color", parseEnum<Color>);
    if (auto finished = reader.finish(); !finished) {
        return std::unexpected(finished.error());
    }
    return event;
}

Parsed<ClientEvent> parsePenaltyStacked(const Json& value)
{
    ObjectReader reader(value, "event");
    static_cast<void>(reader.requiredRaw("kind"));
    PenaltyStackedEvent event;
    event.playerId = reader.required<PlayerId>("playerId", parsePlayerId);
    event.pendingDraw = reader.required<std::size_t>("pendingDraw", parseCount);
    if (auto finished = reader.finish(); !finished) {
        return std::unexpected(finished.error());
    }
    return event;
}

Parsed<ClientEvent> parseChallengeResolved(const Json& value)
{
    ObjectReader reader(value, "event");
    static_cast<void>(reader.requiredRaw("kind"));
    ChallengeResolvedEvent event;
    event.challengerId = reader.required<PlayerId>("challengerId", parsePlayerId);
    event.challengedId = reader.required<PlayerId>("challengedId", parsePlayerId);
    event.wasBluff = reader.required<bool>("wasBluff", parseBool);
    event.penalizedPlayerId = reader.required<PlayerId>("penalizedPlayerId", parsePlayerId);
    event.penaltyAmount = reader.required<std::size_t>("penaltyAmount", parseCount);
    event.revealedHand = reader.optional<std::vector<Card>>("revealedHand", parseCards);
    if (auto finished = reader.finish(); !finished) {
        return std::unexpected(finished.error());
    }
    return event;
}

Parsed<ClientEvent> parseUnoCaught(const Json& value)
{
    ObjectReader reader(value, "event");
    static_cast<void>(reader.requiredRaw("kind"));
    UnoCaughtEvent event;
    event.catcherId = reader.required<PlayerId>("catcherId", parsePlayerId);
    event.targetId = reader.required<PlayerId>("targetId", parsePlayerId);
    event.penaltyAmount = reader.required<std::size_t>("penaltyAmount", parseCount);
    if (auto finished = reader.finish(); !finished) {
        return std::unexpected(finished.error());
    }
    return event;
}

Parsed<ClientEvent> parseHandsSwapped(const Json& value)
{
    ObjectReader reader(value, "event");
    static_cast<void>(reader.requiredRaw("kind"));
    HandsSwappedEvent event;
    event.playerId = reader.required<PlayerId>("playerId", parsePlayerId);
    event.targetId = reader.required<PlayerId>("targetId", parsePlayerId);
    if (auto finished = reader.finish(); !finished) {
        return std::unexpected(finished.error());
    }
    return event;
}

Parsed<ClientEvent> parseHandsRotated(const Json& value)
{
    ObjectReader reader(value, "event");
    static_cast<void>(reader.requiredRaw("kind"));
    HandsRotatedEvent event;
    event.direction = reader.required<Direction>("direction", parseEnum<Direction>);
    if (auto finished = reader.finish(); !finished) {
        return std::unexpected(finished.error());
    }
    return event;
}

Parsed<ClientEvent> parseDeckReshuffled(const Json& value)
{
    ObjectReader reader(value, "event");
    static_cast<void>(reader.requiredRaw("kind"));
    DeckReshuffledEvent event;
    event.drawPileCount = reader.required<std::size_t>("drawPileCount", parseCount);
    if (auto finished = reader.finish(); !finished) {
        return std::unexpected(finished.error());
    }
    return event;
}

Parsed<ClientEvent> parseRoundEnded(const Json& value)
{
    ObjectReader reader(value, "event");
    static_cast<void>(reader.requiredRaw("kind"));
    RoundEndedEvent event;
    event.winnerId = reader.required<PlayerId>("winnerId", parsePlayerId);
    event.points = reader.required<std::uint32_t>("points", parseUint32);
    if (auto finished = reader.finish(); !finished) {
        return std::unexpected(finished.error());
    }
    return event;
}

Parsed<ClientEvent> parseMatchEnded(const Json& value)
{
    ObjectReader reader(value, "event");
    static_cast<void>(reader.requiredRaw("kind"));
    MatchEndedEvent event;
    event.winnerId = reader.required<PlayerId>("winnerId", parsePlayerId);
    if (auto finished = reader.finish(); !finished) {
        return std::unexpected(finished.error());
    }
    return event;
}

struct EventKind {
    std::string_view name;
    Parsed<ClientEvent> (*parse)(const Json&);
};

constexpr std::array kEventKinds{
    EventKind{.name = "roundStarted", .parse = parseRoundStarted},
    EventKind{.name = "cardPlayed", .parse = parseCardPlayed},
    EventKind{.name = "cardsDrawn", .parse = parseCardsDrawn},
    EventKind{.name = "turnChanged", .parse = parsePlayerEvent<TurnChangedEvent>},
    EventKind{.name = "playerSkipped", .parse = parsePlayerEvent<PlayerSkippedEvent>},
    EventKind{.name = "directionChanged", .parse = parseDirectionChanged},
    EventKind{.name = "colorChosen", .parse = parseColorChosen},
    EventKind{.name = "penaltyStacked", .parse = parsePenaltyStacked},
    EventKind{.name = "challengeResolved", .parse = parseChallengeResolved},
    EventKind{.name = "unoCalled", .parse = parsePlayerEvent<UnoCalledEvent>},
    EventKind{.name = "unoCaught", .parse = parseUnoCaught},
    EventKind{.name = "handsSwapped", .parse = parseHandsSwapped},
    EventKind{.name = "handsRotated", .parse = parseHandsRotated},
    EventKind{.name = "deckReshuffled", .parse = parseDeckReshuffled},
    EventKind{.name = "roundEnded", .parse = parseRoundEnded},
    EventKind{.name = "matchEnded", .parse = parseMatchEnded},
    EventKind{.name = "playerDisconnected", .parse = parsePlayerEvent<PlayerDisconnectedEvent>},
    EventKind{.name = "playerReconnected", .parse = parsePlayerEvent<PlayerReconnectedEvent>},
    EventKind{.name = "hostChanged", .parse = parsePlayerEvent<HostChangedEvent>},
};

Parsed<ClientEvent> parseEvent(const Json& value)
{
    if (!value.is_object()) {
        return std::unexpected("event must be an object");
    }
    const auto kind = value.find("kind");
    if (kind == value.end() || !kind->is_string()) {
        return std::unexpected("event.kind is required and must be a string");
    }
    const auto name = kind->get<std::string>();
    for (const EventKind& known : kEventKinds) {
        if (known.name == name) {
            return known.parse(value);
        }
    }
    return std::unexpected("event.kind is not an allowed value");
}

Parsed<PenaltyResponseOptions> parsePenaltyOptions(const Json& value)
{
    ObjectReader reader(value, "penaltyResponse");
    PenaltyResponseOptions options;
    options.amount = reader.required<std::size_t>("amount", parseCount);
    options.canChallenge = reader.required<bool>("canChallenge", parseBool);
    options.canStack = reader.required<bool>("canStack", parseBool);
    if (auto finished = reader.finish(); !finished) {
        return std::unexpected(finished.error());
    }
    return options;
}

Parsed<MyState> parseMe(const Json& value)
{
    ObjectReader reader(value, "me");
    MyState me;
    me.playerId = reader.required<PlayerId>("playerId", parsePlayerId);
    me.hand = reader.required<std::vector<Card>>("hand", parseCards);
    me.playableCardIds = reader.required<std::vector<CardId>>("playableCardIds", parseCardIds);
    me.canDraw = reader.required<bool>("canDraw", parseBool);
    me.canPass = reader.required<bool>("canPass", parseBool);
    me.canCallUno = reader.required<bool>("canCallUno", parseBool);
    me.canChooseColor = reader.required<bool>("canChooseColor", parseBool);
    me.penaltyResponse = reader.required<std::optional<PenaltyResponseOptions>>(
        "penaltyResponse", orNull<PenaltyResponseOptions>(parsePenaltyOptions));
    if (auto finished = reader.finish(); !finished) {
        return std::unexpected(finished.error());
    }
    return me;
}

struct SeatEntry {
    SeatView seat;
    SeatInfo info;
};

Parsed<SeatEntry> parseSeatEntry(const Json& value)
{
    ObjectReader reader(value, "player");
    SeatEntry entry;
    entry.seat.playerId = reader.required<PlayerId>("playerId", parsePlayerId);
    entry.info.nickname = reader.required<std::string>("nickname", parseString);
    entry.seat.seat = reader.required<std::size_t>("seat", parseCount);
    entry.seat.cardCount = reader.required<std::size_t>("cardCount", parseCount);
    entry.seat.score = reader.required<std::uint32_t>("score", parseUint32);
    entry.info.isConnected = reader.required<bool>("isConnected", parseBool);
    entry.info.isBot = reader.required<bool>("isBot", parseBool);
    entry.info.isHost = reader.required<bool>("isHost", parseBool);
    entry.seat.hasCalledUno = reader.required<bool>("hasCalledUno", parseBool);
    entry.info.isReadyForNextRound = reader.required<bool>("isReadyForNextRound", parseBool);
    if (auto finished = reader.finish(); !finished) {
        return std::unexpected(finished.error());
    }
    return entry;
}

Parsed<RoundResult> parseRoundResult(const Json& value)
{
    ObjectReader reader(value, "roundResult");
    RoundResult result;
    result.winnerId = reader.required<PlayerId>("winnerId", parsePlayerId);
    result.points = reader.required<std::uint32_t>("points", parseUint32);
    if (const Json* const hands = reader.requiredRaw("revealedHands")) {
        if (!hands->is_object()) {
            reader.fail("revealedHands", "must be an object");
        } else {
            for (const auto& [playerId, cards] : hands->items()) {
                const auto owner = parsePlayerId(Json(playerId));
                const auto parsed = parseCards(cards);
                if (!owner || !parsed) {
                    reader.fail("revealedHands", "has an invalid entry");
                    break;
                }
                result.revealedHands.push_back(RevealedHand{.playerId = *owner, .cards = *parsed});
            }
        }
    }
    if (auto finished = reader.finish(); !finished) {
        return std::unexpected(finished.error());
    }
    return result;
}

Parsed<UnoWindowInfo> parseUnoWindow(const Json& value)
{
    ObjectReader reader(value, "unoWindow");
    UnoWindowInfo window;
    window.targetId = reader.required<PlayerId>("targetId", parsePlayerId);
    window.graceEndsAt = reader.required<std::int64_t>("graceEndsAt", parseEpochMillis);
    window.expiresAt = reader.required<std::int64_t>("expiresAt", parseEpochMillis);
    if (auto finished = reader.finish(); !finished) {
        return std::unexpected(finished.error());
    }
    return window;
}

Parsed<GameView> parseView(const Json& value)
{
    ObjectReader reader(value, "view");
    GameView view;
    PlayerView& game = view.game;
    view.stateVersion = reader.required<std::uint64_t>("stateVersion", parseVersion);
    game.phase = reader.required<ViewPhase>("phase", parseEnum<ViewPhase>);
    game.me = reader.required<MyState>("me", parseMe);
    const auto entries = reader.required<std::vector<SeatEntry>>(
        "players", [](const Json& players) { return parseArray<SeatEntry>(players, parseSeatEntry); });
    for (const SeatEntry& entry : entries) {
        game.players.push_back(entry.seat);
        view.seats.push_back(entry.info);
    }
    game.currentPlayerId = reader.required<PlayerId>("currentPlayerId", parsePlayerId);
    game.direction = reader.required<Direction>("direction", parseEnum<Direction>);
    game.currentColor = reader.required<std::optional<Color>>("currentColor", parseColorOrNull);
    game.discardTop = reader.required<Card>("discardTop", parseCard);
    game.drawPileCount = reader.required<std::size_t>("drawPileCount", parseCount);
    game.pendingDraw = reader.required<std::size_t>("pendingDraw", parseCount);
    view.turnDeadline =
        reader.required<std::optional<std::int64_t>>("turnDeadline", orNull<std::int64_t>(parseEpochMillis));
    view.nextRoundDeadline =
        reader.required<std::optional<std::int64_t>>("nextRoundDeadline", orNull<std::int64_t>(parseEpochMillis));
    view.unoWindows = reader.required<std::vector<UnoWindowInfo>>(
        "unoWindows", [](const Json& windows) { return parseArray<UnoWindowInfo>(windows, parseUnoWindow); });
    game.round = reader.required<std::uint32_t>("round", parseUint32);
    view.settings = reader.required<RoomSettings>("settings", parseSettings);
    game.roundResult =
        reader.required<std::optional<RoundResult>>("roundResult", orNull<RoundResult>(parseRoundResult));
    game.matchWinnerId = reader.required<std::optional<PlayerId>>("matchWinnerId", orNull<PlayerId>(parsePlayerId));
    if (auto finished = reader.finish(); !finished) {
        return std::unexpected(finished.error());
    }
    return view;
}

} // namespace

Json encodeSettings(const RoomSettings& settings)
{
    return Json{
        {"stacking", toWire(settings.stacking)},
        {"jumpIn", settings.jumpIn},
        {"sevenZero", settings.sevenZero},
        {"drawUntilPlayable", settings.drawUntilPlayable},
        {"wildDrawFourMode", toWire(settings.wildDrawFourMode)},
        {"turnTimerSeconds", static_cast<int>(settings.turnTimer)},
        {"matchLength", toWire(settings.matchLength)},
        {"maxPlayers", settings.maxPlayers},
    };
}

Parsed<RoomSettings> parseSettings(const Json& value)
{
    ObjectReader reader(value, "settings");
    RoomSettings settings;
    settings.stacking = reader.required<StackingMode>("stacking", parseEnum<StackingMode>);
    settings.jumpIn = reader.required<bool>("jumpIn", parseBool);
    settings.sevenZero = reader.required<bool>("sevenZero", parseBool);
    settings.drawUntilPlayable = reader.required<bool>("drawUntilPlayable", parseBool);
    settings.wildDrawFourMode = reader.required<WildDrawFourMode>("wildDrawFourMode", parseEnum<WildDrawFourMode>);
    settings.turnTimer = reader.required<TurnTimerSeconds>("turnTimerSeconds", parseTurnTimer);
    settings.matchLength = reader.required<MatchLength>("matchLength", parseEnum<MatchLength>);
    settings.maxPlayers = reader.required<std::uint8_t>("maxPlayers", parseMaxPlayers);
    if (auto finished = reader.finish(); !finished) {
        return std::unexpected(finished.error());
    }
    return settings;
}

Json encodeGameUpdate(const GameUpdate& update)
{
    Json events = Json::array();
    for (const ClientEvent& event : update.events) {
        events.push_back(std::visit(EventEncoder{}, event));
    }
    return Json{{"serverTime", update.serverTime}, {"events", std::move(events)}, {"view", encodeView(update.view)}};
}

Parsed<GameUpdate> parseGameUpdate(const Json& payload)
{
    ObjectReader reader(payload, "payload");
    GameUpdate update;
    update.serverTime = reader.required<std::int64_t>("serverTime", parseEpochMillis);
    update.events = reader.required<std::vector<ClientEvent>>(
        "events", [](const Json& events) { return parseArray<ClientEvent>(events, parseEvent); });
    update.view = reader.required<GameView>("view", parseView);
    if (auto finished = reader.finish(); !finished) {
        return std::unexpected(finished.error());
    }
    return update;
}

} // namespace uno::net::detail
