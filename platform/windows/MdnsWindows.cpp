// SPDX-License-Identifier: GPL-3.0-or-later
#include <winsock2.h>
#include "core/Discovery.h"
#include <atomic>
#include <juce_core/juce_core.h>
#include <mdns.h>
#include <thread>
#include <ws2tcpip.h>
namespace aw {
class MdnsWindows final : public Discovery {
    struct Service {
        MdnsWindows* owner;
        int socket = -1;
        std::string instance, hostname, id, name, aoo;
        mdns_record_t answer{}, extra[7]{};
    };

  public:
    ~MdnsWindows() override {
        stop();
    }
    void advertise(const Device& d, uint16_t audioPort) override {
        stop();
        device_ = d;
        audio_ = audioPort;
        running_ = true;
        thread_ = std::thread([this] { run(); });
    }
    void browse(std::function<void(Device)> found) override {
        found_ = std::move(found);
    }
    void stop() override {
        running_ = false;
        if (thread_.joinable())
            thread_.join();
    }

  private:
    static mdns_string_t ms(const std::string& s) {
        return {s.data(), s.size()};
    }
    static int query(int sock, const sockaddr* from, size_t addrlen, mdns_entry_type_t entry,
                     uint16_t queryId, uint16_t type, uint16_t klass, uint32_t, const void* data, size_t size,
                     size_t offset, size_t, size_t, size_t, void* user) {
        if (entry != MDNS_ENTRYTYPE_QUESTION)
            return 0;
        auto& s = *static_cast<Service*>(user);
        char text[512]{};
        auto name = mdns_string_extract(data, size, &offset, text, sizeof(text));
        std::string queryName(name.str, name.length);
        if (queryName != s.owner->type_ && queryName != s.instance && queryName != s.hostname)
            return 0;
        alignas(4) char out[4096]{};
        mdns_record_t answer = s.answer;
        if (queryName == s.instance && type == MDNS_RECORDTYPE_SRV)
            answer = s.extra[0];
        else if (queryName == s.hostname && type == MDNS_RECORDTYPE_A)
            answer = s.extra[1];
        else if (queryName == s.instance && type == MDNS_RECORDTYPE_TXT)
            answer = s.extra[2];
        if (klass & MDNS_UNICAST_RESPONSE)
            mdns_query_answer_unicast(sock, from, addrlen, out, sizeof(out), queryId,
                                      static_cast<mdns_record_type_t>(type), queryName.data(),
                                      queryName.size(), answer, nullptr, 0, s.extra, 7);
        else
            mdns_query_answer_multicast(sock, out, sizeof(out), answer, nullptr, 0, s.extra, 7);
        return 0;
    }
    void run() {
        WSADATA winsock{};
        WSAStartup(MAKEWORD(2, 2), &winsock);
        while (running_) {
            std::vector<std::unique_ptr<Service>> services;
            for (auto ip : juce::IPAddress::getAllAddresses()) {
                if (ip.isIPv6 || ip.toString().startsWith("127.") || ip.toString().startsWith("169.254."))
                    continue;
                sockaddr_in address{};
                address.sin_family = AF_INET;
                address.sin_port = htons(MDNS_PORT);
                inet_pton(AF_INET, ip.toString().toRawUTF8(), &address.sin_addr);
                int sock = mdns_socket_open_ipv4(&address);
                if (sock < 0)
                    continue;
                auto service = std::make_unique<Service>();
                auto& s = *service;
                s.owner = this;
                s.socket = sock;
                s.instance = device_.id + "." + type_;
                s.hostname = device_.id + ".local.";
                s.id = device_.id;
                s.name = device_.name;
                s.aoo = std::to_string(audio_);
                s.answer.name = ms(type_);
                s.answer.type = MDNS_RECORDTYPE_PTR;
                s.answer.data.ptr.name = ms(s.instance);
                s.extra[0].name = ms(s.instance);
                s.extra[0].type = MDNS_RECORDTYPE_SRV;
                s.extra[0].data.srv.name = ms(s.hostname);
                s.extra[0].data.srv.port = device_.port;
                s.extra[1].name = ms(s.hostname);
                s.extra[1].type = MDNS_RECORDTYPE_A;
                s.extra[1].data.a.addr = address;
                static const std::string v = "v", one = "1", id = "id", name = "name", aoo = "aoo",
                                         caps = "caps", abilities = "spk,mic";
                const mdns_string_t keys[] = {ms(v), ms(id), ms(name), ms(aoo), ms(caps)};
                const mdns_string_t values[] = {ms(one), ms(s.id), ms(s.name), ms(s.aoo), ms(abilities)};
                for (int i = 0; i < 5; ++i) {
                    s.extra[i + 2].name = ms(s.instance);
                    s.extra[i + 2].type = MDNS_RECORDTYPE_TXT;
                    s.extra[i + 2].data.txt.key = keys[i];
                    s.extra[i + 2].data.txt.value = values[i];
                }
                alignas(4) char out[4096]{};
                mdns_announce_multicast(sock, out, sizeof(out), s.answer, nullptr, 0, s.extra, 7);
                services.push_back(std::move(service));
            }
            // Reopen periodically to follow Wi-Fi/interface changes and PC resume.
            for (int tick = 0; running_ && tick < 300; ++tick) {
                for (auto& s : services) {
                    alignas(4) char data[4096]{};
                    mdns_socket_listen(s->socket, data, sizeof(data), query, s.get());
                }
                juce::Thread::sleep(100);
            }
            for (auto& s : services) {
                alignas(4) char out[4096]{};
                mdns_goodbye_multicast(s->socket, out, sizeof(out), s->answer, nullptr, 0, s->extra, 7);
                mdns_socket_close(s->socket);
            }
        }
        WSACleanup();
    }
    Device device_;
    uint16_t audio_ = 0;
    std::string type_ = "_audiowire._udp.local.";
    std::atomic<bool> running_{false};
    std::thread thread_;
    std::function<void(Device)> found_;
};
std::unique_ptr<Discovery> makeDiscovery() {
    return std::make_unique<MdnsWindows>();
}
} // namespace aw
