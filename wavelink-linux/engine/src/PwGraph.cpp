#include "PwGraph.hpp"

#include <QDebug>
#include <QSocketNotifier>
#include <QTimer>

#include <pipewire/extensions/metadata.h>
#include <pipewire/filter.h>
#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <spa/param/audio/raw.h>
#include <spa/param/props.h>
#include <spa/pod/builder.h>
#include <spa/pod/iter.h>
#include <spa/utils/result.h>
#include <spa/utils/string.h>

#include <QElapsedTimer>

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <map>
#include <optional>
#include <set>

#include "MixerCore.hpp"

namespace wl {

namespace {

const char* kChannelNames[kChannels] = {"FL", "FR"};

std::string str(const spa_dict* props, const char* key) {
    const char* v = props ? spa_dict_lookup(props, key) : nullptr;
    return v ? v : "";
}

uint32_t u32(const spa_dict* props, const char* key) {
    const char* v = props ? spa_dict_lookup(props, key) : nullptr;
    return v ? uint32_t(std::strtoul(v, nullptr, 10)) : SPA_ID_INVALID;
}

struct PwPort {
    uint32_t id = 0;
    uint32_t node = 0;
    bool output = false;
    std::string channel;
    std::string name;
};

}  // namespace

struct PwGraph::Impl {
    // A PipeWire object we created (virtual device or link) and must destroy ourselves.
    struct Owned {
        Impl* impl = nullptr;
        pw_proxy* proxy = nullptr;
        spa_hook listener{};
        bool failed = false;
    };

    PwGraph* q;
    MixerCore& mixer;

    pw_loop* loop = nullptr;
    pw_context* context = nullptr;
    pw_core* core = nullptr;
    pw_registry* registry = nullptr;
    pw_filter* filter = nullptr;
    pw_metadata* metadata = nullptr;
    spa_hook coreListener{}, registryListener{}, filterListener{};

    // Sound Check playback stream; `sample` only changes while no stream exists.
    pw_stream* sampleStream = nullptr;
    spa_hook sampleListener{};
    std::vector<float> sample;
    std::atomic<size_t> samplePos{0};
    std::atomic<bool> sampleDone{true};

    void* inPorts[kInputSlots][kChannels]{};
    void* outPorts[kMixSlots][kChannels]{};

    std::map<uint32_t, PwNode> nodes;
    std::map<uint32_t, PwPort> ports;
    std::map<uint32_t, std::pair<uint32_t, uint32_t>> links;  // link id -> (out port, in port)

    std::map<std::string, std::pair<VirtualDevice, std::unique_ptr<Owned>>> devices;
    std::map<std::pair<uint32_t, uint32_t>, std::unique_ptr<Owned>> ownedLinks;
    std::vector<LinkSpec> wantedLinks;

    std::string defaultSink;
    bool defaultSinkApplied = false;
    spa_hook metadataListener{};
    std::string userDefaultSink;  // last default sink seen that is not ours
    bool defaultsKnown = false;   // the server's current default sink has been reported
    bool restoreDefault = false;  // management was turned off: hand the default back

    // Nodes whose volume we follow (bound node proxies subscribed to Props).
    struct Bound {
        Impl* impl = nullptr;
        std::string name;
        pw_node* node = nullptr;
        spa_hook listener{};
        uint32_t channels = 2;
        bool known = false;
        float slider = 1.0f;
        bool mute = false;
    };
    struct Desired {
        float slider;
        bool mute;
        qint64 at;
    };
    std::map<uint32_t, std::unique_ptr<Bound>> bound;  // node global id -> binding
    std::set<std::string> volumeWatch;
    std::map<std::string, Desired> desired;  // values we set and have not seen confirmed yet

    // Links whose creation failed recently are retried only after a pause, so a link the server keeps
    // rejecting cannot turn reconcile into a busy loop.
    std::map<std::pair<uint32_t, uint32_t>, qint64> failedAt;
    QElapsedTimer clock;

    std::unique_ptr<QSocketNotifier> notifier;
    QTimer reconcileTimer;

    Impl(PwGraph* q, MixerCore& m) : q(q), mixer(m) {
        clock.start();
        reconcileTimer.setSingleShot(true);
        reconcileTimer.setInterval(20);
        QObject::connect(&reconcileTimer, &QTimer::timeout, q, [this] { reconcile(); });
    }

    ~Impl() {
        for (auto& [id, b] : bound) {
            spa_hook_remove(&b->listener);
            pw_proxy_destroy(reinterpret_cast<pw_proxy*>(b->node));
        }
        for (auto& [pair, o] : ownedLinks) release(o);
        for (auto& [name, entry] : devices) release(entry.second);
        if (sampleStream) pw_stream_destroy(sampleStream);
        if (metadata) {
            spa_hook_remove(&metadataListener);
            pw_proxy_destroy(reinterpret_cast<pw_proxy*>(metadata));
        }
        if (filter) pw_filter_destroy(filter);
        if (registry) pw_proxy_destroy(reinterpret_cast<pw_proxy*>(registry));
        if (core) pw_core_disconnect(core);
        if (context) pw_context_destroy(context);
        if (loop) {
            pw_loop_leave(loop);
            pw_loop_destroy(loop);
        }
    }

    void schedule() { reconcileTimer.start(); }

    // ---- owned proxies -------------------------------------------------------------------------

    static void ownedError(void* data, int, int res, const char* message) {
        auto* o = static_cast<Owned*>(data);
        qWarning() << "pipewire object failed:" << spa_strerror(res) << message;
        o->failed = true;
        o->impl->schedule();
    }

    static void ownedRemoved(void* data) {
        auto* o = static_cast<Owned*>(data);
        o->failed = true;
        o->impl->schedule();
    }

    std::unique_ptr<Owned> own(pw_proxy* proxy) {
        static const pw_proxy_events events = {
            .version = PW_VERSION_PROXY_EVENTS,
            .removed = ownedRemoved,
            .error = ownedError,
        };
        auto o = std::make_unique<Owned>();
        o->impl = this;
        o->proxy = proxy;
        if (proxy) pw_proxy_add_listener(proxy, &o->listener, &events, o.get());
        return o;
    }

    static void release(std::unique_ptr<Owned>& o) {
        if (!o) return;
        spa_hook_remove(&o->listener);
        if (o->proxy) pw_proxy_destroy(o->proxy);
        o.reset();
    }

    // ---- registry ------------------------------------------------------------------------------

    static void onGlobal(void* data, uint32_t id, uint32_t, const char* type, uint32_t, const spa_dict* props) {
        auto* d = static_cast<Impl*>(data);
        if (spa_streq(type, PW_TYPE_INTERFACE_Node)) {
            PwNode n;
            n.id = id;
            n.name = str(props, PW_KEY_NODE_NAME);
            n.description = str(props, PW_KEY_NODE_DESCRIPTION);
            n.mediaClass = str(props, PW_KEY_MEDIA_CLASS);
            n.appName = str(props, PW_KEY_APP_NAME);
            n.appBinary = str(props, PW_KEY_APP_PROCESS_BINARY);
            d->nodes[id] = n;
            if (d->volumeWatch.count(n.name)) d->bindNode(id, n.name);
            d->schedule();
            emit d->q->nodesChanged();
        } else if (spa_streq(type, PW_TYPE_INTERFACE_Port)) {
            PwPort p;
            p.id = id;
            p.node = u32(props, PW_KEY_NODE_ID);
            p.output = str(props, PW_KEY_PORT_DIRECTION) == "out";
            p.channel = str(props, PW_KEY_AUDIO_CHANNEL);
            p.name = str(props, PW_KEY_PORT_NAME);
            d->ports[id] = p;
            d->schedule();
        } else if (spa_streq(type, PW_TYPE_INTERFACE_Link)) {
            d->links[id] = {u32(props, PW_KEY_LINK_OUTPUT_PORT), u32(props, PW_KEY_LINK_INPUT_PORT)};
            emit d->q->nodesChanged();  // stream routing may have changed
        } else if (spa_streq(type, PW_TYPE_INTERFACE_Metadata) && str(props, PW_KEY_METADATA_NAME) == "default" &&
                   !d->metadata) {
            d->metadata = static_cast<pw_metadata*>(
                pw_registry_bind(d->registry, id, PW_TYPE_INTERFACE_Metadata, PW_VERSION_METADATA, 0));
            static const pw_metadata_events metadataEvents = {
                .version = PW_VERSION_METADATA_EVENTS,
                .property = Impl::onMetadataProperty,
            };
            pw_metadata_add_listener(d->metadata, &d->metadataListener, &metadataEvents, d);
            d->schedule();
        }
    }

    // "default" metadata: remember where system audio played before our sink became the default.
    static int onMetadataProperty(void* data, uint32_t subject, const char* key, const char*, const char* value) {
        auto* d = static_cast<Impl*>(data);
        if (subject != PW_ID_CORE || !key) return 0;
        const bool current = spa_streq(key, "default.audio.sink");
        if (!current && !spa_streq(key, "default.configured.audio.sink")) return 0;
        std::string name;  // value is {"name": "<node.name>"}
        if (const char* k = value ? std::strstr(value, "\"name\"") : nullptr)
            if (const char* open = std::strchr(k + 6, '"'))
                if (const char* close = std::strchr(open + 1, '"')) name.assign(open + 1, close);
        if (!name.empty() && name.rfind("wavelink.", 0) != 0 && (current || d->userDefaultSink.empty()) &&
            name != d->userDefaultSink) {
            d->userDefaultSink = name;
            emit d->q->nodesChanged();
        }
        if (current && !d->defaultsKnown) {
            d->defaultsKnown = true;
            d->schedule();
        }
        return 0;
    }

    static void onGlobalRemove(void* data, uint32_t id) {
        auto* d = static_cast<Impl*>(data);
        d->unbindNode(id);
        if (d->nodes.erase(id)) emit d->q->nodesChanged();
        d->ports.erase(id);
        if (auto it = d->links.find(id); it != d->links.end()) {
            // Someone removed a link we rely on; drop our handle so reconcile recreates it.
            if (auto own = d->ownedLinks.find(it->second); own != d->ownedLinks.end()) own->second->failed = true;
            d->links.erase(it);
            emit d->q->nodesChanged();
        }
        d->schedule();
    }

    // ---- node volumes --------------------------------------------------------------------------

    void bindNode(uint32_t id, const std::string& name) {
        if (bound.count(id)) return;
        static const pw_node_events events = {
            .version = PW_VERSION_NODE_EVENTS,
            .param = onNodeParam,
        };
        auto b = std::make_unique<Bound>();
        b->impl = this;
        b->name = name;
        b->node = static_cast<pw_node*>(pw_registry_bind(registry, id, PW_TYPE_INTERFACE_Node, PW_VERSION_NODE, 0));
        if (!b->node) return;
        pw_node_add_listener(b->node, &b->listener, &events, b.get());
        uint32_t ids[] = {SPA_PARAM_Props};
        pw_node_subscribe_params(b->node, ids, 1);
        bound[id] = std::move(b);
    }

    void unbindNode(uint32_t id) {
        auto it = bound.find(id);
        if (it == bound.end()) return;
        spa_hook_remove(&it->second->listener);
        pw_proxy_destroy(reinterpret_cast<pw_proxy*>(it->second->node));
        bound.erase(it);
    }

    static void onNodeParam(void* data, int, uint32_t id, uint32_t, uint32_t, const spa_pod* param) {
        auto* b = static_cast<Bound*>(data);
        if (id != SPA_PARAM_Props || !param || !spa_pod_is_object(param)) return;
        float volumes[SPA_AUDIO_MAX_CHANNELS];
        uint32_t n = 0;
        bool mute = b->mute;
        bool haveMute = false;
        const auto* obj = reinterpret_cast<const spa_pod_object*>(param);
        const spa_pod_prop* prop;
        SPA_POD_OBJECT_FOREACH(obj, prop) {
            if (prop->key == SPA_PROP_channelVolumes)
                n = spa_pod_copy_array(&prop->value, SPA_TYPE_Float, volumes, SPA_AUDIO_MAX_CHANNELS);
            else if (prop->key == SPA_PROP_mute)
                haveMute = spa_pod_get_bool(&prop->value, &mute) == 0;
        }
        if (n == 0 && !haveMute) return;
        float slider = b->slider;
        if (n > 0) {
            float sum = 0;
            for (uint32_t i = 0; i < n; i++) sum += volumes[i];
            b->channels = n;
            slider = std::cbrt(std::max(0.0f, sum / float(n)));
        }
        b->impl->onProps(*b, slider, mute);
    }

    void onProps(Bound& b, float slider, bool mute) {
        if (auto want = desired.find(b.name); want != desired.end()) {
            const bool matches = std::fabs(want->second.slider - slider) < 0.01f && want->second.mute == mute;
            if (!b.known && !matches) {  // first sight of the node: enforce the configured value
                b.known = true;
                want->second.at = clock.elapsed();
                applyVolume(b, want->second.slider, want->second.mute);
                return;
            }
            if (matches) desired.erase(want);
            else if (clock.elapsed() - want->second.at < 1000) return;  // stale event from before our change
            else desired.erase(want);
        }
        b.known = true;
        if (std::fabs(b.slider - slider) < 0.0005f && b.mute == mute) return;
        b.slider = slider;
        b.mute = mute;
        emit q->nodeVolumeChanged(QString::fromStdString(b.name), slider, mute);
    }

    void applyVolume(Bound& b, float slider, bool mute) {
        uint8_t buffer[1024];
        spa_pod_builder builder;
        spa_pod_builder_init(&builder, buffer, sizeof buffer);
        spa_pod_frame frame;
        spa_pod_builder_push_object(&builder, &frame, SPA_TYPE_OBJECT_Props, SPA_PARAM_Props);
        float volumes[SPA_AUDIO_MAX_CHANNELS];
        const float linear = slider * slider * slider;
        for (uint32_t i = 0; i < b.channels; i++) volumes[i] = linear;
        spa_pod_builder_prop(&builder, SPA_PROP_channelVolumes, 0);
        spa_pod_builder_array(&builder, sizeof(float), SPA_TYPE_Float, b.channels, volumes);
        spa_pod_builder_prop(&builder, SPA_PROP_mute, 0);
        spa_pod_builder_bool(&builder, mute);
        auto* pod = static_cast<spa_pod*>(spa_pod_builder_pop(&builder, &frame));
        pw_node_set_param(b.node, SPA_PARAM_Props, 0, pod);
    }

    // ---- filter --------------------------------------------------------------------------------

    static void onProcess(void* data, spa_io_position* position) {
        auto* d = static_cast<Impl*>(data);
        const uint32_t frames = position->clock.duration;
        if (position->clock.rate.denom) d->mixer.setSampleRate(position->clock.rate.denom);
        const float* in[kInputSlots][kChannels];
        float* out[kMixSlots][kChannels];
        for (int i = 0; i < kInputSlots; i++)
            for (int c = 0; c < kChannels; c++)
                in[i][c] = static_cast<const float*>(pw_filter_get_dsp_buffer(d->inPorts[i][c], frames));
        for (int m = 0; m < kMixSlots; m++)
            for (int c = 0; c < kChannels; c++)
                out[m][c] = static_cast<float*>(pw_filter_get_dsp_buffer(d->outPorts[m][c], frames));
        d->mixer.process(in, out, frames);
    }

    // Data thread: mono sample to interleaved stereo, silence after the end.
    static void onSampleProcess(void* data) {
        auto* d = static_cast<Impl*>(data);
        pw_buffer* b = pw_stream_dequeue_buffer(d->sampleStream);
        if (!b) return;
        spa_data& out = b->buffer->datas[0];
        auto* dst = static_cast<float*>(out.data);
        const uint32_t stride = 2 * sizeof(float);
        uint32_t frames = dst ? out.maxsize / stride : 0;
        if (b->requested) frames = std::min(frames, uint32_t(b->requested));
        size_t pos = d->samplePos.load(std::memory_order_relaxed);
        for (uint32_t i = 0; i < frames; i++) dst[2 * i] = dst[2 * i + 1] = pos < d->sample.size() ? d->sample[pos++] : 0.0f;
        d->samplePos.store(pos, std::memory_order_relaxed);
        if (pos >= d->sample.size()) d->sampleDone.store(true, std::memory_order_release);
        out.chunk->offset = 0;
        out.chunk->stride = stride;
        out.chunk->size = frames * stride;
        pw_stream_queue_buffer(d->sampleStream, b);
    }

    static void onFilterState(void*, pw_filter_state, pw_filter_state state, const char* error) {
        if (state == PW_FILTER_STATE_ERROR) qWarning() << "engine filter error:" << (error ? error : "");
    }

    void* addPort(pw_direction direction, const std::string& name, const char* channel) {
        return pw_filter_add_port(filter, direction, PW_FILTER_PORT_FLAG_MAP_BUFFERS, sizeof(uint32_t),
                                  pw_properties_new(PW_KEY_FORMAT_DSP, "32 bit float mono audio",
                                                    PW_KEY_PORT_NAME, name.c_str(), PW_KEY_AUDIO_CHANNEL,
                                                    channel, nullptr),
                                  nullptr, 0);
    }

    // ---- reconciliation ------------------------------------------------------------------------

    const PwNode* nodeByName(const std::string& name) const {
        for (const auto& [id, n] : nodes)
            if (n.name == name) return &n;
        return nullptr;
    }

    std::optional<uint32_t> findPort(uint32_t node, bool output, const std::string& name,
                                     const std::string& channel) const {
        std::vector<const PwPort*> candidates;
        for (const auto& [id, p] : ports)
            if (p.node == node && p.output == output) candidates.push_back(&p);
        for (const PwPort* p : candidates)
            if (!name.empty() ? p->name == name : p->channel == channel) return p->id;
        if (!name.empty()) return std::nullopt;
        if (candidates.size() == 1) return candidates.front()->id;  // mono device
        for (const PwPort* p : candidates)
            if (p->channel == "MONO") return p->id;
        return std::nullopt;
    }

    bool linkExists(std::pair<uint32_t, uint32_t> ports) const {
        for (const auto& [id, l] : links)
            if (l == ports) return true;
        return false;
    }

    void reconcile() {
        if (!core) return;

        for (auto& [name, entry] : devices) {
            if (entry.second && entry.second->failed) release(entry.second);
            if (!entry.second) entry.second = own(createDevice(entry.first));
        }

        std::set<std::pair<uint32_t, uint32_t>> wanted;
        std::map<std::pair<uint32_t, uint32_t>, std::pair<uint32_t, uint32_t>> nodesOf;
        for (const auto& spec : wantedLinks) {
            const PwNode* out = nodeByName(spec.outNode);
            const PwNode* in = nodeByName(spec.inNode);
            if (!out || !in) continue;
            auto outPort = findPort(out->id, true, spec.outPort, spec.outChannel);
            auto inPort = findPort(in->id, false, spec.inPort, spec.inChannel);
            if (!outPort || !inPort) continue;
            wanted.insert({*outPort, *inPort});
            nodesOf[{*outPort, *inPort}] = {out->id, in->id};
        }
        for (auto it = ownedLinks.begin(); it != ownedLinks.end();) {
            if (!wanted.count(it->first) || it->second->failed) {
                if (it->second->failed) failedAt[it->first] = clock.elapsed();
                release(it->second);
                it = ownedLinks.erase(it);
            } else {
                ++it;
            }
        }
        bool retryLater = false;
        for (const auto& pair : wanted) {
            if (ownedLinks.count(pair) || linkExists(pair)) continue;
            if (auto f = failedAt.find(pair); f != failedAt.end() && clock.elapsed() - f->second < 2000) {
                retryLater = true;
                continue;
            }
            const auto [outNode, inNode] = nodesOf[pair];
            pw_properties* props = pw_properties_new(PW_KEY_OBJECT_LINGER, "false", nullptr);
            pw_properties_setf(props, PW_KEY_LINK_OUTPUT_NODE, "%u", outNode);
            pw_properties_setf(props, PW_KEY_LINK_OUTPUT_PORT, "%u", pair.first);
            pw_properties_setf(props, PW_KEY_LINK_INPUT_NODE, "%u", inNode);
            pw_properties_setf(props, PW_KEY_LINK_INPUT_PORT, "%u", pair.second);
            auto* proxy = static_cast<pw_proxy*>(pw_core_create_object(
                core, "link-factory", PW_TYPE_INTERFACE_Link, PW_VERSION_LINK, &props->dict, 0));
            pw_properties_free(props);
            ownedLinks[pair] = own(proxy);
        }
        if (retryLater) QTimer::singleShot(2000, q, [this] { schedule(); });

        if (metadata && restoreDefault) {
            restoreDefault = false;
            const std::string value = "{ \"name\": \"" + userDefaultSink + "\" }";
            pw_metadata_set_property(metadata, PW_ID_CORE, "default.configured.audio.sink",
                                     userDefaultSink.empty() ? nullptr : "Spa:String:JSON",
                                     userDefaultSink.empty() ? nullptr : value.c_str());
        }
        if (metadata && defaultsKnown && !defaultSink.empty() && !defaultSinkApplied && nodeByName(defaultSink)) {
            const std::string value = "{ \"name\": \"" + defaultSink + "\" }";
            pw_metadata_set_property(metadata, PW_ID_CORE, "default.configured.audio.sink", "Spa:String:JSON",
                                     value.c_str());
            defaultSinkApplied = true;
        }
    }

    pw_proxy* createDevice(const VirtualDevice& dev) {
        pw_properties* props = pw_properties_new(
            "factory.name", "support.null-audio-sink", PW_KEY_NODE_NAME, dev.nodeName.c_str(),
            PW_KEY_NODE_DESCRIPTION, dev.description.c_str(), PW_KEY_MEDIA_CLASS,
            dev.source ? "Audio/Source/Virtual" : "Audio/Sink", "audio.position", "FL,FR",
            "monitor.channel-volumes", "true", PW_KEY_NODE_VIRTUAL, "true", PW_KEY_OBJECT_LINGER, "false",
            "device.icon-name", dev.source ? "audio-input-microphone" : "audio-speakers", nullptr);
        auto* proxy = static_cast<pw_proxy*>(
            pw_core_create_object(core, "adapter", PW_TYPE_INTERFACE_Node, PW_VERSION_NODE, &props->dict, 0));
        pw_properties_free(props);
        return proxy;
    }
};

PwGraph::PwGraph(MixerCore& mixer, QObject* parent) : QObject(parent), d(std::make_unique<Impl>(this, mixer)) {}

PwGraph::~PwGraph() = default;

std::string PwGraph::engineNodeName() { return "wavelink.engine"; }

std::string PwGraph::inputPortName(int slot, int channel) {
    return "in_" + std::to_string(slot) + "_" + kChannelNames[channel];
}

std::string PwGraph::outputPortName(int mixSlot, int channel) {
    return "out_" + std::to_string(mixSlot) + "_" + kChannelNames[channel];
}

bool PwGraph::start(QString* error) {
    d->loop = pw_loop_new(nullptr);
    if (!d->loop) {
        *error = "pw_loop_new failed";
        return false;
    }
    pw_loop_enter(d->loop);
    d->notifier = std::make_unique<QSocketNotifier>(pw_loop_get_fd(d->loop), QSocketNotifier::Read);
    connect(d->notifier.get(), &QSocketNotifier::activated, this, [this] { pw_loop_iterate(d->loop, 0); });

    d->context = pw_context_new(d->loop, nullptr, 0);
    d->core = d->context ? pw_context_connect(d->context, nullptr, 0) : nullptr;
    if (!d->core) {
        *error = "cannot connect to PipeWire";
        return false;
    }
    static const pw_registry_events registryEvents = {
        .version = PW_VERSION_REGISTRY_EVENTS,
        .global = Impl::onGlobal,
        .global_remove = Impl::onGlobalRemove,
    };
    d->registry = pw_core_get_registry(d->core, PW_VERSION_REGISTRY, 0);
    pw_registry_add_listener(d->registry, &d->registryListener, &registryEvents, d.get());

    static const pw_filter_events filterEvents = {
        .version = PW_VERSION_FILTER_EVENTS,
        .state_changed = Impl::onFilterState,
        .process = Impl::onProcess,
    };
    d->filter = pw_filter_new(d->core, "Wave Link Engine",
                              pw_properties_new(PW_KEY_MEDIA_TYPE, "Audio", PW_KEY_MEDIA_CATEGORY, "Filter",
                                                PW_KEY_MEDIA_ROLE, "DSP", PW_KEY_NODE_NAME,
                                                engineNodeName().c_str(), PW_KEY_NODE_DESCRIPTION,
                                                "Wave Link Engine", "node.always-process", "true", nullptr));
    pw_filter_add_listener(d->filter, &d->filterListener, &filterEvents, d.get());
    for (int i = 0; i < kInputSlots; i++)
        for (int c = 0; c < kChannels; c++)
            d->inPorts[i][c] = d->addPort(PW_DIRECTION_INPUT, inputPortName(i, c), kChannelNames[c]);
    for (int m = 0; m < kMixSlots; m++)
        for (int c = 0; c < kChannels; c++)
            d->outPorts[m][c] = d->addPort(PW_DIRECTION_OUTPUT, outputPortName(m, c), kChannelNames[c]);
    if (pw_filter_connect(d->filter, PW_FILTER_FLAG_RT_PROCESS, nullptr, 0) < 0) {
        *error = "cannot connect the engine filter";
        return false;
    }
    pw_loop_iterate(d->loop, 0);
    return true;
}

void PwGraph::setVirtualDevices(const std::vector<VirtualDevice>& devices) {
    std::set<std::string> names;
    for (const auto& dev : devices) {
        names.insert(dev.nodeName);
        auto it = d->devices.find(dev.nodeName);
        if (it != d->devices.end() && (it->second.first.description != dev.description ||
                                       it->second.first.source != dev.source)) {
            Impl::release(it->second.second);  // properties are fixed at creation: recreate
            it->second.first = dev;
        } else if (it == d->devices.end()) {
            d->devices[dev.nodeName].first = dev;
        }
    }
    for (auto it = d->devices.begin(); it != d->devices.end();) {
        if (!names.count(it->first)) {
            Impl::release(it->second.second);
            it = d->devices.erase(it);
        } else {
            ++it;
        }
    }
    d->schedule();
}

void PwGraph::setLinks(std::vector<LinkSpec> links) {
    d->wantedLinks = std::move(links);
    d->schedule();
}

void PwGraph::setDefaultSink(const std::string& nodeName) {
    if (nodeName.empty() && !d->defaultSink.empty() && d->defaultSinkApplied) d->restoreDefault = true;
    if (nodeName != d->defaultSink) d->defaultSinkApplied = false;
    d->defaultSink = nodeName;
    d->schedule();
}

void PwGraph::setVolumeWatch(const std::vector<std::string>& nodeNames) {
    d->volumeWatch = std::set<std::string>(nodeNames.begin(), nodeNames.end());
    std::vector<uint32_t> drop;
    for (const auto& [id, b] : d->bound)
        if (!d->volumeWatch.count(b->name)) drop.push_back(id);
    for (uint32_t id : drop) d->unbindNode(id);
    for (const auto& [id, n] : d->nodes)
        if (d->volumeWatch.count(n.name)) d->bindNode(id, n.name);
}

void PwGraph::setNodeVolume(const std::string& nodeName, float slider, bool mute) {
    slider = std::clamp(slider, 0.0f, 1.0f);
    d->desired[nodeName] = {slider, mute, d->clock.elapsed()};
    for (auto& [id, b] : d->bound)
        if (b->name == nodeName && b->known) d->applyVolume(*b, slider, mute);
}

void PwGraph::playSample(std::vector<float> samples, uint32_t rate, const std::string& targetNode) {
    stopSample();
    d->sample = std::move(samples);
    d->samplePos.store(0, std::memory_order_relaxed);
    d->sampleDone.store(d->sample.empty(), std::memory_order_release);
    pw_properties* props = pw_properties_new(PW_KEY_MEDIA_TYPE, "Audio", PW_KEY_MEDIA_CATEGORY, "Playback",
                                             PW_KEY_MEDIA_ROLE, "Music", PW_KEY_NODE_NAME, "wavelink.soundcheck",
                                             PW_KEY_NODE_DESCRIPTION, "Wave Link Sound Check", PW_KEY_APP_NAME,
                                             "Wave Link", nullptr);
    if (!targetNode.empty()) pw_properties_set(props, PW_KEY_TARGET_OBJECT, targetNode.c_str());
    static const pw_stream_events events = {
        .version = PW_VERSION_STREAM_EVENTS,
        .process = Impl::onSampleProcess,
    };
    d->sampleStream = pw_stream_new(d->core, "Wave Link Sound Check", props);
    pw_stream_add_listener(d->sampleStream, &d->sampleListener, &events, d.get());
    uint8_t buffer[512];
    spa_pod_builder builder = SPA_POD_BUILDER_INIT(buffer, sizeof buffer);
    spa_audio_info_raw info{};
    info.format = SPA_AUDIO_FORMAT_F32;
    info.rate = rate;
    info.channels = 2;
    info.position[0] = SPA_AUDIO_CHANNEL_FL;
    info.position[1] = SPA_AUDIO_CHANNEL_FR;
    const spa_pod* params[] = {spa_format_audio_raw_build(&builder, SPA_PARAM_EnumFormat, &info)};
    const auto flags = pw_stream_flags(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS | PW_STREAM_FLAG_RT_PROCESS);
    if (pw_stream_connect(d->sampleStream, PW_DIRECTION_OUTPUT, PW_ID_ANY, flags, params, 1) < 0) {
        qWarning() << "cannot start the Sound Check playback";
        stopSample();
    }
}

void PwGraph::stopSample() {
    if (!d->sampleStream) return;
    spa_hook_remove(&d->sampleListener);
    pw_stream_destroy(d->sampleStream);  // waits for the data thread to let go of it
    d->sampleStream = nullptr;
    d->sampleDone.store(true, std::memory_order_release);
}

bool PwGraph::samplePlaying() const {
    return d->sampleStream && !d->sampleDone.load(std::memory_order_acquire);
}

double PwGraph::sampleProgress() const {
    return d->sample.empty() ? 0.0 : double(d->samplePos.load(std::memory_order_relaxed)) / double(d->sample.size());
}

std::string PwGraph::userDefaultSink() const { return d->userDefaultSink; }

std::vector<AppStream> PwGraph::appStreams() const {
    std::vector<AppStream> out;
    for (const auto& [id, n] : d->nodes) {
        if (n.mediaClass != "Stream/Output/Audio" || n.name.rfind("wavelink.", 0) == 0) continue;
        AppStream s{id, n.appName, n.appBinary, {}};
        for (const auto& [linkId, l] : d->links) {
            auto from = d->ports.find(l.first);
            auto to = d->ports.find(l.second);
            if (from == d->ports.end() || to == d->ports.end() || from->second.node != id) continue;
            if (auto target = d->nodes.find(to->second.node); target != d->nodes.end()) {
                s.target = target->second.name;
                break;
            }
        }
        out.push_back(s);
    }
    return out;
}

void PwGraph::setStreamTarget(uint32_t streamId, const std::string& targetNode) {
    if (!d->metadata) return;
    pw_metadata_set_property(d->metadata, streamId, "target.object", nullptr,
                             targetNode.empty() ? nullptr : targetNode.c_str());
}

std::vector<PwNode> PwGraph::nodes() const {
    std::vector<PwNode> out;
    for (const auto& [id, n] : d->nodes) out.push_back(n);
    return out;
}

}  // namespace wl
