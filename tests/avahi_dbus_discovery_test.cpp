#include "avahi_dbus_discovery.h"

#ifndef _WIN32
#if defined(__linux__) && !defined(__ANDROID__)

#include <dbus/dbus.h>

#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace lego_loco::network;
using namespace std::chrono_literals;

namespace {

constexpr char kAvahiName[] = "org.freedesktop.Avahi";
constexpr char kServerInterface[] = "org.freedesktop.Avahi.Server";
constexpr char kServer2Interface[] = "org.freedesktop.Avahi.Server2";
constexpr char kBrowserInterface[] = "org.freedesktop.Avahi.ServiceBrowser";
constexpr char kResolverInterface[] = "org.freedesktop.Avahi.ServiceResolver";
constexpr char kEntryGroupInterface[] = "org.freedesktop.Avahi.EntryGroup";
constexpr char kBrowserPath[] = "/fake/browser/1";
constexpr char kResolverPath[] = "/fake/resolver/1";
constexpr char kEntryGroupPath[] = "/fake/group/1";
constexpr char kUuid[] = "12345678-1234-1234-1234-123456789abc";
constexpr char kInstance[] = "Fake Lego Loco [12345678]";
constexpr char kServiceType[] = "_legoloco._tcp";
constexpr char kDomain[] = "local";

bool AppendBasic(DBusMessageIter& iter, int type, const void* value) {
    return dbus_message_iter_append_basic(&iter, type, value) != FALSE;
}

bool AppendTxt(DBusMessageIter& iter, const std::vector<std::string>& txt) {
    DBusMessageIter outer;
    assert(dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "ay", &outer));
    for (const std::string& entry : txt) {
        DBusMessageIter bytes;
        assert(dbus_message_iter_open_container(&outer, DBUS_TYPE_ARRAY, "y", &bytes));
        const unsigned char* data =
            reinterpret_cast<const unsigned char*>(entry.data());
        const int size = static_cast<int>(entry.size());
        assert(dbus_message_iter_append_fixed_array(&bytes, DBUS_TYPE_BYTE, &data, size));
        assert(dbus_message_iter_close_container(&outer, &bytes));
    }
    return dbus_message_iter_close_container(&iter, &outer) != FALSE;
}

class FakeAvahiService {
public:
    explicit FakeAvahiService(bool server2) : server2_(server2) {
        thread_ = std::thread([this] { Run(); });
        std::unique_lock<std::mutex> lock(mutex_);
        ready_cv_.wait(lock, [this] { return ready_; });
        assert(start_error_.empty());
    }

    ~FakeAvahiService() {
        stop_.store(true);
        if (thread_.joinable()) {
            thread_.join();
        }
    }

    void EmitRemove() { emit_remove_.store(true); }
    void DropName() { drop_name_.store(true); }

    int update_count() const { return update_count_.load(); }
    int free_count() const { return free_count_.load(); }
    int prepare_count() const { return prepare_count_.load(); }
    int browser_new_count() const { return browser_new_count_.load(); }

private:
    void SetReady(const std::string& error = {}) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            start_error_ = error;
            ready_ = true;
        }
        ready_cv_.notify_all();
    }

    void Run() {
        dbus_threads_init_default();
        DBusError error;
        dbus_error_init(&error);
        connection_ = dbus_bus_get_private(DBUS_BUS_SYSTEM, &error);
        if (!connection_) {
            const std::string detail = error.message ? error.message : "bus connection failed";
            dbus_error_free(&error);
            SetReady(detail);
            return;
        }
        dbus_connection_set_exit_on_disconnect(connection_, FALSE);
        const int request = dbus_bus_request_name(
            connection_, kAvahiName, DBUS_NAME_FLAG_DO_NOT_QUEUE, &error);
        if (dbus_error_is_set(&error) || request != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
            const std::string detail = error.message ? error.message : "name request failed";
            dbus_error_free(&error);
            SetReady(detail);
            Cleanup();
            return;
        }
        dbus_error_free(&error);
        SetReady();

        while (!stop_.load()) {
            dbus_connection_read_write(connection_, 10);
            while (DBusMessage* message = dbus_connection_pop_message(connection_)) {
                HandleMessage(message);
                dbus_message_unref(message);
            }
            if (emit_remove_.exchange(false)) {
                SendBrowserSignal("ItemRemove");
            }
            if (drop_name_.exchange(false)) {
                DBusError release_error;
                dbus_error_init(&release_error);
                dbus_bus_release_name(connection_, kAvahiName, &release_error);
                dbus_error_free(&release_error);
            }
        }
        Cleanup();
    }

    void Cleanup() {
        if (connection_) {
            dbus_connection_close(connection_);
            dbus_connection_unref(connection_);
            connection_ = nullptr;
        }
    }

    void HandleMessage(DBusMessage* message) {
        if (dbus_message_get_type(message) != DBUS_MESSAGE_TYPE_METHOD_CALL) {
            return;
        }
        const char* interface = dbus_message_get_interface(message);
        const char* member = dbus_message_get_member(message);
        const char* path = dbus_message_get_path(message);
        const std::string iface = interface ? interface : "";
        const std::string method = member ? member : "";
        const std::string object_path = path ? path : "";

        if (iface == kServerInterface && method == "GetAPIVersion") {
            DBusMessage* reply = dbus_message_new_method_return(message);
            DBusMessageIter iter;
            dbus_message_iter_init_append(reply, &iter);
            std::uint32_t version = 0x0204;
            assert(AppendBasic(iter, DBUS_TYPE_UINT32, &version));
            Send(reply);
            return;
        }
        if (iface == kServer2Interface && method == "ServiceBrowserPrepare") {
            ++prepare_count_;
            if (!server2_) {
                Send(dbus_message_new_error(
                    message, DBUS_ERROR_UNKNOWN_METHOD, "Server2 unavailable"));
                return;
            }
            SendObjectPath(message, kBrowserPath);
            return;
        }
        if (iface == kServerInterface && method == "ServiceBrowserNew") {
            ++browser_new_count_;
            SendObjectPath(message, kBrowserPath);
            SendBrowserSignal("ItemNew");
            SendSimpleSignal(kBrowserPath, kBrowserInterface, "AllForNow");
            return;
        }
        if (object_path == kBrowserPath && iface == kBrowserInterface &&
            method == "Start") {
            Send(dbus_message_new_method_return(message));
            SendBrowserSignal("ItemNew");
            SendSimpleSignal(kBrowserPath, kBrowserInterface, "AllForNow");
            return;
        }
        if (iface == kServerInterface && method == "ServiceResolverNew") {
            SendObjectPath(message, kResolverPath);
            SendFoundSignal();
            return;
        }
        if (iface == kServerInterface && method == "EntryGroupNew") {
            SendObjectPath(message, kEntryGroupPath);
            return;
        }
        if (object_path == kEntryGroupPath && iface == kEntryGroupInterface &&
            (method == "AddService" || method == "Commit")) {
            Send(dbus_message_new_method_return(message));
            return;
        }
        if (object_path == kEntryGroupPath && iface == kEntryGroupInterface &&
            method == "UpdateServiceTxt") {
            ++update_count_;
            Send(dbus_message_new_method_return(message));
            return;
        }
        if (method == "Free") {
            ++free_count_;
            if (!dbus_message_get_no_reply(message)) {
                Send(dbus_message_new_method_return(message));
            }
            return;
        }

        Send(dbus_message_new_error(
            message, DBUS_ERROR_UNKNOWN_METHOD, "fake Avahi method not implemented"));
    }

    void Send(DBusMessage* message) {
        assert(message);
        assert(dbus_connection_send(connection_, message, nullptr));
        dbus_connection_flush(connection_);
        dbus_message_unref(message);
    }

    void SendObjectPath(DBusMessage* request, const char* path) {
        DBusMessage* reply = dbus_message_new_method_return(request);
        DBusMessageIter iter;
        dbus_message_iter_init_append(reply, &iter);
        assert(AppendBasic(iter, DBUS_TYPE_OBJECT_PATH, &path));
        Send(reply);
    }

    void SendSimpleSignal(const char* path, const char* interface, const char* name) {
        DBusMessage* signal = dbus_message_new_signal(path, interface, name);
        Send(signal);
    }

    void SendBrowserSignal(const char* name) {
        DBusMessage* signal = dbus_message_new_signal(kBrowserPath, kBrowserInterface, name);
        DBusMessageIter iter;
        dbus_message_iter_init_append(signal, &iter);
        std::int32_t interface_index = 2;
        std::int32_t protocol = 0;
        const char* instance = kInstance;
        const char* type = kServiceType;
        const char* domain = kDomain;
        std::uint32_t flags = 0;
        assert(AppendBasic(iter, DBUS_TYPE_INT32, &interface_index));
        assert(AppendBasic(iter, DBUS_TYPE_INT32, &protocol));
        assert(AppendBasic(iter, DBUS_TYPE_STRING, &instance));
        assert(AppendBasic(iter, DBUS_TYPE_STRING, &type));
        assert(AppendBasic(iter, DBUS_TYPE_STRING, &domain));
        assert(AppendBasic(iter, DBUS_TYPE_UINT32, &flags));
        Send(signal);
    }

    void SendFoundSignal() {
        DBusMessage* signal = dbus_message_new_signal(
            kResolverPath, kResolverInterface, "Found");
        DBusMessageIter iter;
        dbus_message_iter_init_append(signal, &iter);
        std::int32_t interface_index = 2;
        std::int32_t protocol = 0;
        const char* instance = kInstance;
        const char* type = kServiceType;
        const char* domain = kDomain;
        const char* host = "fake-loco.local";
        std::int32_t address_protocol = 0;
        const char* address = "192.0.2.44";
        std::uint16_t port = 42424;
        std::uint32_t flags = 0;
        const std::vector<std::string> txt = {
            std::string("id=") + kUuid,
            "name=Fake session",
            "pv=1",
            "lv=300",
            "players=1",
            "max=9",
        };
        assert(AppendBasic(iter, DBUS_TYPE_INT32, &interface_index));
        assert(AppendBasic(iter, DBUS_TYPE_INT32, &protocol));
        assert(AppendBasic(iter, DBUS_TYPE_STRING, &instance));
        assert(AppendBasic(iter, DBUS_TYPE_STRING, &type));
        assert(AppendBasic(iter, DBUS_TYPE_STRING, &domain));
        assert(AppendBasic(iter, DBUS_TYPE_STRING, &host));
        assert(AppendBasic(iter, DBUS_TYPE_INT32, &address_protocol));
        assert(AppendBasic(iter, DBUS_TYPE_STRING, &address));
        assert(AppendBasic(iter, DBUS_TYPE_UINT16, &port));
        assert(AppendTxt(iter, txt));
        assert(AppendBasic(iter, DBUS_TYPE_UINT32, &flags));
        Send(signal);
    }

    bool server2_ = false;
    std::thread thread_;
    DBusConnection* connection_ = nullptr;
    std::atomic<bool> stop_{false};
    std::atomic<bool> emit_remove_{false};
    std::atomic<bool> drop_name_{false};
    std::atomic<int> update_count_{0};
    std::atomic<int> free_count_{0};
    std::atomic<int> prepare_count_{0};
    std::atomic<int> browser_new_count_{0};
    std::mutex mutex_;
    std::condition_variable ready_cv_;
    bool ready_ = false;
    std::string start_error_;
};

SessionAdvertisement Publication(std::uint8_t players = 1) {
    SessionAdvertisement result;
    result.session_uuid = kUuid;
    result.display_name = "Published session";
    result.transport_version = 1;
    result.legacy_version = 300;
    result.current_players = players;
    result.max_players = 9;
    result.tcp_port = 42424;
    return result;
}

template <typename Predicate>
BackendPollResult PollUntil(IDiscoveryBackend& backend, Predicate predicate) {
    BackendPollResult combined;
    for (int attempt = 0; attempt < 200; ++attempt) {
        BackendPollResult poll = backend.Poll();
        if (poll.health == BackendHealth::Fatal) {
            return poll;
        }
        combined.events.insert(
            combined.events.end(),
            std::make_move_iterator(poll.events.begin()),
            std::make_move_iterator(poll.events.end()));
        if (predicate(combined)) {
            return combined;
        }
        std::this_thread::sleep_for(5ms);
    }
    assert(false && "timed out waiting for Avahi backend event");
    return combined;
}

void TestBrowsePublishUpdateRemoveAndDaemonLoss(bool server2) {
    FakeAvahiService fake(server2);
    DiscoveryBackendFactory factory = AvahiDbusDiscoveryFactory();
    std::unique_ptr<IDiscoveryBackend> backend = factory.create();

    DiscoveryRequest request;
    request.browse = true;
    request.publication = Publication();
    BackendStartResult start = backend->Start(request);
    assert(start.ok);

    BackendPollResult discovered = PollUntil(*backend, [](const BackendPollResult& poll) {
        bool added = false;
        bool ready = false;
        for (const BackendEvent& event : poll.events) {
            added |= event.kind == BackendEventKind::Added;
            ready |= event.kind == BackendEventKind::Ready;
        }
        return added && ready;
    });
    bool verified = false;
    for (const BackendEvent& event : discovered.events) {
        if (event.kind != BackendEventKind::Added) {
            continue;
        }
        assert(event.session.has_value());
        assert(event.session->metadata.session_uuid == kUuid);
        assert(event.session->metadata.legacy_version == 300);
        assert(event.session->endpoints.size() == 1);
        assert(event.session->endpoints[0].host_or_numeric_address == "192.0.2.44");
        assert(event.session->endpoints[0].port == 42424);
        verified = true;
    }
    assert(verified);
    assert(fake.prepare_count() == 1);
    assert(fake.browser_new_count() == (server2 ? 0 : 1));

    BackendUpdateResult update = backend->Update(Publication(2));
    assert(update.ok);
    for (int attempt = 0; attempt < 200 && fake.update_count() == 0; ++attempt) {
        BackendPollResult poll = backend->Poll();
        assert(poll.health == BackendHealth::Running);
        std::this_thread::sleep_for(5ms);
    }
    assert(fake.update_count() == 1);

    SessionAdvertisement invalid_update = Publication(3);
    ++invalid_update.tcp_port;
    BackendUpdateResult rejected = backend->Update(invalid_update);
    assert(!rejected.ok);
    assert(!rejected.fatal);

    fake.EmitRemove();
    BackendPollResult removed = PollUntil(*backend, [](const BackendPollResult& poll) {
        for (const BackendEvent& event : poll.events) {
            if (event.kind == BackendEventKind::Removed) {
                return true;
            }
        }
        return false;
    });
    assert(removed.health == BackendHealth::Running);

    fake.DropName();
    BackendPollResult lost = PollUntil(*backend, [](const BackendPollResult&) {
        return false;
    });
    assert(lost.health == BackendHealth::Fatal);
    assert(lost.error.find("disappeared") != std::string::npos);
    backend->Stop();
}

void TestExplicitObjectCleanup() {
    FakeAvahiService fake(true);
    std::unique_ptr<IDiscoveryBackend> backend =
        AvahiDbusDiscoveryFactory().create();
    DiscoveryRequest request;
    request.browse = true;
    request.publication = Publication();
    assert(backend->Start(request).ok);
    backend->Stop();
    for (int attempt = 0; attempt < 200 && fake.free_count() < 2; ++attempt) {
        std::this_thread::sleep_for(5ms);
    }
    assert(fake.free_count() >= 2);
}

}  // namespace

int main() {
    TestExplicitObjectCleanup();
    TestBrowsePublishUpdateRemoveAndDaemonLoss(true);
    TestBrowsePublishUpdateRemoveAndDaemonLoss(false);
    std::cout << "PASS: Avahi D-Bus publication, browse/resolve, v1 fallback, and loss handling\n";
    return 0;
}

#endif  // defined(__linux__) && !defined(__ANDROID__)
#endif  // !_WIN32
