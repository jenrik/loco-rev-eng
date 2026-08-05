#ifndef _WIN32
#if defined(__linux__) && !defined(__ANDROID__)

#include "avahi_dbus_discovery.h"
#include "network_discovery_protocol.h"

#include <dbus/dbus.h>

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace lego_loco::network {
namespace {

constexpr char kAvahiName[] = "org.freedesktop.Avahi";
constexpr char kServerPath[] = "/";
constexpr char kServerInterface[] = "org.freedesktop.Avahi.Server";
constexpr char kServer2Interface[] = "org.freedesktop.Avahi.Server2";
constexpr char kEntryGroupInterface[] = "org.freedesktop.Avahi.EntryGroup";
constexpr char kServiceBrowserInterface[] = "org.freedesktop.Avahi.ServiceBrowser";
constexpr char kServiceResolverInterface[] = "org.freedesktop.Avahi.ServiceResolver";
constexpr char kServiceType[] = "_legoloco._tcp";
constexpr char kDomain[] = "local";
constexpr int kCallTimeoutMs = 1000;
constexpr std::int32_t kInterfaceUnspecified = -1;
constexpr std::int32_t kProtocolUnspecified = -1;
constexpr std::uint32_t kNoFlags = 0;
constexpr std::int32_t kServerCollision = 3;
constexpr std::int32_t kServerFailure = 4;
constexpr std::int32_t kEntryGroupCollision = 3;
constexpr std::int32_t kEntryGroupFailure = 4;

using AppendArguments = std::function<bool(DBusMessageIter&)>;

class ScopedDbusError {
public:
    ScopedDbusError() { dbus_error_init(&error_); }
    ~ScopedDbusError() { dbus_error_free(&error_); }

    ScopedDbusError(const ScopedDbusError&) = delete;
    ScopedDbusError& operator=(const ScopedDbusError&) = delete;

    DBusError* get() { return &error_; }
    bool set() const { return dbus_error_is_set(&error_); }
    std::string message() const {
        if (!set()) {
            return {};
        }
        std::string result = error_.name ? error_.name : "D-Bus error";
        if (error_.message && *error_.message) {
            result += ": ";
            result += error_.message;
        }
        return result;
    }

private:
    DBusError error_{};
};

struct MessageUnref {
    void operator()(DBusMessage* message) const {
        if (message) {
            dbus_message_unref(message);
        }
    }
};
using MessagePtr = std::unique_ptr<DBusMessage, MessageUnref>;

std::string ReplyError(DBusMessage* reply) {
    if (!reply || dbus_message_get_type(reply) != DBUS_MESSAGE_TYPE_ERROR) {
        return {};
    }
    std::string result = dbus_message_get_error_name(reply)
        ? dbus_message_get_error_name(reply)
        : "D-Bus method error";
    DBusMessageIter iter;
    if (dbus_message_iter_init(reply, &iter) &&
        dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_STRING) {
        const char* detail = nullptr;
        dbus_message_iter_get_basic(&iter, &detail);
        if (detail && *detail) {
            result += ": ";
            result += detail;
        }
    }
    return result;
}

bool AppendBasic(DBusMessageIter& iter, int type, const void* value) {
    return dbus_message_iter_append_basic(&iter, type, value) != FALSE;
}

bool AppendTxtArray(DBusMessageIter& iter, const std::vector<std::string>& txt) {
    DBusMessageIter outer;
    if (!dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "ay", &outer)) {
        return false;
    }
    for (const std::string& entry : txt) {
        DBusMessageIter bytes;
        if (!dbus_message_iter_open_container(&outer, DBUS_TYPE_ARRAY, "y", &bytes)) {
            return false;
        }
        if (!entry.empty()) {
            const unsigned char* data =
                reinterpret_cast<const unsigned char*>(entry.data());
            const int length = static_cast<int>(entry.size());
            if (!dbus_message_iter_append_fixed_array(
                    &bytes, DBUS_TYPE_BYTE, &data, length)) {
                return false;
            }
        }
        if (!dbus_message_iter_close_container(&outer, &bytes)) {
            return false;
        }
    }
    return dbus_message_iter_close_container(&iter, &outer) != FALSE;
}

MessagePtr NewMethodCall(
    const char* path,
    const char* interface,
    const char* method,
    const AppendArguments& append,
    std::string& error) {
    MessagePtr message(dbus_message_new_method_call(kAvahiName, path, interface, method));
    if (!message) {
        error = "out of memory creating D-Bus method call";
        return {};
    }
    if (append) {
        DBusMessageIter iter;
        dbus_message_iter_init_append(message.get(), &iter);
        if (!append(iter)) {
            error = std::string("failed to encode D-Bus arguments for ") + method;
            return {};
        }
    }
    return message;
}

MessagePtr Call(
    DBusConnection* connection,
    const char* path,
    const char* interface,
    const char* method,
    const AppendArguments& append,
    std::string& error) {
    MessagePtr message = NewMethodCall(path, interface, method, append, error);
    if (!message) {
        return {};
    }

    ScopedDbusError dbus_error;
    MessagePtr reply(dbus_connection_send_with_reply_and_block(
        connection, message.get(), kCallTimeoutMs, dbus_error.get()));
    if (!reply) {
        error = dbus_error.set() ? dbus_error.message()
                                 : std::string("no D-Bus reply from ") + method;
        return {};
    }
    const std::string reply_error = ReplyError(reply.get());
    if (!reply_error.empty()) {
        error = reply_error;
        return {};
    }
    return reply;
}

DBusPendingCall* SendAsync(
    DBusConnection* connection,
    const char* path,
    const char* interface,
    const char* method,
    const AppendArguments& append,
    std::string& error) {
    MessagePtr message = NewMethodCall(path, interface, method, append, error);
    if (!message) {
        return nullptr;
    }
    DBusPendingCall* pending = nullptr;
    if (!dbus_connection_send_with_reply(
            connection, message.get(), &pending, kCallTimeoutMs) || !pending) {
        error = std::string("failed to queue D-Bus call ") + method;
        return nullptr;
    }
    dbus_connection_flush(connection);
    return pending;
}

void SendNoReply(
    DBusConnection* connection,
    const std::string& path,
    const char* interface,
    const char* method) {
    if (!connection || path.empty()) {
        return;
    }
    MessagePtr message(dbus_message_new_method_call(
        kAvahiName, path.c_str(), interface, method));
    if (!message) {
        return;
    }
    dbus_message_set_no_reply(message.get(), TRUE);
    dbus_connection_send(connection, message.get(), nullptr);
}

bool ReadObjectPath(DBusMessage* reply, std::string& path, std::string& error) {
    DBusMessageIter iter;
    if (!dbus_message_iter_init(reply, &iter) ||
        dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_OBJECT_PATH) {
        error = "Avahi returned an invalid object path reply";
        return false;
    }
    const char* value = nullptr;
    dbus_message_iter_get_basic(&iter, &value);
    if (!value || !*value) {
        error = "Avahi returned an empty object path";
        return false;
    }
    path = value;
    return true;
}

bool ParseTxtArray(DBusMessageIter& iter, std::vector<std::string>& result) {
    if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_ARRAY ||
        dbus_message_iter_get_element_type(&iter) != DBUS_TYPE_ARRAY) {
        return false;
    }
    DBusMessageIter outer;
    dbus_message_iter_recurse(&iter, &outer);
    while (dbus_message_iter_get_arg_type(&outer) != DBUS_TYPE_INVALID) {
        if (dbus_message_iter_get_arg_type(&outer) != DBUS_TYPE_ARRAY ||
            dbus_message_iter_get_element_type(&outer) != DBUS_TYPE_BYTE) {
            return false;
        }
        DBusMessageIter bytes;
        dbus_message_iter_recurse(&outer, &bytes);
        unsigned char* data = nullptr;
        int length = 0;
        if (dbus_message_iter_get_arg_type(&bytes) != DBUS_TYPE_INVALID) {
            if (dbus_message_iter_get_arg_type(&bytes) != DBUS_TYPE_BYTE) {
                return false;
            }
            dbus_message_iter_get_fixed_array(&bytes, &data, &length);
        }
        result.emplace_back(
            data ? reinterpret_cast<const char*>(data) : "",
            static_cast<std::size_t>(length));
        dbus_message_iter_next(&outer);
    }
    return true;
}

struct ServiceKey {
    std::int32_t interface_index = 0;
    std::int32_t protocol = 0;
    std::string name;
    std::string type;
    std::string domain;

    bool operator<(const ServiceKey& other) const {
        return std::tie(interface_index, protocol, name, type, domain) <
               std::tie(other.interface_index, other.protocol, other.name,
                        other.type, other.domain);
    }
};

struct SessionAggregate {
    SessionAdvertisement metadata;
    std::map<ServiceKey, EndpointCandidate> endpoints;
};

struct PendingResolver {
    DBusPendingCall* call = nullptr;
    ServiceKey key;
};

class AvahiDbusDiscoveryBackend final : public IDiscoveryBackend {
public:
    ~AvahiDbusDiscoveryBackend() override { Stop(); }

    DiscoveryBackendKind kind() const noexcept override {
        return DiscoveryBackendKind::AvahiDbus;
    }

    BackendStartResult Start(const DiscoveryRequest& request) override {
        Stop();
        request_ = request;

        if (request.publication) {
            std::string validation_error;
            if (!discovery_protocol::ValidateAdvertisement(*request.publication, validation_error)) {
                return {false, validation_error};
            }
        }
        if (!request.browse && !request.publication) {
            return {false, "discovery request contains neither browse nor publication"};
        }

        static std::once_flag dbus_threads_once;
        std::call_once(dbus_threads_once, [] { dbus_threads_init_default(); });

        ScopedDbusError dbus_error;
        connection_ = dbus_bus_get_private(DBUS_BUS_SYSTEM, dbus_error.get());
        if (!connection_) {
            return {false, dbus_error.message().empty()
                               ? "failed to connect to the system D-Bus"
                               : dbus_error.message()};
        }
        dbus_connection_set_exit_on_disconnect(connection_, FALSE);
        if (!dbus_connection_add_filter(connection_, &Filter, this, nullptr)) {
            Stop();
            return {false, "failed to install Avahi D-Bus signal filter"};
        }
        filter_installed_ = true;

        std::string match_error;
        if (!AddSignalMatches(match_error)) {
            Stop();
            return {false, match_error};
        }

        std::string error;
        MessagePtr api_reply = Call(
            connection_, kServerPath, kServerInterface, "GetAPIVersion", {}, error);
        if (!api_reply) {
            Stop();
            return {false, "Avahi is unavailable: " + error};
        }
        DBusMessageIter api_iter;
        if (!dbus_message_iter_init(api_reply.get(), &api_iter) ||
            dbus_message_iter_get_arg_type(&api_iter) != DBUS_TYPE_UINT32) {
            Stop();
            return {false, "Avahi returned an invalid API version"};
        }

        if (request.browse && !StartBrowser(error)) {
            Stop();
            return {false, error};
        }
        if (request.publication && !StartPublication(*request.publication, error)) {
            Stop();
            return {false, error};
        }
        return {true, {}};
    }

    BackendUpdateResult Update(const SessionAdvertisement& advertisement) override {
        if (!connection_ || entry_group_path_.empty()) {
            return {false, false, "Avahi backend is not publishing a session"};
        }
        std::string error;
        if (!discovery_protocol::ValidateAdvertisement(advertisement, error)) {
            return {false, false, error};
        }
        if (!request_.publication ||
            advertisement.session_uuid != request_.publication->session_uuid ||
            advertisement.transport_version != request_.publication->transport_version ||
            advertisement.legacy_version != request_.publication->legacy_version ||
            advertisement.max_players != request_.publication->max_players ||
            advertisement.tcp_port != request_.publication->tcp_port) {
            return {false, false,
                    "publication identity, protocol, capacity, and TCP port are immutable"};
        }
        // Coalesce updates. Poll sends the latest value asynchronously.
        requested_update_ = advertisement;
        request_.publication = advertisement;
        return {true, false, {}};
    }

    BackendPollResult Poll() override {
        BackendPollResult result;
        if (!connection_) {
            result.health = BackendHealth::Fatal;
            result.error = "Avahi D-Bus connection is not open";
            return result;
        }
        if (!dbus_connection_get_is_connected(connection_)) {
            result.health = BackendHealth::Fatal;
            result.error = "system D-Bus disconnected";
            return result;
        }

        dbus_connection_read_write(connection_, 0);
        while (dbus_connection_dispatch(connection_) == DBUS_DISPATCH_DATA_REMAINS) {
        }

        ProcessPendingResolvers();
        if (!ProcessPendingUpdate(result.error)) {
            result.health = BackendHealth::Fatal;
        }

        std::vector<DBusMessage*> incoming;
        incoming.swap(incoming_signals_);
        for (DBusMessage* message : incoming) {
            if (result.health == BackendHealth::Running) {
                ProcessSignal(message, result);
            }
            dbus_message_unref(message);
        }

        if (result.health == BackendHealth::Running &&
            !pending_update_ && requested_update_) {
            if (!BeginUpdate(*requested_update_, result.error)) {
                result.health = BackendHealth::Fatal;
            } else {
                requested_update_.reset();
            }
        }

        result.events = std::move(events_);
        events_.clear();
        return result;
    }

    void Stop() noexcept override {
        for (PendingResolver& resolver : pending_resolvers_) {
            if (resolver.call) {
                dbus_pending_call_cancel(resolver.call);
                dbus_pending_call_unref(resolver.call);
            }
        }
        pending_resolvers_.clear();
        if (pending_update_) {
            dbus_pending_call_cancel(pending_update_);
            dbus_pending_call_unref(pending_update_);
            pending_update_ = nullptr;
        }

        for (DBusMessage* message : incoming_signals_) {
            dbus_message_unref(message);
        }
        incoming_signals_.clear();

        if (connection_) {
            for (const auto& resolver : resolver_paths_) {
                SendNoReply(connection_, resolver.first, kServiceResolverInterface, "Free");
            }
            SendNoReply(connection_, browser_path_, kServiceBrowserInterface, "Free");
            SendNoReply(connection_, entry_group_path_, kEntryGroupInterface, "Free");
            dbus_connection_flush(connection_);
            if (filter_installed_) {
                dbus_connection_remove_filter(connection_, &Filter, this);
            }
            dbus_connection_close(connection_);
            dbus_connection_unref(connection_);
        }

        connection_ = nullptr;
        filter_installed_ = false;
        browser_path_.clear();
        entry_group_path_.clear();
        instance_name_.clear();
        resolver_paths_.clear();
        pending_keys_.clear();
        key_to_uuid_.clear();
        sessions_.clear();
        events_.clear();
        requested_update_.reset();
        request_ = {};
    }

private:
    static DBusHandlerResult Filter(
        DBusConnection*, DBusMessage* message, void* userdata) {
        auto* self = static_cast<AvahiDbusDiscoveryBackend*>(userdata);
        if (dbus_message_get_type(message) != DBUS_MESSAGE_TYPE_SIGNAL) {
            return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
        }
        dbus_message_ref(message);
        self->incoming_signals_.push_back(message);
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    bool AddSignalMatches(std::string& error) {
        const char* rules[] = {
            "type='signal',sender='org.freedesktop.Avahi'",
            "type='signal',sender='org.freedesktop.DBus',"
            "interface='org.freedesktop.DBus',member='NameOwnerChanged',"
            "arg0='org.freedesktop.Avahi'",
        };
        for (const char* rule : rules) {
            ScopedDbusError dbus_error;
            dbus_bus_add_match(connection_, rule, dbus_error.get());
            if (dbus_error.set()) {
                error = dbus_error.message();
                return false;
            }
        }
        dbus_connection_flush(connection_);
        return true;
    }

    bool StartBrowser(std::string& error) {
        auto append = [](DBusMessageIter& iter) {
            std::int32_t interface_index = kInterfaceUnspecified;
            std::int32_t protocol = kProtocolUnspecified;
            const char* type = kServiceType;
            const char* domain = kDomain;
            std::uint32_t flags = kNoFlags;
            return AppendBasic(iter, DBUS_TYPE_INT32, &interface_index) &&
                   AppendBasic(iter, DBUS_TYPE_INT32, &protocol) &&
                   AppendBasic(iter, DBUS_TYPE_STRING, &type) &&
                   AppendBasic(iter, DBUS_TYPE_STRING, &domain) &&
                   AppendBasic(iter, DBUS_TYPE_UINT32, &flags);
        };

        std::string prepare_error;
        MessagePtr reply = Call(
            connection_, kServerPath, kServer2Interface,
            "ServiceBrowserPrepare", append, prepare_error);
        if (reply) {
            if (!ReadObjectPath(reply.get(), browser_path_, error)) {
                return false;
            }
            MessagePtr started = Call(
                connection_, browser_path_.c_str(), kServiceBrowserInterface,
                "Start", {}, error);
            if (!started) {
                SendNoReply(connection_, browser_path_, kServiceBrowserInterface, "Free");
                browser_path_.clear();
                return false;
            }
            return true;
        }

        reply = Call(
            connection_, kServerPath, kServerInterface,
            "ServiceBrowserNew", append, error);
        if (!reply) {
            error = "Avahi browser creation failed (Server2: " + prepare_error +
                    "; v1: " + error + ")";
            return false;
        }
        return ReadObjectPath(reply.get(), browser_path_, error);
    }

    bool StartPublication(
        const SessionAdvertisement& advertisement,
        std::string& error) {
        MessagePtr group = Call(
            connection_, kServerPath, kServerInterface,
            "EntryGroupNew", {}, error);
        if (!group || !ReadObjectPath(group.get(), entry_group_path_, error)) {
            return false;
        }

        instance_name_ = discovery_protocol::InstanceName(advertisement);
        const std::vector<std::string> txt = discovery_protocol::EncodeTxt(advertisement);
        auto append = [&](DBusMessageIter& iter) {
            std::int32_t interface_index = kInterfaceUnspecified;
            std::int32_t protocol = kProtocolUnspecified;
            std::uint32_t flags = kNoFlags;
            const char* name = instance_name_.c_str();
            const char* type = kServiceType;
            const char* domain = "";
            const char* host = "";
            std::uint16_t port = advertisement.tcp_port;
            return AppendBasic(iter, DBUS_TYPE_INT32, &interface_index) &&
                   AppendBasic(iter, DBUS_TYPE_INT32, &protocol) &&
                   AppendBasic(iter, DBUS_TYPE_UINT32, &flags) &&
                   AppendBasic(iter, DBUS_TYPE_STRING, &name) &&
                   AppendBasic(iter, DBUS_TYPE_STRING, &type) &&
                   AppendBasic(iter, DBUS_TYPE_STRING, &domain) &&
                   AppendBasic(iter, DBUS_TYPE_STRING, &host) &&
                   AppendBasic(iter, DBUS_TYPE_UINT16, &port) &&
                   AppendTxtArray(iter, txt);
        };
        MessagePtr added = Call(
            connection_, entry_group_path_.c_str(), kEntryGroupInterface,
            "AddService", append, error);
        if (!added) {
            return false;
        }
        MessagePtr committed = Call(
            connection_, entry_group_path_.c_str(), kEntryGroupInterface,
            "Commit", {}, error);
        return static_cast<bool>(committed);
    }

    bool BeginUpdate(
        const SessionAdvertisement& advertisement,
        std::string& error) {
        const std::vector<std::string> txt = discovery_protocol::EncodeTxt(advertisement);
        auto append = [&](DBusMessageIter& iter) {
            std::int32_t interface_index = kInterfaceUnspecified;
            std::int32_t protocol = kProtocolUnspecified;
            std::uint32_t flags = kNoFlags;
            const char* name = instance_name_.c_str();
            const char* type = kServiceType;
            const char* domain = "";
            return AppendBasic(iter, DBUS_TYPE_INT32, &interface_index) &&
                   AppendBasic(iter, DBUS_TYPE_INT32, &protocol) &&
                   AppendBasic(iter, DBUS_TYPE_UINT32, &flags) &&
                   AppendBasic(iter, DBUS_TYPE_STRING, &name) &&
                   AppendBasic(iter, DBUS_TYPE_STRING, &type) &&
                   AppendBasic(iter, DBUS_TYPE_STRING, &domain) &&
                   AppendTxtArray(iter, txt);
        };
        pending_update_ = SendAsync(
            connection_, entry_group_path_.c_str(), kEntryGroupInterface,
            "UpdateServiceTxt", append, error);
        return pending_update_ != nullptr;
    }

    bool ProcessPendingUpdate(std::string& error) {
        if (!pending_update_ || !dbus_pending_call_get_completed(pending_update_)) {
            return true;
        }
        MessagePtr reply(dbus_pending_call_steal_reply(pending_update_));
        dbus_pending_call_unref(pending_update_);
        pending_update_ = nullptr;
        if (!reply) {
            error = "Avahi TXT update completed without a reply";
            return false;
        }
        error = ReplyError(reply.get());
        return error.empty();
    }

    void BeginResolve(const ServiceKey& key) {
        if (pending_keys_.find(key) != pending_keys_.end()) {
            return;
        }
        auto append = [&](DBusMessageIter& iter) {
            std::int32_t interface_index = key.interface_index;
            std::int32_t protocol = key.protocol;
            const char* name = key.name.c_str();
            const char* type = key.type.c_str();
            const char* domain = key.domain.c_str();
            std::int32_t address_protocol = kProtocolUnspecified;
            std::uint32_t flags = kNoFlags;
            return AppendBasic(iter, DBUS_TYPE_INT32, &interface_index) &&
                   AppendBasic(iter, DBUS_TYPE_INT32, &protocol) &&
                   AppendBasic(iter, DBUS_TYPE_STRING, &name) &&
                   AppendBasic(iter, DBUS_TYPE_STRING, &type) &&
                   AppendBasic(iter, DBUS_TYPE_STRING, &domain) &&
                   AppendBasic(iter, DBUS_TYPE_INT32, &address_protocol) &&
                   AppendBasic(iter, DBUS_TYPE_UINT32, &flags);
        };
        std::string error;
        DBusPendingCall* call = SendAsync(
            connection_, kServerPath, kServerInterface,
            "ServiceResolverNew", append, error);
        if (!call) {
            return;
        }
        pending_keys_.insert(key);
        pending_resolvers_.push_back({call, key});
    }

    void ProcessPendingResolvers() {
        for (auto it = pending_resolvers_.begin(); it != pending_resolvers_.end();) {
            if (!dbus_pending_call_get_completed(it->call)) {
                ++it;
                continue;
            }
            MessagePtr reply(dbus_pending_call_steal_reply(it->call));
            dbus_pending_call_unref(it->call);
            pending_keys_.erase(it->key);
            if (reply && ReplyError(reply.get()).empty()) {
                std::string path;
                std::string error;
                if (ReadObjectPath(reply.get(), path, error)) {
                    resolver_paths_[path] = it->key;
                }
            }
            it = pending_resolvers_.erase(it);
        }
    }

    void CancelPendingKey(const ServiceKey& key) {
        for (auto it = pending_resolvers_.begin(); it != pending_resolvers_.end();) {
            if (!(it->key < key) && !(key < it->key)) {
                dbus_pending_call_cancel(it->call);
                dbus_pending_call_unref(it->call);
                it = pending_resolvers_.erase(it);
            } else {
                ++it;
            }
        }
        pending_keys_.erase(key);
    }

    void ProcessSignal(DBusMessage* message, BackendPollResult& result) {
        const char* interface = dbus_message_get_interface(message);
        const char* member = dbus_message_get_member(message);
        const char* path = dbus_message_get_path(message);
        if (!interface || !member) {
            return;
        }

        if (std::string(interface) == "org.freedesktop.DBus" &&
            std::string(member) == "NameOwnerChanged") {
            const char* name = nullptr;
            const char* old_owner = nullptr;
            const char* new_owner = nullptr;
            ScopedDbusError error;
            if (dbus_message_get_args(
                    message, error.get(),
                    DBUS_TYPE_STRING, &name,
                    DBUS_TYPE_STRING, &old_owner,
                    DBUS_TYPE_STRING, &new_owner,
                    DBUS_TYPE_INVALID) &&
                name && std::string(name) == kAvahiName &&
                old_owner && *old_owner && (!new_owner || !*new_owner)) {
                result.health = BackendHealth::Fatal;
                result.error = "Avahi daemon disappeared from the system bus";
            }
            return;
        }

        if (std::string(interface) == kServiceBrowserInterface &&
            browser_path_ == (path ? path : "")) {
            if (std::string(member) == "ItemNew" ||
                std::string(member) == "ItemRemove") {
                ServiceKey key;
                const char* name = nullptr;
                const char* type = nullptr;
                const char* domain = nullptr;
                std::uint32_t flags = 0;
                ScopedDbusError error;
                if (!dbus_message_get_args(
                        message, error.get(),
                        DBUS_TYPE_INT32, &key.interface_index,
                        DBUS_TYPE_INT32, &key.protocol,
                        DBUS_TYPE_STRING, &name,
                        DBUS_TYPE_STRING, &type,
                        DBUS_TYPE_STRING, &domain,
                        DBUS_TYPE_UINT32, &flags,
                        DBUS_TYPE_INVALID)) {
                    return;
                }
                key.name = name ? name : "";
                key.type = type ? type : "";
                key.domain = domain ? domain : "";
                if (std::string(member) == "ItemNew") {
                    BeginResolve(key);
                } else {
                    CancelPendingKey(key);
                    RemoveResolvedKey(key);
                }
                return;
            }
            if (std::string(member) == "AllForNow") {
                events_.push_back({BackendEventKind::Ready, {}, std::nullopt, {}});
                return;
            }
            if (std::string(member) == "Failure") {
                result.health = BackendHealth::Fatal;
                result.error = SignalError(message, "Avahi browser failed");
                return;
            }
        }

        if (std::string(interface) == kServiceResolverInterface && path) {
            const auto resolver = resolver_paths_.find(path);
            if (resolver == resolver_paths_.end()) {
                return;
            }
            if (std::string(member) == "Found") {
                ProcessFound(message, resolver->second);
            }
            SendNoReply(connection_, resolver->first, kServiceResolverInterface, "Free");
            resolver_paths_.erase(resolver);
            return;
        }

        if (std::string(interface) == kEntryGroupInterface &&
            entry_group_path_ == (path ? path : "") &&
            std::string(member) == "StateChanged") {
            std::int32_t state = 0;
            const char* detail = nullptr;
            ScopedDbusError error;
            if (dbus_message_get_args(
                    message, error.get(),
                    DBUS_TYPE_INT32, &state,
                    DBUS_TYPE_STRING, &detail,
                    DBUS_TYPE_INVALID) &&
                (state == kEntryGroupCollision || state == kEntryGroupFailure)) {
                result.health = BackendHealth::Fatal;
                result.error = detail && *detail
                    ? detail
                    : (state == kEntryGroupCollision
                           ? "Avahi publication name collision"
                           : "Avahi publication failed");
            }
            return;
        }

        if ((std::string(interface) == kServerInterface ||
             std::string(interface) == kServer2Interface) &&
            std::string(member) == "StateChanged") {
            std::int32_t state = 0;
            const char* detail = nullptr;
            ScopedDbusError error;
            if (dbus_message_get_args(
                    message, error.get(),
                    DBUS_TYPE_INT32, &state,
                    DBUS_TYPE_STRING, &detail,
                    DBUS_TYPE_INVALID) &&
                (state == kServerCollision || state == kServerFailure)) {
                result.health = BackendHealth::Fatal;
                result.error = detail && *detail
                    ? detail
                    : "Avahi server entered a fatal state";
            }
        }
    }

    static std::string SignalError(DBusMessage* message, const char* fallback) {
        DBusMessageIter iter;
        if (dbus_message_iter_init(message, &iter) &&
            dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_STRING) {
            const char* value = nullptr;
            dbus_message_iter_get_basic(&iter, &value);
            if (value && *value) {
                return value;
            }
        }
        return fallback;
    }

    void ProcessFound(DBusMessage* message, const ServiceKey& key) {
        DBusMessageIter iter;
        if (!dbus_message_iter_init(message, &iter)) {
            return;
        }

        std::int32_t interface_index = 0;
        std::int32_t protocol = 0;
        const char* name = nullptr;
        const char* type = nullptr;
        const char* domain = nullptr;
        const char* host = nullptr;
        std::int32_t address_protocol = 0;
        const char* address = nullptr;
        std::uint16_t port = 0;
        std::uint32_t flags = 0;

        auto read_basic = [&](int expected, void* output) {
            if (dbus_message_iter_get_arg_type(&iter) != expected) {
                return false;
            }
            dbus_message_iter_get_basic(&iter, output);
            dbus_message_iter_next(&iter);
            return true;
        };

        if (!read_basic(DBUS_TYPE_INT32, &interface_index) ||
            !read_basic(DBUS_TYPE_INT32, &protocol) ||
            !read_basic(DBUS_TYPE_STRING, &name) ||
            !read_basic(DBUS_TYPE_STRING, &type) ||
            !read_basic(DBUS_TYPE_STRING, &domain) ||
            !read_basic(DBUS_TYPE_STRING, &host) ||
            !read_basic(DBUS_TYPE_INT32, &address_protocol) ||
            !read_basic(DBUS_TYPE_STRING, &address) ||
            !read_basic(DBUS_TYPE_UINT16, &port)) {
            return;
        }
        std::vector<std::string> txt;
        if (!ParseTxtArray(iter, txt)) {
            return;
        }
        dbus_message_iter_next(&iter);
        if (!read_basic(DBUS_TYPE_UINT32, &flags)) {
            return;
        }

        SessionAdvertisement metadata;
        std::string error;
        if (!discovery_protocol::ParseTxt(txt, port, metadata, error)) {
            return;
        }

        EndpointCandidate endpoint;
        endpoint.host_or_numeric_address = address && *address
            ? address
            : (host ? host : "");
        endpoint.port = port;
        if (address_protocol == 1 &&
            endpoint.host_or_numeric_address.size() >= 5) {
            std::string prefix = endpoint.host_or_numeric_address.substr(0, 5);
            std::transform(prefix.begin(), prefix.end(), prefix.begin(),
                           [](unsigned char value) { return std::tolower(value); });
            if (prefix == "fe80:") {
                endpoint.scope_id = static_cast<std::uint32_t>(interface_index);
            }
        }
        if (endpoint.host_or_numeric_address.empty()) {
            return;
        }

        const auto old_key = key_to_uuid_.find(key);
        if (old_key != key_to_uuid_.end() &&
            old_key->second != metadata.session_uuid) {
            RemoveResolvedKey(key);
        }

        const bool added = sessions_.find(metadata.session_uuid) == sessions_.end();
        SessionAggregate& aggregate = sessions_[metadata.session_uuid];
        aggregate.metadata = metadata;
        aggregate.endpoints[key] = endpoint;
        key_to_uuid_[key] = metadata.session_uuid;
        EmitSession(added ? BackendEventKind::Added : BackendEventKind::Updated,
                    metadata.session_uuid, aggregate);
    }

    void RemoveResolvedKey(const ServiceKey& key) {
        const auto key_entry = key_to_uuid_.find(key);
        if (key_entry == key_to_uuid_.end()) {
            return;
        }
        const std::string uuid = key_entry->second;
        key_to_uuid_.erase(key_entry);

        const auto session = sessions_.find(uuid);
        if (session == sessions_.end()) {
            return;
        }
        session->second.endpoints.erase(key);
        if (session->second.endpoints.empty()) {
            events_.push_back({BackendEventKind::Removed, uuid, std::nullopt, {}});
            sessions_.erase(session);
        } else {
            EmitSession(BackendEventKind::Updated, uuid, session->second);
        }
    }

    void EmitSession(
        BackendEventKind kind,
        const std::string& uuid,
        const SessionAggregate& aggregate) {
        DiscoveredSession discovered;
        discovered.metadata = aggregate.metadata;
        for (const auto& entry : aggregate.endpoints) {
            const EndpointCandidate& candidate = entry.second;
            const bool duplicate = std::any_of(
                discovered.endpoints.begin(), discovered.endpoints.end(),
                [&](const EndpointCandidate& existing) {
                    return existing.host_or_numeric_address ==
                               candidate.host_or_numeric_address &&
                           existing.port == candidate.port &&
                           existing.scope_id == candidate.scope_id;
                });
            if (!duplicate) {
                discovered.endpoints.push_back(candidate);
            }
        }
        events_.push_back({kind, uuid, std::move(discovered), {}});
    }

    DBusConnection* connection_ = nullptr;
    bool filter_installed_ = false;
    DiscoveryRequest request_;
    std::string browser_path_;
    std::string entry_group_path_;
    std::string instance_name_;
    std::vector<DBusMessage*> incoming_signals_;
    std::vector<PendingResolver> pending_resolvers_;
    std::set<ServiceKey> pending_keys_;
    std::map<std::string, ServiceKey> resolver_paths_;
    std::map<ServiceKey, std::string> key_to_uuid_;
    std::map<std::string, SessionAggregate> sessions_;
    std::vector<BackendEvent> events_;
    DBusPendingCall* pending_update_ = nullptr;
    std::optional<SessionAdvertisement> requested_update_;
};

}  // namespace

DiscoveryBackendFactory AvahiDbusDiscoveryFactory() {
    return {
        DiscoveryBackendKind::AvahiDbus,
        [] { return std::make_unique<AvahiDbusDiscoveryBackend>(); },
    };
}

}  // namespace lego_loco::network

#endif  // defined(__linux__) && !defined(__ANDROID__)
#endif  // !_WIN32
