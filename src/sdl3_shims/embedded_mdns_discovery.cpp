#ifndef _WIN32

#include "embedded_mdns_discovery.h"
#include "network_discovery_protocol.h"
#include "vendor/mjansson_mdns/mdns.h"

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/socket.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cerrno>
#include <cstring>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace lego_loco::network {
namespace {

using Clock = std::chrono::steady_clock;
constexpr std::chrono::seconds kBrowseInterval{2};
constexpr std::chrono::seconds kAnnounceInterval{30};
constexpr std::chrono::seconds kInterfaceRefreshInterval{5};
constexpr std::chrono::milliseconds kProbeInterval{250};
constexpr int kProbeCount = 3;
constexpr std::uint32_t kMaxAcceptedTtl = 3600;

std::string LowerDnsName(const char* data, std::size_t size) {
    std::string result(data, size);
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
    if (!result.empty() && result.back() != '.') {
        result.push_back('.');
    }
    return result;
}

mdns_string_t MdnsString(const std::string& value) {
    return {value.c_str(), value.size()};
}

Clock::time_point Expiry(std::uint32_t ttl) {
    return Clock::now() + std::chrono::seconds(std::min(ttl, kMaxAcceptedTtl));
}

bool SameEndpoint(const EndpointCandidate& left, const EndpointCandidate& right) {
    return left.host_or_numeric_address == right.host_or_numeric_address &&
           left.port == right.port && left.scope_id == right.scope_id;
}

bool SameSession(const DiscoveredSession& left, const DiscoveredSession& right) {
    const SessionAdvertisement& a = left.metadata;
    const SessionAdvertisement& b = right.metadata;
    if (a.session_uuid != b.session_uuid || a.display_name != b.display_name ||
        a.transport_version != b.transport_version ||
        a.legacy_version != b.legacy_version ||
        a.current_players != b.current_players || a.max_players != b.max_players ||
        a.tcp_port != b.tcp_port || left.endpoints.size() != right.endpoints.size()) {
        return false;
    }
    for (const EndpointCandidate& endpoint : left.endpoints) {
        if (std::none_of(right.endpoints.begin(), right.endpoints.end(),
                         [&](const EndpointCandidate& candidate) {
                             return SameEndpoint(endpoint, candidate);
                         })) {
            return false;
        }
    }
    return true;
}

bool HasCompetingIpv4Responder() {
    const int socket_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket_fd < 0) return true;
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(MDNS_PORT);
    const bool occupied =
        bind(socket_fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0;
    close(socket_fd);
    return occupied;
}

int OpenIpv4ServiceSocket(const std::optional<sockaddr_in>& interface_address) {
    const int socket_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket_fd < 0) {
        return -1;
    }
    int enabled = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled)) < 0) {
        close(socket_fd);
        return -1;
    }
    unsigned char ttl = 1;
    unsigned char loopback = 1;
    setsockopt(socket_fd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));
    setsockopt(socket_fd, IPPROTO_IP, IP_MULTICAST_LOOP, &loopback, sizeof(loopback));
    ip_mreq membership{};
    membership.imr_multiaddr.s_addr = htonl(0xE00000FBU);
    if (interface_address) membership.imr_interface = interface_address->sin_addr;
    if (setsockopt(socket_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                   &membership, sizeof(membership)) < 0) {
        close(socket_fd);
        return -1;
    }
    if (interface_address)
        setsockopt(socket_fd, IPPROTO_IP, IP_MULTICAST_IF,
                   &interface_address->sin_addr, sizeof(interface_address->sin_addr));
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(MDNS_PORT);
    if (bind(socket_fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        close(socket_fd);
        return -1;
    }
    const int flags = fcntl(socket_fd, F_GETFL, 0);
    if (flags < 0 || fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        close(socket_fd);
        return -1;
    }
    return socket_fd;
}

int OpenIpv4ClientSocket(const std::optional<sockaddr_in>& interface_address) {
    sockaddr_in address = interface_address.value_or(sockaddr_in{});
    address.sin_family = AF_INET;
    address.sin_port = 0;
    return mdns_socket_open_ipv4(&address);
}

int OpenIpv6ClientSocket(const std::optional<sockaddr_in6>& interface_address) {
    sockaddr_in6 address = interface_address.value_or(sockaddr_in6{});
    address.sin6_family = AF_INET6;
    address.sin6_port = 0;
    return mdns_socket_open_ipv6(&address);
}

int OpenIpv6ServiceSocket() {
    const int socket_fd = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    if (socket_fd < 0) {
        return -1;
    }
    int enabled = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled)) < 0) {
        close(socket_fd);
        return -1;
    }
    int hops = 1;
    unsigned int loopback = 1;
    setsockopt(socket_fd, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &hops, sizeof(hops));
    setsockopt(socket_fd, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &loopback, sizeof(loopback));
    ipv6_mreq membership{};
    membership.ipv6mr_multiaddr.s6_addr[0] = 0xFF;
    membership.ipv6mr_multiaddr.s6_addr[1] = 0x02;
    membership.ipv6mr_multiaddr.s6_addr[15] = 0xFB;
    if (setsockopt(socket_fd, IPPROTO_IPV6, IPV6_JOIN_GROUP,
                   &membership, sizeof(membership)) < 0) {
        close(socket_fd);
        return -1;
    }
    sockaddr_in6 address{};
    address.sin6_family = AF_INET6;
    address.sin6_addr = in6addr_any;
    address.sin6_port = htons(MDNS_PORT);
    if (bind(socket_fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        close(socket_fd);
        return -1;
    }
    const int flags = fcntl(socket_fd, F_GETFL, 0);
    if (flags < 0 || fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        close(socket_fd);
        return -1;
    }
    return socket_fd;
}

struct TimedPtr { std::string instance; Clock::time_point expiry; };
struct TimedSrv { std::string target; std::uint16_t port = 0; Clock::time_point expiry; };
struct TimedTxt { std::vector<std::string> entries; Clock::time_point expiry; };
struct TimedAddress { EndpointCandidate endpoint; Clock::time_point expiry; };

struct LocalAddresses {
    std::optional<sockaddr_in> ipv4;
    std::optional<sockaddr_in6> ipv6;

    bool operator==(const LocalAddresses& other) const {
        const bool same4 = (!ipv4 && !other.ipv4) ||
            (ipv4 && other.ipv4 &&
             ipv4->sin_addr.s_addr == other.ipv4->sin_addr.s_addr);
        const bool same6 = (!ipv6 && !other.ipv6) ||
            (ipv6 && other.ipv6 &&
             std::memcmp(&ipv6->sin6_addr, &other.ipv6->sin6_addr,
                         sizeof(in6_addr)) == 0 &&
             ipv6->sin6_scope_id == other.ipv6->sin6_scope_id);
        return same4 && same6;
    }
};

LocalAddresses EnumerateLocalAddresses() {
    LocalAddresses result;
    ifaddrs* addresses = nullptr;
    if (getifaddrs(&addresses) < 0) {
        return result;
    }
    for (ifaddrs* current = addresses; current; current = current->ifa_next) {
        if (!current->ifa_addr || !(current->ifa_flags & IFF_UP) ||
            !(current->ifa_flags & IFF_MULTICAST) ||
            (current->ifa_flags & IFF_LOOPBACK) ||
            (current->ifa_flags & IFF_POINTOPOINT)) {
            continue;
        }
        if (!result.ipv4 && current->ifa_addr->sa_family == AF_INET) {
            result.ipv4 = *reinterpret_cast<sockaddr_in*>(current->ifa_addr);
        } else if (!result.ipv6 && current->ifa_addr->sa_family == AF_INET6) {
            const auto* value = reinterpret_cast<sockaddr_in6*>(current->ifa_addr);
            if (!IN6_IS_ADDR_LOOPBACK(&value->sin6_addr)) {
                result.ipv6 = *value;
            }
        }
    }
    freeifaddrs(addresses);
    return result;
}

class EmbeddedMdnsDiscoveryBackend final : public IDiscoveryBackend {
public:
    ~EmbeddedMdnsDiscoveryBackend() override { Stop(); }

    DiscoveryBackendKind kind() const noexcept override {
        return DiscoveryBackendKind::EmbeddedMdns;
    }

    BackendStartResult Start(const DiscoveryRequest& request) override {
        Stop();
        if (!request.browse && !request.publication) {
            return {false, "discovery request contains neither browse nor publication"};
        }
        if (request.publication) {
            std::string error;
            if (!discovery_protocol::ValidateAdvertisement(*request.publication, error)) {
                return {false, error};
            }
        }

        if (request.publication && HasCompetingIpv4Responder()) {
            return {false,
                    "another local mDNS responder already owns UDP 5353; "
                    "embedded publication would be unreliable"};
        }

        local_addresses_ = EnumerateLocalAddresses();
        const int client4 = OpenIpv4ClientSocket(local_addresses_.ipv4);
        if (client4 >= 0) client_sockets_.push_back(client4);
        const int client6 = OpenIpv6ClientSocket(local_addresses_.ipv6);
        if (client6 >= 0) client_sockets_.push_back(client6);
        // A browser also listens on 5353 for unsolicited announcements/goodbyes.
        // Ephemeral query sockets remain authoritative when another local responder
        // captures shared-port unicast delivery.
        const int service4 = OpenIpv4ServiceSocket(local_addresses_.ipv4);
        if (service4 >= 0) service_sockets_.push_back(service4);
        const int service6 = OpenIpv6ServiceSocket();
        if (service6 >= 0) service_sockets_.push_back(service6);
        if (client_sockets_.empty() ||
            (request.publication && service_sockets_.empty())) {
            Stop();
            return {false, "failed to open required mDNS query/service sockets"};
        }

        request_ = request;
        const Clock::time_point now = Clock::now();
        next_browse_ = now;
        next_interface_refresh_ = now + kInterfaceRefreshInterval;
        if (request.publication) {
            BuildPublication(*request.publication);
            probing_ = true;
            probes_sent_ = 0;
            next_probe_ = now;
        }
        return {true, {}};
    }

    BackendUpdateResult Update(const SessionAdvertisement& advertisement) override {
        if (!request_.publication) {
            return {false, false, "embedded mDNS backend is not publishing a session"};
        }
        std::string error;
        if (!discovery_protocol::ValidateAdvertisement(advertisement, error)) {
            return {false, false, error};
        }
        const SessionAdvertisement& current = *request_.publication;
        if (advertisement.session_uuid != current.session_uuid ||
            advertisement.transport_version != current.transport_version ||
            advertisement.legacy_version != current.legacy_version ||
            advertisement.max_players != current.max_players ||
            advertisement.tcp_port != current.tcp_port) {
            return {false, false,
                    "publication identity, protocol, capacity, and TCP port are immutable"};
        }
        request_.publication = advertisement;
        BuildPublication(advertisement);
        if (published_ && !Announce(false)) {
            return {false, true, "failed to multicast updated mDNS publication"};
        }
        return {true, false, {}};
    }

    BackendPollResult Poll() override {
        BackendPollResult result;
        if (client_sockets_.empty()) {
            return {BackendHealth::Fatal, {}, "embedded mDNS query sockets are closed"};
        }

        for (int socket_fd : service_sockets_) {
            for (;;) {
                const int records = mdns_socket_listen(
                    socket_fd, receive_buffer_.data(), receive_buffer_.size(),
                    &RecordCallback, this);
                if (records <= 0) break;
            }
        }
        for (int socket_fd : client_sockets_) {
            for (;;) {
                const int records = mdns_query_recv(
                    socket_fd, receive_buffer_.data(), receive_buffer_.size(),
                    &RecordCallback, this, 0);
                if (records <= 0) break;
            }
        }
        if (conflict_) {
            return {BackendHealth::Fatal, {},
                    "embedded mDNS detected a publication name conflict"};
        }

        const Clock::time_point now = Clock::now();
        if (request_.browse && now >= next_browse_) {
            if (!SendBrowseQuery()) {
                return {BackendHealth::Fatal, {}, "failed to send mDNS browse query"};
            }
            SendUnresolvedAddressQueries();
            next_browse_ = now + kBrowseInterval;
        }
        if (probing_ && now >= next_probe_) {
            if (probes_sent_ < kProbeCount) {
                if (!SendProbe()) {
                    return {BackendHealth::Fatal, {}, "failed to send mDNS publication probe"};
                }
                ++probes_sent_;
                next_probe_ = now + kProbeInterval;
            } else {
                probing_ = false;
                published_ = true;
                if (!Announce(false)) {
                    return {BackendHealth::Fatal, {}, "failed to announce mDNS publication"};
                }
                next_announce_ = now + kAnnounceInterval;
            }
        } else if (published_ && now >= next_announce_) {
            if (!Announce(false)) {
                return {BackendHealth::Fatal, {}, "failed to refresh mDNS publication"};
            }
            next_announce_ = now + kAnnounceInterval;
        }
        if (now >= next_interface_refresh_) {
            const LocalAddresses refreshed = EnumerateLocalAddresses();
            if (!(refreshed == local_addresses_)) {
                local_addresses_ = refreshed;
                if (published_) Announce(false);
            }
            next_interface_refresh_ = now + kInterfaceRefreshInterval;
        }

        ExpireAndReconcile();
        result.events = std::move(events_);
        events_.clear();
        return result;
    }

    void Stop() noexcept override {
        if (published_) Announce(true);
        for (int socket_fd : client_sockets_) mdns_socket_close(socket_fd);
        for (int socket_fd : service_sockets_) mdns_socket_close(socket_fd);
        client_sockets_.clear();
        service_sockets_.clear();
        request_ = {};
        ptr_.clear(); srv_.clear(); txt_.clear(); addresses_.clear(); visible_.clear();
        events_.clear();
        probing_ = false; published_ = false; conflict_ = false; probes_sent_ = 0;
        instance_name_.clear(); instance_fqdn_.clear(); hostname_fqdn_.clear();
    }

private:
    static int RecordCallback(
        int socket_fd, const sockaddr* from, std::size_t addrlen,
        mdns_entry_type_t entry, std::uint16_t query_id, std::uint16_t rtype,
        std::uint16_t rclass, std::uint32_t ttl, const void* data, std::size_t size,
        std::size_t name_offset, std::size_t, std::size_t record_offset,
        std::size_t record_length, void* userdata) {
        auto* self = static_cast<EmbeddedMdnsDiscoveryBackend*>(userdata);
        if (entry == MDNS_ENTRYTYPE_QUESTION) {
            self->AnswerQuestion(socket_fd, from, addrlen, query_id, rtype, rclass,
                                 data, size, name_offset);
        } else {
            self->ConsumeRecord(from, rtype, ttl, data, size, name_offset,
                                record_offset, record_length);
        }
        return 0;
    }

    void BuildPublication(const SessionAdvertisement& advertisement) {
        instance_name_ = discovery_protocol::InstanceName(advertisement);
        instance_fqdn_ = instance_name_ + "." + discovery_protocol::kQualifiedServiceType;
        hostname_fqdn_ = "loco-" + advertisement.session_uuid.substr(0, 8) + ".local.";
        txt_entries_ = discovery_protocol::EncodeTxt(advertisement);
    }

    mdns_record_t PtrRecord() const {
        mdns_record_t value{}; value.name = MdnsString(service_name_);
        value.type = MDNS_RECORDTYPE_PTR; value.data.ptr.name = MdnsString(instance_fqdn_);
        return value;
    }
    mdns_record_t SrvRecord() const {
        mdns_record_t value{}; value.name = MdnsString(instance_fqdn_);
        value.type = MDNS_RECORDTYPE_SRV; value.data.srv.name = MdnsString(hostname_fqdn_);
        value.data.srv.port = request_.publication->tcp_port;
        return value;
    }
    std::vector<mdns_record_t> AdditionalRecords() const {
        std::vector<mdns_record_t> result;
        result.push_back(SrvRecord());
        if (local_addresses_.ipv4) {
            mdns_record_t record{}; record.name = MdnsString(hostname_fqdn_);
            record.type = MDNS_RECORDTYPE_A; record.data.a.addr = *local_addresses_.ipv4;
            result.push_back(record);
        }
        if (local_addresses_.ipv6) {
            mdns_record_t record{}; record.name = MdnsString(hostname_fqdn_);
            record.type = MDNS_RECORDTYPE_AAAA; record.data.aaaa.addr = *local_addresses_.ipv6;
            result.push_back(record);
        }
        for (const std::string& entry : txt_entries_) {
            const std::size_t equals = entry.find('=');
            mdns_record_t record{}; record.name = MdnsString(instance_fqdn_);
            record.type = MDNS_RECORDTYPE_TXT;
            record.data.txt.key = {entry.c_str(), equals};
            record.data.txt.value = {entry.c_str() + equals + 1, entry.size() - equals - 1};
            result.push_back(record);
        }
        return result;
    }

    bool Announce(bool goodbye) noexcept {
        if (!request_.publication) return true;
        const mdns_record_t ptr = PtrRecord();
        const std::vector<mdns_record_t> additional = AdditionalRecords();
        bool sent = false;
        for (int socket_fd : service_sockets_) {
            const int status = goodbye
                ? mdns_goodbye_multicast(socket_fd, send_buffer_.data(), send_buffer_.size(),
                                         ptr, nullptr, 0, additional.data(), additional.size())
                : mdns_announce_multicast(socket_fd, send_buffer_.data(), send_buffer_.size(),
                                          ptr, nullptr, 0, additional.data(), additional.size());
            sent |= status == 0;
        }
        return sent;
    }

    bool SendBrowseQuery() {
        bool sent = false;
        for (int socket_fd : client_sockets_) {
            const int id = mdns_query_send(socket_fd, MDNS_RECORDTYPE_PTR,
                service_name_.c_str(), service_name_.size(), send_buffer_.data(),
                send_buffer_.size(), 0);
            sent |= id >= 0;
        }
        return sent;
    }

    void SendUnresolvedAddressQueries() {
        for (const auto& service : srv_) {
            if (addresses_.find(service.second.target) != addresses_.end()) continue;
            for (int socket_fd : client_sockets_) {
                (void)mdns_query_send(socket_fd, MDNS_RECORDTYPE_A,
                    service.second.target.c_str(), service.second.target.size(),
                    send_buffer_.data(), send_buffer_.size(), 0);
                (void)mdns_query_send(socket_fd, MDNS_RECORDTYPE_AAAA,
                    service.second.target.c_str(), service.second.target.size(),
                    send_buffer_.data(), send_buffer_.size(), 0);
            }
        }
    }

    bool SendProbe() {
        mdns_query_t queries[2]{};
        queries[0] = {MDNS_RECORDTYPE_SRV, instance_fqdn_.c_str(), instance_fqdn_.size()};
        queries[1] = {MDNS_RECORDTYPE_TXT, instance_fqdn_.c_str(), instance_fqdn_.size()};
        bool sent = false;
        for (int socket_fd : client_sockets_) {
            sent |= mdns_multiquery_send(socket_fd, queries, 2, send_buffer_.data(),
                                         send_buffer_.size(), 0) >= 0;
        }
        return sent;
    }

    void AnswerQuestion(int socket_fd, const sockaddr* from, std::size_t addrlen,
                        std::uint16_t query_id, std::uint16_t rtype,
                        std::uint16_t rclass, const void* data, std::size_t size,
                        std::size_t name_offset) {
        if (!published_ || !request_.publication) return;
        char extracted[512];
        std::size_t offset = name_offset;
        const mdns_string_t raw = mdns_string_extract(data, size, &offset, extracted, sizeof(extracted));
        const std::string name = LowerDnsName(raw.str, raw.length);
        mdns_record_t answer{};
        std::vector<mdns_record_t> additional;
        const std::string instance =
            LowerDnsName(instance_fqdn_.c_str(), instance_fqdn_.size());
        const std::string hostname =
            LowerDnsName(hostname_fqdn_.c_str(), hostname_fqdn_.size());
        if (name == service_name_ &&
            (rtype == MDNS_RECORDTYPE_PTR || rtype == MDNS_RECORDTYPE_ANY)) {
            answer = PtrRecord();
            additional = AdditionalRecords();
        } else if (name == instance &&
                   (rtype == MDNS_RECORDTYPE_SRV || rtype == MDNS_RECORDTYPE_ANY)) {
            answer = SrvRecord();
            additional = AdditionalRecords();
            if (!additional.empty()) additional.erase(additional.begin());
        } else if (name == instance && rtype == MDNS_RECORDTYPE_TXT) {
            additional = AdditionalRecords();
            additional.erase(
                std::remove_if(additional.begin(), additional.end(),
                               [](const mdns_record_t& record) {
                                   return record.type != MDNS_RECORDTYPE_TXT;
                               }),
                additional.end());
            if (additional.empty()) return;
            answer = additional.front();
            additional.erase(additional.begin());
        } else if (name == hostname && rtype == MDNS_RECORDTYPE_A &&
                   local_addresses_.ipv4) {
            answer = AdditionalRecords().at(1);
        } else if (name == hostname && rtype == MDNS_RECORDTYPE_AAAA &&
                   local_addresses_.ipv6) {
            answer = AdditionalRecords().at(local_addresses_.ipv4 ? 2 : 1);
        } else {
            return;
        }
        if (rclass & MDNS_UNICAST_RESPONSE) {
            mdns_query_answer_unicast(socket_fd, from, addrlen, send_buffer_.data(),
                send_buffer_.size(), query_id, static_cast<mdns_record_type_t>(rtype),
                name.c_str(), name.size(), answer,
                nullptr, 0, additional.data(), additional.size());
        } else {
            mdns_query_answer_multicast(socket_fd, send_buffer_.data(), send_buffer_.size(),
                answer, nullptr, 0, additional.data(), additional.size());
        }
    }

    void ConsumeRecord(const sockaddr* from, std::uint16_t rtype, std::uint32_t ttl,
                       const void* data, std::size_t size, std::size_t name_offset,
                       std::size_t record_offset, std::size_t record_length) {
        char extracted[512];
        std::size_t offset = name_offset;
        const mdns_string_t raw = mdns_string_extract(data, size, &offset, extracted, sizeof(extracted));
        const std::string name = LowerDnsName(raw.str, raw.length);
        if (probing_ && name == LowerDnsName(instance_fqdn_.c_str(), instance_fqdn_.size()) &&
            (rtype == MDNS_RECORDTYPE_SRV || rtype == MDNS_RECORDTYPE_TXT)) {
            conflict_ = true;
            return;
        }
        if (rtype == MDNS_RECORDTYPE_PTR && name == service_name_) {
            char target[512];
            const mdns_string_t value = mdns_record_parse_ptr(
                data, size, record_offset, record_length, target, sizeof(target));
            const std::string instance = LowerDnsName(value.str, value.length);
            if (ttl) ptr_[instance] = {instance, Expiry(ttl)}; else ptr_.erase(instance);
        } else if (rtype == MDNS_RECORDTYPE_SRV) {
            char target[512];
            const mdns_record_srv_t value = mdns_record_parse_srv(
                data, size, record_offset, record_length, target, sizeof(target));
            if (ttl) srv_[name] = {LowerDnsName(value.name.str, value.name.length), value.port, Expiry(ttl)};
            else srv_.erase(name);
        } else if (rtype == MDNS_RECORDTYPE_TXT) {
            std::array<mdns_record_txt_t, 32> records{};
            const std::size_t count = mdns_record_parse_txt(
                data, size, record_offset, record_length, records.data(), records.size());
            std::vector<std::string> entries;
            for (std::size_t index = 0; index < count; ++index) {
                std::string entry(records[index].key.str, records[index].key.length);
                if (records[index].value.length) {
                    entry += '=';
                    entry.append(records[index].value.str, records[index].value.length);
                }
                entries.push_back(std::move(entry));
            }
            if (ttl) txt_[name] = {std::move(entries), Expiry(ttl)}; else txt_.erase(name);
        } else if (rtype == MDNS_RECORDTYPE_A) {
            sockaddr_in value{};
            mdns_record_parse_a(data, size, record_offset, record_length, &value);
            char address[INET_ADDRSTRLEN];
            if (inet_ntop(AF_INET, &value.sin_addr, address, sizeof(address))) {
                StoreAddress(name, {address, 0, 0}, ttl);
            }
        } else if (rtype == MDNS_RECORDTYPE_AAAA) {
            sockaddr_in6 value{};
            mdns_record_parse_aaaa(data, size, record_offset, record_length, &value);
            char address[INET6_ADDRSTRLEN];
            if (inet_ntop(AF_INET6, &value.sin6_addr, address, sizeof(address))) {
                std::uint32_t scope = 0;
                if (IN6_IS_ADDR_LINKLOCAL(&value.sin6_addr) && from && from->sa_family == AF_INET6)
                    scope = reinterpret_cast<const sockaddr_in6*>(from)->sin6_scope_id;
                StoreAddress(name, {address, 0, scope}, ttl);
            }
        }
    }

    void StoreAddress(const std::string& name, EndpointCandidate endpoint, std::uint32_t ttl) {
        auto& values = addresses_[name];
        values.erase(std::remove_if(values.begin(), values.end(), [&](const TimedAddress& current) {
            return SameEndpoint(current.endpoint, endpoint);
        }), values.end());
        if (ttl) values.push_back({std::move(endpoint), Expiry(ttl)});
    }

    void ExpireAndReconcile() {
        const auto now = Clock::now();
        for (auto it = ptr_.begin(); it != ptr_.end();) it = it->second.expiry <= now ? ptr_.erase(it) : std::next(it);
        for (auto it = srv_.begin(); it != srv_.end();) it = it->second.expiry <= now ? srv_.erase(it) : std::next(it);
        for (auto it = txt_.begin(); it != txt_.end();) it = it->second.expiry <= now ? txt_.erase(it) : std::next(it);
        for (auto it = addresses_.begin(); it != addresses_.end();) {
            auto& values = it->second;
            values.erase(std::remove_if(values.begin(), values.end(), [&](const TimedAddress& value) {
                return value.expiry <= now;
            }), values.end());
            it = values.empty() ? addresses_.erase(it) : std::next(it);
        }

        std::map<std::string, DiscoveredSession> next;
        for (const auto& pointer : ptr_) {
            const auto service = srv_.find(pointer.first);
            const auto metadata_txt = txt_.find(pointer.first);
            if (service == srv_.end() || metadata_txt == txt_.end()) continue;
            SessionAdvertisement metadata;
            std::string error;
            if (!discovery_protocol::ParseTxt(metadata_txt->second.entries, service->second.port,
                                              metadata, error)) continue;
            DiscoveredSession session; session.metadata = metadata;
            const auto address = addresses_.find(service->second.target);
            if (address != addresses_.end()) {
                for (const TimedAddress& timed : address->second) {
                    EndpointCandidate endpoint = timed.endpoint;
                    endpoint.port = service->second.port;
                    session.endpoints.push_back(std::move(endpoint));
                }
            }
            if (session.endpoints.empty())
                session.endpoints.push_back({service->second.target, service->second.port, 0});
            next[metadata.session_uuid] = std::move(session);
        }
        for (const auto& entry : next) {
            const auto old = visible_.find(entry.first);
            if (old == visible_.end()) events_.push_back({BackendEventKind::Added, entry.first, entry.second, {}});
            else if (!SameSession(old->second, entry.second)) events_.push_back({BackendEventKind::Updated, entry.first, entry.second, {}});
        }
        for (const auto& old : visible_) {
            if (next.find(old.first) == next.end())
                events_.push_back({BackendEventKind::Removed, old.first, std::nullopt, {}});
        }
        visible_ = std::move(next);
    }

    std::vector<int> client_sockets_;
    std::vector<int> service_sockets_;
    DiscoveryRequest request_;
    LocalAddresses local_addresses_;
    std::string service_name_ = discovery_protocol::kQualifiedServiceType;
    std::string instance_name_, instance_fqdn_, hostname_fqdn_;
    std::vector<std::string> txt_entries_;
    alignas(4) std::array<unsigned char, 4096> receive_buffer_{};
    alignas(4) std::array<unsigned char, 4096> send_buffer_{};
    std::map<std::string, TimedPtr> ptr_;
    std::map<std::string, TimedSrv> srv_;
    std::map<std::string, TimedTxt> txt_;
    std::map<std::string, std::vector<TimedAddress>> addresses_;
    std::map<std::string, DiscoveredSession> visible_;
    std::vector<BackendEvent> events_;
    bool probing_ = false, published_ = false, conflict_ = false;
    int probes_sent_ = 0;
    Clock::time_point next_probe_{}, next_browse_{}, next_announce_{}, next_interface_refresh_{};
};

}  // namespace

DiscoveryBackendFactory EmbeddedMdnsDiscoveryFactory() {
    return {DiscoveryBackendKind::EmbeddedMdns,
            [] { return std::make_unique<EmbeddedMdnsDiscoveryBackend>(); }};
}

}  // namespace lego_loco::network

#endif  // !_WIN32
