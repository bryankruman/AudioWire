// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/Discovery.h"
#import <Foundation/Foundation.h>
#import <Network/Network.h>
#include <arpa/inet.h>
#include <mutex>
@interface AwResolver : NSObject <NSNetServiceDelegate>
@property(nonatomic, strong) NSMutableDictionary<NSString*, NSNetService*>* services;
@property(nonatomic, copy) void (^found)(NSDictionary*);
- (void)resolve:(NSString*)name;
- (void)remove:(NSString*)name;
- (void)stop;
@end
@implementation AwResolver
- (instancetype)init {
    if ((self = [super init]))
        _services = [NSMutableDictionary dictionary];
    return self;
}
- (void)resolve:(NSString*)name {
    NSNetService* service = _services[name];
    if (!service) {
        service = [[NSNetService alloc] initWithDomain:@"local." type:@"_audiowire._udp." name:name];
        _services[name] = service;
        service.delegate = self;
    }
    [service resolveWithTimeout:5];
}
- (void)netServiceDidResolveAddress:(NSNetService*)service {
    NSString* host = nil;
    for (NSData* bytes in service.addresses) {
        const sockaddr* address = (const sockaddr*)bytes.bytes;
        char ip[INET6_ADDRSTRLEN] = {};
        if (address->sa_family == AF_INET) {
            inet_ntop(AF_INET, &((const sockaddr_in*)address)->sin_addr, ip, sizeof(ip));
            host = [NSString stringWithUTF8String:ip];
            break;
        }
    }
    if (!host)
        host = service.hostName;
    NSDictionary* txt =
        service.TXTRecordData ? [NSNetService dictionaryFromTXTRecordData:service.TXTRecordData] : @{};
    NSString* identity = [[NSString alloc] initWithData:txt[@"id"] encoding:NSUTF8StringEncoding];
    NSString* name = [[NSString alloc] initWithData:txt[@"name"] encoding:NSUTF8StringEncoding];
    NSString* version = [[NSString alloc] initWithData:txt[@"v"] encoding:NSUTF8StringEncoding];
    if (host && identity.length == 16 && [version isEqualToString:@"1"] && _found)
        _found(
            @{@"id" : identity, @"name" : name ?: service.name, @"host" : host, @"port" : @(service.port)});
}
- (void)remove:(NSString*)name {
    NSNetService* service = _services[name];
    if (!service)
        return;
    NSDictionary* txt =
        service.TXTRecordData ? [NSNetService dictionaryFromTXTRecordData:service.TXTRecordData] : @{};
    NSString* identity = [[NSString alloc] initWithData:txt[@"id"] encoding:NSUTF8StringEncoding];
    if (identity.length == 16 && _found)
        _found(@{@"id" : identity, @"name" : name, @"host" : @"", @"port" : @0});
    service.delegate = nil;
    [service stop];
    [_services removeObjectForKey:name];
}
- (void)stop {
    for (NSNetService* service in _services.allValues) {
        service.delegate = nil;
        [service stop];
    }
    [_services removeAllObjects];
    _found = nil;
}
@end
namespace aw {
class NwBrowserDiscovery final : public Discovery {
    nw_browser_t browser_ = nullptr;
    AwResolver* resolver_ = nil;
    std::function<void(Device)> found_;

  public:
    ~NwBrowserDiscovery() override {
        stop();
    }
    void advertise(const Device&, uint16_t) override {}
    void browse(std::function<void(Device)> found) override {
        stop();
        found_ = std::move(found);
        resolver_ = [AwResolver new];
        auto* self = this;
        resolver_.found = ^(NSDictionary* value) {
          if (self->found_)
              self->found_({[value[@"id"] UTF8String],
                            [value[@"name"] UTF8String],
                            [value[@"host"] UTF8String],
                            (uint16_t)[value[@"port"] unsignedIntValue]});
        };
        auto descriptor = nw_browse_descriptor_create_bonjour_service("_audiowire._udp", "local.");
        auto parameters = nw_parameters_create_secure_udp(NW_PARAMETERS_DISABLE_PROTOCOL,
                                                          NW_PARAMETERS_DEFAULT_CONFIGURATION);
        browser_ = nw_browser_create(descriptor, parameters);
        nw_browser_set_queue(browser_, dispatch_get_main_queue());
        __weak AwResolver* weakResolver = resolver_;
        nw_browser_set_browse_results_changed_handler(
            browser_, ^(nw_browse_result_t old, nw_browse_result_t result, bool) {
              auto entry = result ? result : old;
              if (!entry)
                  return;
              auto endpoint = nw_browse_result_copy_endpoint(entry);
              const char* name = nw_endpoint_get_bonjour_service_name(endpoint);
              if (name) {
                  auto service = [NSString stringWithUTF8String:name];
                  if (result)
                      [weakResolver resolve:service];
                  else
                      [weakResolver remove:service];
              }
            });
        nw_browser_start(browser_);
    }
    void stop() override {
        if (browser_) {
            nw_browser_cancel(browser_);
            browser_ = nullptr;
        }
        [resolver_ stop];
        resolver_ = nil;
        found_ = {};
    }
};
std::unique_ptr<Discovery> makeDiscovery() {
    return std::make_unique<NwBrowserDiscovery>();
}
} // namespace aw
