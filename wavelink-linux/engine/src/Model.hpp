// Mixer configuration: inputs, mixes and the input×mix cells, mirroring Wave Link 3.4's model
// (see spec/engine/NOTES.md for the source of every constant).
#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>
#include <array>
#include <vector>

namespace wl {

constexpr int kSoftwareSlots = 8;   // AudioMixer.MaxSoftwareInputs
constexpr int kHardwareSlots = 4;   // AudioMixer.MaxHardwareInputs
constexpr int kInputSlots = kSoftwareSlots + kHardwareSlots;
constexpr int kMixSlots = 5;        // AudioMixer.MaxOutputMixes
constexpr int kChannels = 2;

struct SlotDef {
    const char* id;        // Wave Link device id (WaveDeviceDefinitions)
    const char* name;      // Wave Link's constant name for the slot (display names are user data)
    const char* nodeName;  // PipeWire node.name of our virtual device for the slot
};

// Software input slots (virtual playback devices), ids handed out in this order.
extern const std::array<SlotDef, kSoftwareSlots> kSoftwareSlotDefs;
// Mix slots (virtual capture devices), ids handed out in this order.
extern const std::array<SlotDef, kMixSlots> kMixSlotDefs;

enum class InputKind { Software, Hardware };

struct EffectConfig {
    QString id;
    QString type;
    bool bypass = false;
    QMap<QString, double> params;
    QString name;  // user-chosen display name; empty shows the type's name
};

struct Input {
    QString id;
    int slot = -1;  // 0..7 software, 8..11 hardware
    InputKind kind = InputKind::Software;
    QString name;
    QString deviceNodeName;  // hardware inputs: PipeWire node.name of the capture device
    bool hidden = false;     // IsHiddenFromMixes
    float masterVolume = 1.0f;  // the device's own volume (Wave Link: endpoint volume), slider 0..1
    bool masterMuted = false;
    QList<EffectConfig> effects;  // processing order
};

struct Mix {
    QString id;
    int slot = -1;
    QString name;
    QStringList outputDevices;  // PipeWire node.names of playback devices fed by this mix
};

struct Cell {
    float volume = 1.0f;  // slider position 0..1
    bool muted = false;
    // Input is part of the mix (ToggleInputOnAudioMix). New channels start in no mix, as Wave Link's
    // AudioInputMixModelInitState.Default (IsActive: false); the user adds them mix by mix.
    bool active = false;
};

class Model {
public:
    std::vector<Input> inputs;  // display order
    std::vector<Mix> mixes;     // display order
    std::array<std::array<Cell, kMixSlots>, kInputSlots> cells{};
    bool manageDefaultSink = true;
    QMap<QString, QString> appRoutes;  // app key (binary or name) -> input id
    QString soundCheckOutput;          // sink node for Sound Check playback; empty picks one
    bool monitorOutputChosen = false;  // the Personal Mix output was set once (clearing it sticks)

    // Wave Link's first run: Personal, Stream and Chat mixes and a single System input.
    static Model defaults();

    QJsonObject toJson() const;
    static Model fromJson(const QJsonObject& json);

    Input* input(const QString& id);
    const Input* input(const QString& id) const;
    Mix* mix(const QString& id);
    const Mix* mix(const QString& id) const;

    int freeSoftwareSlot() const;  // -1 when all 8 are used
    int freeHardwareSlot() const;
    int freeMixSlot() const;

    // Linear gain the engine applies for a cell (0 when muted, inactive, hidden or absent).
    float cellGain(int inputSlot, int mixSlot) const;
};

// Software-mixer volume law: dB = 60 * v - 60 (AudioMixUtility.MapToMixerRange). The bottom of the
// fader is treated as silence.
QJsonArray effectsToJson(const QList<EffectConfig>& effects);

float sliderToDb(float v);
float sliderToGain(float v);

}  // namespace wl
