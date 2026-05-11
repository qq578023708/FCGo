#pragma once
#include "../types.h"
#include <functional>
#include <array>
#include <vector>

// ============================================================
// APU — NES Audio Processing Unit
// Generates PCM audio at 44100 Hz
// ============================================================

struct PulseChannel {
    bool enabled{false};
    u8   duty{0};          // 0-3
    bool haltLoop{false};  // length counter halt / envelope loop
    bool constVol{false};
    u8   volume{0};        // constant volume or envelope period
    u8   envelopeDiv{0};
    u8   envelopeCounter{0};
    bool envelopeStart{false};

    u16  timerPeriod{0};
    u16  timerCounter{0};
    u8   sequenceStep{0};

    u8   lengthCounter{0};

    // Sweep
    bool sweepEnabled{false};
    bool sweepNegate{false};
    u8   sweepPeriod{0};
    u8   sweepShift{0};
    u8   sweepCounter{0};
    bool sweepReload{false};
    bool onesComplement{false}; // true for pulse 1

    void clockTimer();
    void clockLengthCounter();
    void clockEnvelope();
    void clockSweep();
    u8   output() const;
};

struct TriangleChannel {
    bool enabled{false};
    bool control{false};
    u8   linearPeriod{0};
    u8   linearCounter{0};
    bool linearReload{false};

    u16  timerPeriod{0};
    u16  timerCounter{0};
    u8   sequenceStep{0};

    u8   lengthCounter{0};

    void clockTimer();
    void clockLinearCounter();
    void clockLengthCounter();
    u8   output() const;
};

struct NoiseChannel {
    bool enabled{false};
    bool haltLoop{false};
    bool constVol{false};
    u8   volume{0};
    u8   envelopeDiv{0};
    u8   envelopeCounter{0};
    bool envelopeStart{false};

    bool mode{false};  // short mode
    u16  timerPeriod{0};
    u16  timerCounter{0};
    u16  shiftReg{1};

    u8   lengthCounter{0};

    void clockTimer();
    void clockLengthCounter();
    void clockEnvelope();
    u8   output() const;
};

struct DMCChannel {
    bool enabled{false};
    bool irqEnabled{false};
    bool loop{false};
    u16  rate{0};
    u16  rateCounter{0};
    u8   outputLevel{0};
    u16  sampleAddr{0xC000};
    u16  currentAddr{0xC000};
    u16  sampleLength{0};
    u16  bytesRemaining{0};
    u8   shiftReg{0};
    u8   bitsRemaining{0};
    bool silenceFlag{true};
    bool sampleBufferEmpty{true};
    u8   sampleBuffer{0};

    std::function<u8(u16)> memRead;
    std::function<void()>  irqCallback;

    void clockTimer();
    u8   output() const { return outputLevel; }
    void restart();
};

// ============================================================
class NESConsole;

class APU {
    friend class NESConsole;
public:
    // Sample rate
    static constexpr int kSampleRate = 44100;
    static constexpr int kChannels   = 1;

    void reset();
    void powerOn();

    // CPU register writes ($4000-$4017)
    void regWrite(u16 addr, u8 val);
    u8   regRead(u16 addr);

    // Clock 1 CPU cycle
    void tick();

    // Get accumulated samples (called each frame)
    void getSamples(std::vector<float>& out);

    // DMC memory read callback
    void setMemReadCallback(std::function<u8(u16)> cb);
    void setIRQCallback(std::function<void()> cb);

private:
    PulseChannel    pulse_[2];
    TriangleChannel triangle_;
    NoiseChannel    noise_;
    DMCChannel      dmc_;

    u64  cpuCycles_{0};
    bool frameIRQ_{false};
    u8   frameCounter_{0};
    bool fiveStep_{false};
    bool inhibitIRQ_{false};

    // Sample accumulation
    std::vector<float> sampleBuf_;
    double sampleAcc_{0.0};
    double samplePeriod_{CPU_FREQ_NTSC / kSampleRate};

    std::function<void()> irqCallback_;

    static const u8 kLengthTable[32];
    static const u16 kNoisePeriodTable[16];
    static const u16 kDMCRateTable[16];

    void clockFrameCounter();
    void clockHalfFrame();
    void clockQuarterFrame();
    float mixSample();
};
