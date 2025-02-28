// SPDX-FileCopyrightText: Nheko Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "EventAccessors.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <concepts>
#include <type_traits>

namespace {
struct IsStateEvent
{
    template<class T>
    bool operator()(const mtx::events::StateEvent<T> &)
    {
        return true;
    }
    template<class T>
    bool operator()(const mtx::events::Event<T> &)
    {
        return false;
    }
};

struct EventMsgType
{
    template<class T>
    mtx::events::MessageType operator()(const mtx::events::Event<T> &e)
    {
        if constexpr (requires(decltype(e) t) { t.content.msgtype.value(); })
            return mtx::events::getMessageType(e.content.msgtype.value());
        else if constexpr (requires(decltype(e) t) { std::string{t.content.msgtype}; })
            return mtx::events::getMessageType(e.content.msgtype);
        return mtx::events::MessageType::Unknown;
    }
};

struct EventRoomName
{
    template<class T>
    std::string operator()(const T &e)
    {
        if constexpr (std::is_same_v<mtx::events::StateEvent<mtx::events::state::Name>, T>)
            return e.content.name;
        return "";
    }
};

struct EventRoomTopic
{
    template<class T>
    std::string operator()(const T &e)
    {
        if constexpr (std::is_same_v<mtx::events::StateEvent<mtx::events::state::Topic>, T>)
            return e.content.topic;
        return "";
    }
};

struct CallType
{
    template<class T>
    std::string operator()(const T &e)
    {
        if constexpr (std::is_same_v<mtx::events::RoomEvent<mtx::events::voip::CallInvite>, T>) {
            const char video[]     = "m=video";
            const std::string &sdp = e.content.offer.sdp;
            return std::search(sdp.cbegin(),
                               sdp.cend(),
                               std::cbegin(video),
                               std::cend(video) - 1,
                               [](unsigned char c1, unsigned char c2) {
                                   return std::tolower(c1) == std::tolower(c2);
                               }) != sdp.cend()
                     ? "video"
                     : "voice";
        }
        return "";
    }
};

struct EventBody
{
    template<class T>
    std::string operator()(const mtx::events::Event<T> &e)
    {
        if constexpr (requires(decltype(e) t) { t.content.body.value(); })
            return e.content.body ? e.content.body.value() : "";
        else if constexpr (requires(decltype(e) t) { std::string{t.content.body}; })
            return e.content.body;
        return "";
    }
};

struct EventFormattedBody
{
    template<class T>
    std::string operator()(const mtx::events::RoomEvent<T> &e)
    {
        if constexpr (requires { T::formatted_body; }) {
            if (e.content.format == "org.matrix.custom.html")
                return e.content.formatted_body;
        }
        return "";
    }
};

struct EventFile
{
    template<class T>
    std::optional<mtx::crypto::EncryptedFile> operator()(const mtx::events::Event<T> &e)
    {
        if constexpr (requires { T::file; })
            return e.content.file;
        return std::nullopt;
    }
};

struct EventThumbnailFile
{
    template<class T>
    std::optional<mtx::crypto::EncryptedFile> operator()(const mtx::events::Event<T> &e)
    {
        if constexpr (requires { T::thumbnail_file; })
            return e.content.info.thumbnail_file;
        return std::nullopt;
    }
};

struct EventUrl
{
    template<class T>
    std::string operator()(const mtx::events::Event<T> &e)
    {
        if constexpr (requires { T::url; }) {
            if (auto file = EventFile{}(e))
                return file->url;
            return e.content.url;
        }
        return "";
    }
};

struct EventThumbnailUrl
{
    template<class T>
    std::string operator()(const mtx::events::Event<T> &e)
    {
        if constexpr (requires { e.content.info.thumbnail_url; }) {
            if (auto file = EventThumbnailFile{}(e))
                return file->url;
            return e.content.info.thumbnail_url;
        }
        return "";
    }
};

struct EventDuration
{
    template<class T>
    uint64_t operator()(const mtx::events::Event<T> &e)
    {
        if constexpr (requires { e.content.info.duration; }) {
            return e.content.info.duration;
        }
        return 0;
    }
};

struct EventBlurhash
{
    template<class T>
    std::string operator()(const mtx::events::Event<T> &e)
    {
        if constexpr (requires { e.content.info.blurhash; }) {
            return e.content.info.blurhash;
        }
        return "";
    }
};

struct EventFilename
{
    template<class T>
    std::string operator()(const mtx::events::Event<T> &)
    {
        return "";
    }
    std::string operator()(const mtx::events::RoomEvent<mtx::events::msg::Audio> &e)
    {
        // body may be the original filename
        return e.content.body;
    }
    std::string operator()(const mtx::events::RoomEvent<mtx::events::msg::Video> &e)
    {
        // body may be the original filename
        return e.content.body;
    }
    std::string operator()(const mtx::events::RoomEvent<mtx::events::msg::Image> &e)
    {
        // body may be the original filename
        return e.content.body;
    }
    std::string operator()(const mtx::events::RoomEvent<mtx::events::msg::File> &e)
    {
        // body may be the original filename
        if (!e.content.filename.empty())
            return e.content.filename;
        return e.content.body;
    }
};

struct EventMimeType
{
    template<class T>
    std::string operator()(const mtx::events::Event<T> &e)
    {
        if constexpr (requires { e.content.info.mimetype; }) {
            return e.content.info.mimetype;
        }
        return "";
    }
};

struct EventFilesize
{
    template<class T>
    int64_t operator()(const mtx::events::RoomEvent<T> &e)
    {
        if constexpr (requires { e.content.info.size; }) {
            return e.content.info.size;
        }
        return 0;
    }
};

struct EventRelations
{
    inline const static mtx::common::Relations empty;

    template<class T>
    const mtx::common::Relations &operator()(const mtx::events::Event<T> &e)
    {
        if constexpr (requires { T::relations; }) {
            return e.content.relations;
        }
        return empty;
    }
};

struct SetEventRelations
{
    mtx::common::Relations new_relations;
    template<class T>
    void operator()(mtx::events::Event<T> &e)
    {
        if constexpr (requires { T::relations; }) {
            e.content.relations = std::move(new_relations);
        }
    }
};

struct EventTransactionId
{
    template<class T>
    std::string operator()(const mtx::events::RoomEvent<T> &e)
    {
        return e.unsigned_data.transaction_id;
    }
    template<class T>
    std::string operator()(const mtx::events::Event<T> &e)
    {
        return e.unsigned_data.transaction_id;
    }
};

struct EventMediaHeight
{
    template<class T>
    uint64_t operator()(const mtx::events::Event<T> &e)
    {
        if constexpr (requires { e.content.info.h; }) {
            return e.content.info.h;
        }
        return -1;
    }
};

struct EventMediaWidth
{
    template<class T>
    uint64_t operator()(const mtx::events::Event<T> &e)
    {
        if constexpr (requires { e.content.info.h; }) {
            return e.content.info.w;
        }
        return -1;
    }
};

template<class T>
double
eventPropHeight(const mtx::events::RoomEvent<T> &e)
{
    auto w = eventWidth(e);
    if (w == 0)
        w = 1;

    double prop = eventHeight(e) / (double)w;

    return prop > 0 ? prop : 1.;
}
}

const std::string &
mtx::accessors::event_id(const mtx::events::collections::TimelineEvents &event)
{
    return std::visit([](const auto &e) -> const std::string & { return e.event_id; }, event);
}
const std::string &
mtx::accessors::room_id(const mtx::events::collections::TimelineEvents &event)
{
    return std::visit([](const auto &e) -> const std::string & { return e.room_id; }, event);
}

const std::string &
mtx::accessors::sender(const mtx::events::collections::TimelineEvents &event)
{
    return std::visit([](const auto &e) -> const std::string & { return e.sender; }, event);
}

QDateTime
mtx::accessors::origin_server_ts(const mtx::events::collections::TimelineEvents &event)
{
    return QDateTime::fromMSecsSinceEpoch(
      std::visit([](const auto &e) { return e.origin_server_ts; }, event));
}

std::string
mtx::accessors::filename(const mtx::events::collections::TimelineEvents &event)
{
    return std::visit(EventFilename{}, event);
}

mtx::events::MessageType
mtx::accessors::msg_type(const mtx::events::collections::TimelineEvents &event)
{
    return std::visit(EventMsgType{}, event);
}
std::string
mtx::accessors::room_name(const mtx::events::collections::TimelineEvents &event)
{
    return std::visit(EventRoomName{}, event);
}
std::string
mtx::accessors::room_topic(const mtx::events::collections::TimelineEvents &event)
{
    return std::visit(EventRoomTopic{}, event);
}

std::string
mtx::accessors::call_type(const mtx::events::collections::TimelineEvents &event)
{
    return std::visit(CallType{}, event);
}

std::string
mtx::accessors::body(const mtx::events::collections::TimelineEvents &event)
{
    return std::visit(EventBody{}, event);
}

std::string
mtx::accessors::formatted_body(const mtx::events::collections::TimelineEvents &event)
{
    return std::visit(EventFormattedBody{}, event);
}

QString
mtx::accessors::formattedBodyWithFallback(const mtx::events::collections::TimelineEvents &event)
{
    auto formatted = formatted_body(event);
    if (!formatted.empty())
        return QString::fromStdString(formatted);
    else
        return QString::fromStdString(body(event))
          .toHtmlEscaped()
          .replace(QLatin1String("\n"), QLatin1String("<br>"));
}

std::optional<mtx::crypto::EncryptedFile>
mtx::accessors::file(const mtx::events::collections::TimelineEvents &event)
{
    return std::visit(EventFile{}, event);
}

std::optional<mtx::crypto::EncryptedFile>
mtx::accessors::thumbnail_file(const mtx::events::collections::TimelineEvents &event)
{
    return std::visit(EventThumbnailFile{}, event);
}

std::string
mtx::accessors::url(const mtx::events::collections::TimelineEvents &event)
{
    return std::visit(EventUrl{}, event);
}
std::string
mtx::accessors::thumbnail_url(const mtx::events::collections::TimelineEvents &event)
{
    return std::visit(EventThumbnailUrl{}, event);
}
uint64_t
mtx::accessors::duration(const mtx::events::collections::TimelineEvents &event)
{
    return std::visit(EventDuration{}, event);
}
std::string
mtx::accessors::blurhash(const mtx::events::collections::TimelineEvents &event)
{
    return std::visit(EventBlurhash{}, event);
}
std::string
mtx::accessors::mimetype(const mtx::events::collections::TimelineEvents &event)
{
    return std::visit(EventMimeType{}, event);
}
const mtx::common::Relations &
mtx::accessors::relations(const mtx::events::collections::TimelineEvents &event)
{
    return std::visit(EventRelations{}, event);
}

void
mtx::accessors::set_relations(mtx::events::collections::TimelineEvents &event,
                              mtx::common::Relations relations)
{
    std::visit(SetEventRelations{std::move(relations)}, event);
}

std::string
mtx::accessors::transaction_id(const mtx::events::collections::TimelineEvents &event)
{
    return std::visit(EventTransactionId{}, event);
}

int64_t
mtx::accessors::filesize(const mtx::events::collections::TimelineEvents &event)
{
    return std::visit(EventFilesize{}, event);
}

uint64_t
mtx::accessors::media_height(const mtx::events::collections::TimelineEvents &event)
{
    return std::visit(EventMediaHeight{}, event);
}

uint64_t
mtx::accessors::media_width(const mtx::events::collections::TimelineEvents &event)
{
    return std::visit(EventMediaWidth{}, event);
}

nlohmann::json
mtx::accessors::serialize_event(const mtx::events::collections::TimelineEvents &event)
{
    return std::visit([](const auto &e) { return nlohmann::json(e); }, event);
}

bool
mtx::accessors::is_state_event(const mtx::events::collections::TimelineEvents &event)
{
    return std::visit(IsStateEvent{}, event);
}
