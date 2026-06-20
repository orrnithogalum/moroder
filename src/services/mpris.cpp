#include "../../include/services/mpris.hpp"

#include "spdlog/spdlog.h"

namespace {

/* dispatch
- Remote control arrives from playerctl, desktop widgets and media keys, none
  of which leave any other trace, so log both the call and any refusal
*/
template <typename Fn> void dispatch(const char* name, bool allowed, Fn&& fn) {
    if (!allowed) {
        spdlog::warn("MPRIS: {} refused, the control is not currently available", name);
        return;
    }

    spdlog::info("MPRIS: {}", name);
    fn();
}

}

bool services::Mpris::canControl() const {
    return bool(loop_status_changed_fn) && bool(shuffle_changed_fn) && bool(stop_fn);
}

bool services::Mpris::canGoNext() const {
    return canControl() && bool(next_fn) && this->is_next_possible;
}

bool services::Mpris::canGoPrevious() const {
    return canControl() && bool(previous_fn) && this->is_previous_possible;
}

bool services::Mpris::canPlay() const {
    return canControl() && bool(play_fn) && bool(play_pause_fn);
}

bool services::Mpris::canPause() const {
    return canControl() && bool(pause_fn) && bool(play_pause_fn);
}

bool services::Mpris::canSeek() const {
    return canControl() && bool(seek_fn) && bool(setPosition_fn);
}

void services::Mpris::onQuit(std::function<void(void)> fn) {
    changeProperty(MEDIAPLAYER2, "CanQuit", sdbus::Variant(true));
    quit_fn = fn;
}

void services::Mpris::onNext(std::function<void(void)> fn) {
    changePropertyControlled({"CanGoNext"});
    next_fn = fn;
}

void services::Mpris::onPrevious(std::function<void(void)> fn) {
    changePropertyControlled({"CanGoPrevious"});
    previous_fn = fn;
}

void services::Mpris::onPause(std::function<void(void)> fn) {
    changePropertyControlled({"CanPause"});
    pause_fn = fn;
}

void services::Mpris::onToggle(std::function<void(void)> fn) {
    changePropertyControlled({"CanPlay", "CanPause"});
    play_pause_fn = fn;
}

void services::Mpris::onStop(std::function<void(void)> fn) {
    changePropertyControlled({
        "CanGoNext",
        "CanGoPrevious",
        "CanPause",
        "CanPlay",
        "CanSeek"
    });
    stop_fn = fn;
}

void services::Mpris::onPlay(std::function<void(void)> fn) {
    changePropertyControlled({"CanPlay"});
    play_fn = fn;
}

void services::Mpris::onSeek(std::function<void(int64_t)> fn) {
    changePropertyControlled({"CanSeek"});
    seek_fn = fn;
}

void services::Mpris::onSetPosition(std::function<void(int64_t)> fn) {
    changePropertyControlled({"CanSeek"});
    setPosition_fn = fn;
}

void services::Mpris::onLoopStatusChanged(std::function<void(LoopStatus)> fn) {
    changePropertyControlled({
        "CanGoNext",
        "CanGoPrevious",
        "CanPause",
        "CanPlay",
        "CanSeek"
    });
    loop_status_changed_fn = fn;
}

void services::Mpris::onShuffleChanged(std::function<void(bool)> fn) {
    changePropertyControlled({
        "CanGoNext",
        "CanGoPrevious",
        "CanPause",
        "CanPlay",
        "CanSeek"
    });
    shuffle_changed_fn = fn;
}

void services::Mpris::setHumanName(std::string_view value) {
    human_name = value;
    changeProperty(MEDIAPLAYER2,  "Identity", sdbus::Variant(human_name));
}

void services::Mpris::setPlaybackStatus(PlaybackStatus value) {
    playback_status = value;
    changeProperty(MEDIAPLAYER2PLAYER, "PlaybackStatus", sdbus::Variant(detail::playbackStatusToString(playback_status)));
}

void services::Mpris::setLoopStatus(LoopStatus value) {
    loop_status = value;
    changeProperty(MEDIAPLAYER2PLAYER, "LoopStatus", sdbus::Variant(detail::loopStatusToString(loop_status)));
}

void services::Mpris::setShuffle(bool value) {
    shuffle = value;
    changeProperty(MEDIAPLAYER2PLAYER, "Shuffle", sdbus::Variant(shuffle));
}

void services::Mpris::setPosition(int64_t value) {
    position = value;
}

void services::Mpris::setMetadata(const std::map<Field, sdbus::Variant> &value) {
    metadata.clear();

    for (auto [k, v] : value)
        metadata[detail::fieldToString(k)] = v;

    changeProperty(MEDIAPLAYER2PLAYER, "Metadata", sdbus::Variant(metadata));
}

void services::Mpris::changeProperty(const std::string &interface, const std::string &name, sdbus::Variant value)
{
    std::map<std::string, sdbus::Variant> d;
    d[name] = value;
    object->emitSignal("PropertiesChanged").onInterface("org.freedesktop.DBus.Properties").withArguments(interface, d, std::vector<std::string>{});
}

void services::Mpris::changePropertyControlled(std::vector<std::string> args)
{
    auto f = [&] (std::string_view name) {
        if (name == "CanGoNext")     return canGoNext();
        if (name == "CanGoPrevious") return canGoPrevious();
        if (name == "CanPause")      return canPause();
        if (name == "CanPlay")       return canPlay();
        if (name == "CanSeek")       return canSeek();
        return false;
    };

    std::map<std::string, sdbus::Variant> d;
    for (const auto &v : args) {
        d[v] = sdbus::Variant(f(v));
    }

    object->emitSignal("PropertiesChanged")
          .onInterface("org.freedesktop.DBus.Properties")
          .withArguments(MEDIAPLAYER2PLAYER, d, std::vector<std::string>{});
}

void services::Mpris::setLoopStatusExternal(const std::string &value)
{
    for (auto i = 0u; i < std::size(loop_status_strings); i++) {
        if (value == loop_status_strings[i]) {
            if (!canControl())
                throw sdbus::Error(sdbus::Error::Name{service_name + ".Error"}, "Cannot set loop status (CanControl is false).");
            setLoopStatus(static_cast<LoopStatus>(i));
            loop_status_changed_fn(loop_status);
        }
    }
}

void services::Mpris::setShuffleExternal(bool value)
{
    if (!canControl())
        throw sdbus::Error(sdbus::Error::Name{service_name + ".Error"}, "Cannot set shuffle (CanControl is false).");
    setShuffle(value);
    shuffle_changed_fn(shuffle);
}

void services::Mpris::setPositionMethod(sdbus::ObjectPath id, int64_t pos)
{
    if (!canSeek()) {
        spdlog::warn("MPRIS: SetPosition refused, CanSeek is false");
        return;
    }

    auto tid = metadata.find(detail::fieldToString(Field::TrackId));
    if (tid == metadata.end() || tid->second.get<std::string>() != id) {
        // A stale track id here means the caller raced a track change.
        spdlog::warn("MPRIS: SetPosition ignored, track id does not match the current track");
        return;
    }

    spdlog::info("MPRIS: SetPosition {}", pos);
    setPosition_fn(pos);
}

std::unique_ptr<services::Mpris> services::Mpris::make(std::string_view name)
{
    try {
        auto s = std::make_unique<Mpris>(name);
        return s;
    } catch (const sdbus::Error &error) {
        // Swallowing this left PLAYER reporting "mpris service initialisation
        // failed" with no way to tell whether the bus, the name or the object
        // was the problem.
        spdlog::error("MPRIS: could not claim {} on the session bus, {}", PREFIX + std::string(name), error.what());
        return nullptr;
    }
}

services::Mpris::Mpris(std::string_view name) : service_name(PREFIX + std::string(name)) {
    connection = sdbus::createSessionBusConnection();
    connection->requestName(sdbus::ServiceName{service_name});
    object = sdbus::createObject(*connection, sdbus::ObjectPath{OBJECT_PATH});

    #define M(f) detail::member_fn(this, &Mpris::f)

    object->addVTable(
        sdbus::registerMethod("Quit")              .implementedAs([&] { dispatch("Quit", bool(quit_fn), [&]{ quit_fn(); }); }),

        sdbus::registerProperty("CanQuit")       .withGetter([&] { return bool(quit_fn); }),
        sdbus::registerProperty("HasTrackList")  .withGetter([&] { return false; }),
        sdbus::registerProperty("Identity")      .withGetter([&] { return human_name; })
    ).forInterface(MEDIAPLAYER2);

    object->addVTable(
        sdbus::registerMethod("Next")              .implementedAs([&] { dispatch("Next",      canGoNext(),                [&]{ next_fn();       }); }),
        sdbus::registerMethod("Previous")          .implementedAs([&] { dispatch("Previous",  canGoPrevious(),            [&]{ previous_fn();   }); }),
        sdbus::registerMethod("Pause")             .implementedAs([&] { dispatch("Pause",     canPause(),                 [&]{ pause_fn();      }); }),
        sdbus::registerMethod("PlayPause")         .implementedAs([&] { dispatch("PlayPause", canPlay() || canPause(),    [&]{ play_pause_fn(); }); }),
        sdbus::registerMethod("Stop")              .implementedAs([&] { dispatch("Stop",      canControl(),               [&]{ stop_fn();       }); }),
        sdbus::registerMethod("Play")              .implementedAs([&] { dispatch("Play",      canPlay(),                  [&]{ play_fn();       }); }),

        sdbus::registerMethod("Seek")              .implementedAs([&] (int64_t n) { dispatch("Seek", canSeek(), [&]{ seek_fn(n); }); }) .withInputParamNames("Offset"),
        sdbus::registerMethod("SetPosition")       .implementedAs(M(setPositionMethod))                                  .withInputParamNames("TrackId", "Position"),

        sdbus::registerProperty("PlaybackStatus").withGetter([&] { return detail::playbackStatusToString(playback_status); }),
        sdbus::registerProperty("LoopStatus")    .withGetter([&] { return detail::loopStatusToString(loop_status); }).withSetter(M(setLoopStatusExternal)),
        sdbus::registerProperty("Shuffle")       .withGetter([&] { return shuffle; }).withSetter(M(setShuffleExternal)),
        sdbus::registerProperty("Metadata")      .withGetter([&] { return metadata; }),
        sdbus::registerProperty("Position")      .withGetter([&] { return position; }),
        sdbus::registerProperty("CanGoNext")     .withGetter(M(canGoNext)),
        sdbus::registerProperty("CanGoPrevious") .withGetter(M(canGoPrevious)),
        sdbus::registerProperty("CanPlay")       .withGetter(M(canPlay)),
        sdbus::registerProperty("CanPause")      .withGetter(M(canPause)),
        sdbus::registerProperty("CanSeek")       .withGetter(M(canSeek)),
        sdbus::registerProperty("CanControl")    .withGetter(M(canControl)),

        sdbus::registerSignal("Seeked").withParameters<int64_t>("Position")
    ).forInterface(MEDIAPLAYER2PLAYER);

    #undef M
}

void services::Mpris::startLoop() {
    connection->enterEventLoop();
}

void services::Mpris::startLoopAsync() {
    connection->enterEventLoopAsync();
}

void services::Mpris::sendSeekedSignal(int64_t position) {
    object->emitSignal("Seeked").onInterface(MEDIAPLAYER2PLAYER).withArguments(position);
}

void services::Mpris::setIsNextPossible(bool possible) {
    this->is_next_possible = possible;
    this->changePropertyControlled({"CanGoNext"});
}

void services::Mpris::setIsPreviousPossible(bool possible) {
    this->is_previous_possible = possible;
    this->changePropertyControlled({"CanGoPrevious"});
}

void services::Mpris::updatePlayerControls() {
    this->changePropertyControlled({
        "CanGoNext",
        "CanGoPrevious"
    });
}
