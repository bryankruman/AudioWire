// SPDX-License-Identifier: GPL-3.0-or-later
// AOO integration adapted from SonoBus by Jesse Chappell (Sonosaurus LLC).
#include "Transport.h"
#include <algorithm>
#include <aoo/aoo_opus.h>
#include <aoo/aoo_pcm.h>
#include <mutex>
#include <stdexcept>
namespace aw {
Transport::Transport(Role r, Quality q, double rate, int frames, std::function<void(const char*, int)> send)
    : role_(r), channels_(r == Role::pc ? 2 : 1), rate_(static_cast<int>(rate)), send_(std::move(send)) {
    static std::once_flag init;
    std::call_once(init, [] { aoo_initialize(); });
    source_.reset(aoo::isource::create(1));
    sink_.reset(aoo::isink::create(1));
    if (!source_ || !sink_ || !source_->setup(rate_, frames, channels_) ||
        !sink_->setup(rate_, frames, r == Role::pc ? 1 : 2))
        throw std::runtime_error("Audio engine could not start");
    source_->set_dynamic_resampling(1);
    sink_->set_dynamic_resampling(1);
    sink_->set_buffersize(20);
    source_->set_buffersize(30);
    source_->set_packetsize(1100);
    source_->set_respect_codec_change_requests(0);
    setQuality(q);
}
Transport::~Transport() {
    stop();
}
void Transport::start() {
    source_->add_sink(this, 1, reply);
    source_->start();
}
void Transport::stop() {
    if (source_) {
        source_->stop();
        source_->remove_all();
    }
    if (sink_)
        sink_->uninvite_all();
}
void Transport::setQuality(Quality q) {
    auto p = preset(q);
    baselineMs_ = p.bufferMs;
    if (automatic_ && buffer_ < baselineMs_) {
        buffer_ = baselineMs_;
        sink_->set_buffersize(buffer_);
    }
    if (p.pcm) {
        aoo_format_pcm f{{AOO_CODEC_PCM, channels_, 48000, 480}, AOO_PCM_INT16};
        if (!source_->set_format(f.header))
            throw std::runtime_error("Audio quality unavailable");
    } else {
        aoo_format_opus f{{AOO_CODEC_OPUS, channels_, 48000, 480},
                          role_ == Role::pc ? p.speakerBitrate : p.micBitrate,
                          5,
                          role_ == Role::pc ? OPUS_SIGNAL_MUSIC : OPUS_SIGNAL_VOICE,
                          OPUS_APPLICATION_AUDIO};
        if (!source_->set_format(f.header))
            throw std::runtime_error("Audio quality unavailable");
    }
}
void Transport::setBufferMs(int ms) {
    automatic_ = ms == 0;
    buffer_ = ms == 0 ? baselineMs_ : std::clamp(ms, 10, 100);
    sink_->set_buffersize(buffer_);
}
void Transport::process(const float* const* in, float* const* out, int n) noexcept {
    const auto t = aoo_osctime_get();
    source_->process(const_cast<const float**>(in), n, t);
    const int channels = role_ == Role::pc ? 1 : 2;
    for (int ch = 0; ch < channels; ++ch)
        std::fill_n(out[ch], n, 0.0f);
    sink_->process(const_cast<float**>(out), n, t);
}
int32_t Transport::reply(void* user, const char* data, int32_t n) {
    auto* self = static_cast<Transport*>(user);
    self->send_(data, n);
    return n;
}
void Transport::receive(const char* data, int n) {
    if (!data || n <= 0)
        return;
    int type = 0, id = 0;
    if (!aoo_parse_pattern(data, n, &type, &id))
        return;
    if (type == AOO_TYPE_SOURCE)
        source_->handle_message(data, n, this, reply);
    else
        sink_->handle_message(data, n, this, reply);
}
int32_t Transport::events(void* user, const aoo_event** list, int32_t n) {
    auto* self = static_cast<Transport*>(user);
    for (int i = 0; i < n; ++i)
        if (list[i]->type == AOO_BLOCK_LOST_EVENT)
            self->lost_.fetch_add(
                static_cast<std::uint64_t>(reinterpret_cast<const aoo_block_lost_event*>(list[i])->count));
    return 0;
}
void Transport::sendPending() {
    source_->send();
    sink_->send();
    source_->handle_events(events, this);
    sink_->handle_events(events, this);
    if (!automatic_)
        return;
    const auto loss = lost_.load();
    if (loss != lastLoss_) {
        buffer_ = std::min(buffer_.load() + 5, 80);
        sink_->set_buffersize(buffer_);
        stableTicks_ = 0;
        lastLoss_ = loss;
    } else if (++stableTicks_ >= 5000) {
        stableTicks_ = 0;
        if (buffer_ > baselineMs_) {
            buffer_ = buffer_ - 1;
            sink_->set_buffersize(buffer_);
        }
    }
}
} // namespace aw
