// PipeWire side of the engine: the mixing filter node, the virtual devices apps see, and the links
// between them. All PipeWire objects live on the Qt main thread (the PipeWire loop is driven from a
// QSocketNotifier); only the filter's process callback runs on PipeWire's data thread.
#pragma once

#include <QObject>
#include <QString>
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace wl {

class MixerCore;

struct PwNode {
    uint32_t id = 0;
    std::string name;
    std::string description;
    std::string mediaClass;
    std::string appName;
    std::string appBinary;
};

// A link the engine wants. Each side picks a port by exact port.name when given, otherwise by
// audio.channel; a mono node (single port in that direction) satisfies any channel.
struct LinkSpec {
    std::string outNode, outPort, outChannel;
    std::string inNode, inPort, inChannel;
};

// An application playback stream and the node it currently plays to.
struct AppStream {
    uint32_t id = 0;
    std::string appName;
    std::string binary;
    std::string target;  // node.name of the sink it is linked to, empty if unlinked
};

struct VirtualDevice {
    std::string nodeName;
    std::string description;
    bool source = false;  // Audio/Source/Virtual (apps record from it) instead of Audio/Sink
};

class PwGraph : public QObject {
    Q_OBJECT
public:
    explicit PwGraph(MixerCore& mixer, QObject* parent = nullptr);
    ~PwGraph() override;

    bool start(QString* error);

    void setVirtualDevices(const std::vector<VirtualDevice>& devices);
    void setLinks(std::vector<LinkSpec> links);
    // Makes nodeName the configured default sink once it exists (and the current defaults are known);
    // empty to stop managing it.
    void setDefaultSink(const std::string& nodeName);
    // The default sink as last seen before it was one of ours: where system audio played before.
    std::string userDefaultSink() const;

    // Desktop-style node volume: slider 0..1 where PipeWire's linear channel volume is slider^3 (what
    // KDE and pavucontrol show). Watched nodes report changes; a value set for a node that does not
    // exist yet is applied as soon as it appears.
    void setVolumeWatch(const std::vector<std::string>& nodeNames);
    void setNodeVolume(const std::string& nodeName, float slider, bool mute);

    std::vector<AppStream> appStreams() const;
    // Ask the session manager to move a stream to targetNode (empty: back to the default).
    void setStreamTarget(uint32_t streamId, const std::string& targetNode);

    // Sound Check playback: plays mono samples on targetNode (empty: the default sink) as its own
    // stream. samplePlaying() turns false once the end has been sent or after stopSample().
    void playSample(std::vector<float> samples, uint32_t rate, const std::string& targetNode);
    void stopSample();
    bool samplePlaying() const;
    double sampleProgress() const;  // 0..1

    std::vector<PwNode> nodes() const;
    static std::string engineNodeName();
    static std::string inputPortName(int slot, int channel);
    static std::string outputPortName(int mixSlot, int channel);

signals:
    void nodesChanged();
    void nodeVolumeChanged(const QString& nodeName, float slider, bool mute);

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};

}  // namespace wl
