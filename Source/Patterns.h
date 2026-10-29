// Pattern memory for the four x0x devices. All cells are atomics so the
// editor can write while the audio thread reads without locks.
#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <atomic>

namespace rb
{
enum Device { Bass1 = 0, Bass2, R808, R909, NumDevices };

constexpr int kSteps = 16;
constexpr int kPatterns = 32;
constexpr int kDrumLanes = 12;

inline bool isBass (int d) { return d == Bass1 || d == Bass2; }

extern const char* const kDeviceIds[NumDevices];    // "bass1", "bass2", "r808", "r909"
extern const char* const kDeviceLabels[NumDevices]; // "BASS 1", ...

// Lane ids (first lane is always the accent lane "ac").
extern const char* const k808Lanes[kDrumLanes];
extern const char* const k909Lanes[kDrumLanes];
extern const char* const k808LaneNames[kDrumLanes];
extern const char* const k909LaneNames[kDrumLanes];
inline const char* const* drumLanes (int d) { return d == R808 ? k808Lanes : k909Lanes; }
inline const char* const* drumLaneNames (int d) { return d == R808 ? k808LaneNames : k909LaneNames; }

// Drum cell values (909 additionally supports per-voice accent and flam).
enum DrumCell : uint8_t { Off = 0, On = 1, Accent = 2, Flam = 3 };

struct BassStep
{
    int note = 0;    // 0..12 (C .. C')
    int octave = 0;  // -1, 0, +1
    bool gate = false, tie = false, accent = false, slide = false;

    uint16_t pack() const
    {
        return (uint16_t) ((note & 31) | ((octave + 1) & 3) << 5 | (gate ? 1 << 7 : 0)
                           | (tie ? 1 << 8 : 0) | (accent ? 1 << 9 : 0) | (slide ? 1 << 10 : 0));
    }
    static BassStep unpack (uint16_t v)
    {
        BassStep s;
        s.note = v & 31;
        s.octave = ((v >> 5) & 3) - 1;
        s.gate = (v >> 7) & 1;
        s.tie = (v >> 8) & 1;
        s.accent = (v >> 9) & 1;
        s.slide = (v >> 10) & 1;
        return s;
    }
};

struct BassPattern
{
    std::atomic<int> length { 16 };
    std::atomic<bool> shuffle { false };
    std::array<std::atomic<uint16_t>, kSteps> steps {};

    BassStep step (int i) const { return BassStep::unpack (steps[(size_t) i].load (std::memory_order_relaxed)); }
    void setStep (int i, const BassStep& s) { steps[(size_t) i].store (s.pack(), std::memory_order_relaxed); }
    void clear();
};

struct DrumPattern
{
    std::atomic<int> length { 16 };
    std::atomic<bool> shuffle { false };
    std::array<std::array<std::atomic<uint8_t>, kSteps>, kDrumLanes> cells {};

    uint8_t get (int lane, int step) const { return cells[(size_t) lane][(size_t) step].load (std::memory_order_relaxed); }
    void set (int lane, int step, uint8_t v) { cells[(size_t) lane][(size_t) step].store (v, std::memory_order_relaxed); }
    void clear();
};

struct PatternBank
{
    std::array<std::array<BassPattern, kPatterns>, 2> bass;
    std::array<std::array<DrumPattern, kPatterns>, 2> drums;

    BassPattern& bassPattern (int device, int index) { return bass[(size_t) (device - Bass1)][(size_t) index]; }
    DrumPattern& drumPattern (int device, int index) { return drums[(size_t) (device - R808)][(size_t) index]; }
    int length (int device, int index)
    {
        return isBass (device) ? bassPattern (device, index).length.load() : drumPattern (device, index).length.load();
    }
    bool shuffle (int device, int index)
    {
        return isBass (device) ? bassPattern (device, index).shuffle.load() : drumPattern (device, index).shuffle.load();
    }

    void clearAll();
    void loadDemo();

    std::unique_ptr<juce::XmlElement> toXml() const;
    void fromXml (const juce::XmlElement& xml);
};

} // namespace rb
