#include "../includes/embeddedhandlerregistry.h"
#include "../includes/embeddedparser.h"

#include <functional>
#include <unordered_map>

namespace {

using HandlerFactory = std::function<std::unique_ptr<JsonBagHandler>()>;

const std::unordered_map<std::string, HandlerFactory>& serverHandlers() {
    static const std::unordered_map<std::string, HandlerFactory> handlers = {
        {"register_ack",              [] { return std::make_unique<RegisterAckHandler>(); }},
        {"update_embedded_info",      [] { return std::make_unique<UpdateEmbeddedInfoHandler>(); }},
        {"push_resources_to_download",[] { return std::make_unique<PushResourcesHandler>(); }},
        {"push_schedule_playlist",    [] { return std::make_unique<PushScheduleHandler>(); }},
        {"request_screenshot",        [] { return std::make_unique<ScreenshotRequestHandler>(); }},
        {"ota_update",                [] { return std::make_unique<OtaUpdateHandler>(); }},
    };
    return handlers;
}

}  // namespace

std::unique_ptr<JsonBagHandler> EmbeddedHandlerRegistry::create(const std::string& source,
                                                                const std::string& cmd) {
    if (source != "server") {
        return nullptr;
    }
    auto it = serverHandlers().find(cmd);
    if (it == serverHandlers().end()) {
        return nullptr;
    }
    return it->second();
}
