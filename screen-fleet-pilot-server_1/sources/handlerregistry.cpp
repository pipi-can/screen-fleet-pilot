#include "../includes/handlerregistry.h"
#include "../includes/serverparser.h"

#include <functional>
#include <unordered_map>

namespace {

using HandlerFactory = std::function<std::unique_ptr<JsonBagHandler>()>;

const std::unordered_map<std::string, HandlerFactory>& embeddedHandlers() {
    static const std::unordered_map<std::string, HandlerFactory> handlers = {
        {"register",        [] { return std::make_unique<RegisterHandler>(); }},
        {"heartbeat",       [] { return std::make_unique<EmbeddedHeartbeatHandler>(); }},
        {"update_info_ack", [] { return std::make_unique<UpdateInfoAckHandler>(); }},
        {"screenshot_data", [] { return std::make_unique<EmbeddedScreenshotDataHandler>(); }},
        {"ota_update_ack",  [] { return std::make_unique<EmbeddedOtaUpdateAckHandler>(); }},
    };
    return handlers;
}

const std::unordered_map<std::string, HandlerFactory>& clientHandlers() {
    static const std::unordered_map<std::string, HandlerFactory> handlers = {
        {"register",                      [] { return std::make_unique<RegisterHandler>(); }},
        {"heartbeat",                     [] { return std::make_unique<ClientHeartbeatHandler>(); }},
        {"fetch_devices",                 [] { return std::make_unique<FetchDeviceHandler>(); }},
        {"request_update_embedded",       [] { return std::make_unique<RequestUpdateEmbeddedHandler>(); }},
        {"request_file_list",             [] { return std::make_unique<RequestFileListHandler>(); }},
        {"mask_device",                   [] { return std::make_unique<MaskDeviceHandler>(); }},
        {"request_screenshot",            [] { return std::make_unique<ClientRequestScreenshotHandler>(); }},
        {"request_push_content_to_embedded", [] { return std::make_unique<RequestPushContentHandler>(); }},
        {"request_schedule_push",         [] { return std::make_unique<RequestSchedulePushHandler>(); }},
        {"request_firmware_list",         [] { return std::make_unique<RequestFirmwareListHandler>(); }},
        {"request_ota_update",            [] { return std::make_unique<RequestOtaUpdateHandler>(); }},
        {"request_check_firmware",        [] { return std::make_unique<RequestCheckFirmwareHandler>(); }},
    };
    return handlers;
}

std::unique_ptr<JsonBagHandler> lookup(const std::unordered_map<std::string, HandlerFactory>& handlers,
                                       const std::string& cmd) {
    auto it = handlers.find(cmd);
    if (it == handlers.end()) {
        return nullptr;
    }
    return it->second();
}

}  // namespace

std::unique_ptr<JsonBagHandler> HandlerRegistry::create(const std::string& source, const std::string& cmd) {
    if (source == "embedded") {
        return lookup(embeddedHandlers(), cmd);
    }
    if (source == "client") {
        return lookup(clientHandlers(), cmd);
    }
    return nullptr;
}
