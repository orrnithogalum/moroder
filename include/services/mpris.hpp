/* MPRIS
- Provides an interface to sdbus-c++ so that the user can control the player
  remotely from other applications.

- This implementation is based on code from: https://github.com/chrg127/mpris-server and has been modified for this application's use case.
- Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*/

#include <sdbus-c++/sdbus-c++.h>
#include <string>
#include <vector>

namespace services {

using namespace std::literals::string_literals;

using StringList = std::vector<std::string>;
using Metadata   = std::map<std::string, sdbus::Variant>;

static const auto PROPERTIES = "org.freedesktop.DBus.Properties"s;
static const auto MEDIAPLAYER2PLAYER = "org.mpris.MediaPlayer2.Player"s;
static const auto MEDIAPLAYER2 = "org.mpris.MediaPlayer2"s;
static const auto OBJECT_PATH = "/org/mpris/MediaPlayer2"s;
static const auto PREFIX = "org.mpris.MediaPlayer2."s;

enum class PlaybackStatus {
    Playing,
    Paused,
    Stopped
};

enum class LoopStatus {
    None,
    Track,
    Playlist
};

enum class Field {
    TrackId     , Length     , ArtUrl      , Album          ,
    AlbumArtist , Artist     , AsText      , AudioBPM       ,
    AutoRating  , Comment    , Composer    , ContentCreated ,
    DiscNumber  , FirstUsed  , Genre       , LastUsed       ,
    Lyricist    , Title      , TrackNumber , Url            ,
    UseCount    , UserRating
};

static const char *playback_status_strings[] = {
    "Playing",
    "Paused",
    "Stopped"
};

static const char *loop_status_strings[] = {
    "None",
    "Track",
    "Playlist"
};

static const char *metadata_strings[] = { 
    "mpris:trackid"     , "mpris:length"        , "mpris:artUrl"       , "xesam:album"           ,
    "xesam:albumArtist" , "xesam:artist"        , "xesam:asText"       , "xesam:audioBPM"        ,
    "xesam:autoRating"  , "xesam:comment"       , "xesam:composer"    , "xesam:contentCreated" ,
    "xesam:discNumber" , "xesam:firstUsed"    , "xesam:genre"       , "xesam:lastUsed"       ,
    "xesam:lyricist"   , "xesam:title"        , "xesam:trackNumber" , "xesam:url"            ,
    "xesam:useCount"   , "xesam:userRating"
};

namespace detail {

template <typename T, typename R, typename... Args> std::function<R(Args...)> member_fn(T *obj, R (T::*fn)(Args...)) {
    return [=](Args&&... args) -> R { return (obj->*fn)(args...); };
}

template <typename T, typename R, typename... Args> std::function<R(Args...)> member_fn(T *obj, R (T::*fn)(Args...) const) {
    return [=](Args&&... args) -> R { return (obj->*fn)(args...); };
}

inline std::string playbackStatusToString(PlaybackStatus status) {
    return playback_status_strings[static_cast<int>(status)];
}

inline std::string loopStatusToString(LoopStatus status) {
    return loop_status_strings[static_cast<int>(status)];
}

inline std::string fieldToString(Field entry) { 
    return metadata_strings[static_cast<int>(entry)];
}

}

class Mpris {
private:
    std::unique_ptr<sdbus::IConnection> connection;
    std::unique_ptr<sdbus::IObject> object;

    std::function<void(void)> quit_fn;
    std::function<void(void)> next_fn;
    std::function<void(void)> previous_fn;
    std::function<void(void)> pause_fn;
    std::function<void(void)> play_pause_fn;
    std::function<void(void)> stop_fn;
    std::function<void(void)> play_fn;
    
    std::function<void(LoopStatus)> loop_status_changed_fn;
    std::function<void(int64_t)> setPosition_fn;
    std::function<void(int64_t)> seek_fn;

    std::function<void(bool)> shuffle_changed_fn;

    std::string service_name;
    std::string human_name;

    PlaybackStatus playback_status = PlaybackStatus::Stopped;
    LoopStatus loop_status = LoopStatus::None;
    Metadata metadata = {};

    int64_t position = 0;

    bool shuffle = false;

    void changeProperty(const std::string &interface, const std::string &name, sdbus::Variant value);
    void changePropertyControlled(std::vector<std::string> args);

    bool canGoPrevious() const;
    bool canControl() const;
    bool canGoNext() const;
    bool canPause() const;
    bool canPlay() const;
    bool canSeek() const;

    void setPositionMethod(sdbus::ObjectPath id, int64_t pos);
    void setLoopStatusExternal(const std::string &value);
    void setShuffleExternal(bool value);

public:
    static std::unique_ptr<Mpris> make(std::string_view name);

    explicit Mpris(std::string_view player_name);
    
    void startLoopAsync();
    void startLoop();

    void setMetadata(const std::map<Field, sdbus::Variant> &value);
    void setPlaybackStatus(PlaybackStatus value);

    void onLoopStatusChanged(std::function<void(LoopStatus)> fn);
    void onShuffleChanged(std::function<void(bool)> fn);

    void onPrevious(std::function<void(void)> fn);
    void onToggle(std::function<void(void)> fn);
    void onPause(std::function<void(void)> fn);
    void onNext(std::function<void(void)> fn);
    void onStop(std::function<void(void)> fn);
    void onPlay(std::function<void(void)> fn);
    void onQuit(std::function<void(void)> fn);

    void onSetPosition(std::function<void(int64_t)> fn);
    void onSeek(std::function<void(int64_t)> fn);

    void setHumanName(std::string_view value);
    void setLoopStatus(LoopStatus value);
    void setPosition(int64_t value);
    void setShuffle(bool value);

    void sendSeekedSignal(int64_t position);
};

}