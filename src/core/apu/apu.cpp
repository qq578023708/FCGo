#include "apu.h"
#include <cstring>
#include <cmath>

// ============================================================
// Lookup Tables
// ============================================================
const u8 APU::kLengthTable[32] = {
    10,254, 20,  2, 40,  4, 80,  6, 160,  8, 60, 10, 14, 12, 26, 14,
    12, 16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30
};
const u16 APU::kNoisePeriodTable[16] = {
    4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068
};
const u16 APU::kDMCRateTable[16] = {
    428,380,340,320,286,254,226,214,190,160,142,128,106,84,72,54
};

// Duty cycle sequences
static const u8 kDutyTable[4][8] = {
    {0,1,0,0,0,0,0,0},
    {0,1,1,0,0,0,0,0},
    {0,1,1,1,1,0,0,0},
    {1,0,0,1,1,1,1,1}
};

static const u8 kTriangleSeq[32] = {
    15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0,
    0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
};

// ============================================================
// PulseChannel
// ============================================================
void PulseChannel::clockTimer() {
    if (timerCounter == 0) {
        timerCounter = timerPeriod;
        sequenceStep = (sequenceStep + 1) & 7;
    } else timerCounter--;
}

void PulseChannel::clockLengthCounter() {
    if (!haltLoop && lengthCounter > 0) lengthCounter--;
}

void PulseChannel::clockEnvelope() {
    if (envelopeStart) {
        envelopeStart = false;
        envelopeCounter = 15;
        envelopeDiv = volume;
    } else {
        if (envelopeDiv == 0) {
            envelopeDiv = volume;
            if (envelopeCounter == 0) { if(haltLoop) envelopeCounter=15; }
            else envelopeCounter--;
        } else envelopeDiv--;
    }
}

void PulseChannel::clockSweep() {
    if (sweepReload) {
        sweepCounter = sweepPeriod;
        sweepReload = false;
    } else if (sweepCounter > 0) sweepCounter--;
    else {
        sweepCounter = sweepPeriod;
        if (sweepEnabled && sweepShift > 0) {
            int change = timerPeriod >> sweepShift;
            if (sweepNegate) change = onesComplement ? -(change+1) : -change;
            int newPeriod = (int)timerPeriod + change;
            if (newPeriod >= 8 && newPeriod < 0x800) timerPeriod = (u16)newPeriod;
        }
    }
}

u8 PulseChannel::output() const {
    if (!enabled || lengthCounter == 0) return 0;
    if (timerPeriod < 8 || timerPeriod > 0x7FF) return 0;
    if (!kDutyTable[duty][sequenceStep]) return 0;
    return constVol ? volume : envelopeCounter;
}

// ============================================================
// TriangleChannel
// ============================================================
void TriangleChannel::clockTimer() {
    if (timerCounter == 0) {
        timerCounter = timerPeriod;
        if (linearCounter > 0 && lengthCounter > 0)
            sequenceStep = (sequenceStep + 1) & 31;
    } else timerCounter--;
}

void TriangleChannel::clockLinearCounter() {
    if (linearReload) linearCounter = linearPeriod;
    else if (linearCounter > 0) linearCounter--;
    if (!control) linearReload = false;
}

void TriangleChannel::clockLengthCounter() {
    if (!control && lengthCounter > 0) lengthCounter--;
}

u8 TriangleChannel::output() const {
    if (!enabled || lengthCounter == 0 || linearCounter == 0) return 0;
    return kTriangleSeq[sequenceStep];
}

// ============================================================
// NoiseChannel
// ============================================================
void NoiseChannel::clockTimer() {
    if (timerCounter == 0) {
        timerCounter = timerPeriod;
        u16 feedback = (shiftReg & 1) ^ ((mode ? (shiftReg>>6) : (shiftReg>>1)) & 1);
        shiftReg = (shiftReg >> 1) | (feedback << 14);
    } else timerCounter--;
}

void NoiseChannel::clockLengthCounter() {
    if (!haltLoop && lengthCounter > 0) lengthCounter--;
}

void NoiseChannel::clockEnvelope() {
    if (envelopeStart) {
        envelopeStart = false;
        envelopeCounter = 15;
        envelopeDiv = volume;
    } else {
        if (envelopeDiv == 0) {
            envelopeDiv = volume;
            if (envelopeCounter == 0) { if(haltLoop) envelopeCounter=15; }
            else envelopeCounter--;
        } else envelopeDiv--;
    }
}

u8 NoiseChannel::output() const {
    if (!enabled || lengthCounter == 0) return 0;
    if (shiftReg & 1) return 0;
    return constVol ? volume : envelopeCounter;
}

// ============================================================
// DMCChannel
// ============================================================
void DMCChannel::clockTimer() {
    if (rateCounter == 0) {
        rateCounter = rate;
        // Output unit
        if (!silenceFlag) {
            int delta = (shiftReg & 1) ? 2 : -2;
            int newLevel = (int)outputLevel + delta;
            if (newLevel >= 0 && newLevel <= 127) outputLevel = (u8)newLevel;
        }
        shiftReg >>= 1;
        if (--bitsRemaining == 0) {
            bitsRemaining = 8;
            if (sampleBufferEmpty) silenceFlag = true;
            else {
                silenceFlag = false;
                shiftReg = sampleBuffer;
                sampleBufferEmpty = true;
            }
        }
        // Fill sample buffer
        if (sampleBufferEmpty && bytesRemaining > 0) {
            if (memRead) sampleBuffer = memRead(currentAddr);
            sampleBufferEmpty = false;
            currentAddr = (currentAddr == 0xFFFF) ? 0x8000 : currentAddr+1;
            bytesRemaining--;
            if (bytesRemaining == 0) {
                if (loop) restart();
                else if (irqEnabled && irqCallback) irqCallback();
            }
        }
    } else rateCounter--;
}

void DMCChannel::restart() {
    currentAddr = sampleAddr;
    bytesRemaining = sampleLength;
}

// ============================================================
// APU
// ============================================================
void APU::powerOn() {
    memset(&pulse_,   0, sizeof(pulse_));
    memset(&triangle_,0, sizeof(triangle_));
    memset(&noise_,   0, sizeof(noise_));
    memset(&dmc_,     0, sizeof(dmc_));
    noise_.shiftReg = 1;
    pulse_[0].onesComplement = true;
    reset();
}

void APU::reset() {
    cpuCycles_ = 0;
    frameCounter_ = 0;
    fiveStep_ = false;
    inhibitIRQ_ = false;
    sampleAcc_ = 0;
    sampleBuf_.clear();
    sampleBuf_.reserve(4096);
    // Disable all channels
    pulse_[0].enabled = pulse_[1].enabled = false;
    triangle_.enabled = noise_.enabled = dmc_.enabled = false;
}

void APU::setMemReadCallback(std::function<u8(u16)> cb)  { dmc_.memRead = std::move(cb); }
void APU::setIRQCallback(std::function<void()> cb)        { irqCallback_ = std::move(cb); dmc_.irqCallback = cb; }

void APU::regWrite(u16 addr, u8 val) {
    switch (addr) {
    case 0x4000: case 0x4004: {
        int ch = (addr>>2)&1;
        pulse_[ch].duty      = (val>>6)&3;
        pulse_[ch].haltLoop  = (val>>5)&1;
        pulse_[ch].constVol  = (val>>4)&1;
        pulse_[ch].volume    = val&0x0F;
    } break;
    case 0x4001: case 0x4005: {
        int ch = (addr>>2)&1;
        pulse_[ch].sweepEnabled = (val>>7)&1;
        pulse_[ch].sweepPeriod  = ((val>>4)&7)+1;
        pulse_[ch].sweepNegate  = (val>>3)&1;
        pulse_[ch].sweepShift   = val&7;
        pulse_[ch].sweepReload  = true;
    } break;
    case 0x4002: case 0x4006: {
        int ch = (addr>>2)&1;
        pulse_[ch].timerPeriod = (pulse_[ch].timerPeriod&0xFF00)|(val);
    } break;
    case 0x4003: case 0x4007: {
        int ch = (addr>>2)&1;
        pulse_[ch].timerPeriod = (pulse_[ch].timerPeriod&0x00FF)|((val&7)<<8);
        if (pulse_[ch].enabled) pulse_[ch].lengthCounter = kLengthTable[val>>3];
        pulse_[ch].sequenceStep = 0;
        pulse_[ch].envelopeStart = true;
    } break;
    case 0x4008:
        triangle_.control     = (val>>7)&1;
        triangle_.linearPeriod= val&0x7F;
        break;
    case 0x400A:
        triangle_.timerPeriod = (triangle_.timerPeriod&0xFF00)|val;
        break;
    case 0x400B:
        triangle_.timerPeriod = (triangle_.timerPeriod&0x00FF)|((val&7)<<8);
        if (triangle_.enabled) triangle_.lengthCounter = kLengthTable[val>>3];
        triangle_.linearReload = true;
        break;
    case 0x400C:
        noise_.haltLoop = (val>>5)&1;
        noise_.constVol = (val>>4)&1;
        noise_.volume   = val&0x0F;
        break;
    case 0x400E:
        noise_.mode        = (val>>7)&1;
        noise_.timerPeriod = kNoisePeriodTable[val&0x0F];
        break;
    case 0x400F:
        if (noise_.enabled) noise_.lengthCounter = kLengthTable[val>>3];
        noise_.envelopeStart = true;
        break;
    case 0x4010:
        dmc_.irqEnabled = (val>>7)&1;
        dmc_.loop       = (val>>6)&1;
        dmc_.rate       = kDMCRateTable[val&0x0F];
        break;
    case 0x4011:
        dmc_.outputLevel = val&0x7F;
        break;
    case 0x4012:
        dmc_.sampleAddr  = 0xC000|(val<<6);
        break;
    case 0x4013:
        dmc_.sampleLength= (val<<4)|1;
        break;
    case 0x4015: {
        pulse_[0].enabled = val&1;
        pulse_[1].enabled = (val>>1)&1;
        triangle_.enabled = (val>>2)&1;
        noise_.enabled    = (val>>3)&1;
        dmc_.enabled      = (val>>4)&1;
        if (!pulse_[0].enabled) pulse_[0].lengthCounter = 0;
        if (!pulse_[1].enabled) pulse_[1].lengthCounter = 0;
        if (!triangle_.enabled) triangle_.lengthCounter = 0;
        if (!noise_.enabled)    noise_.lengthCounter    = 0;
        if (!dmc_.enabled) { dmc_.bytesRemaining = 0; }
        else if (dmc_.bytesRemaining == 0) dmc_.restart();
    } break;
    case 0x4017:
        fiveStep_  = (val>>7)&1;
        inhibitIRQ_= (val>>6)&1;
        if (inhibitIRQ_) frameIRQ_ = false;
        frameCounter_ = 0;
        if (fiveStep_) clockHalfFrame(), clockQuarterFrame();
        break;
    }
}

u8 APU::regRead(u16 addr) {
    if (addr == 0x4015) {
        u8 s = 0;
        if (pulse_[0].lengthCounter>0) s|=1;
        if (pulse_[1].lengthCounter>0) s|=2;
        if (triangle_.lengthCounter>0) s|=4;
        if (noise_.lengthCounter>0)    s|=8;
        if (dmc_.bytesRemaining>0)     s|=16;
        if (frameIRQ_)  s|=64;  frameIRQ_=false;
        return s;
    }
    return 0;
}

void APU::clockQuarterFrame() {
    pulse_[0].clockEnvelope();
    pulse_[1].clockEnvelope();
    noise_.clockEnvelope();
    triangle_.clockLinearCounter();
}

void APU::clockHalfFrame() {
    pulse_[0].clockLengthCounter();
    pulse_[0].clockSweep();
    pulse_[1].clockLengthCounter();
    pulse_[1].clockSweep();
    triangle_.clockLengthCounter();
    noise_.clockLengthCounter();
}

void APU::clockFrameCounter() {
    // Frame counter cycles every 14915 (4-step) or 18641 (5-step) CPU cycles
    u32 cycleInFrame = (u32)(cpuCycles_ % (fiveStep_ ? 18641u : 14915u));

    if (!fiveStep_) {
        // 4-step mode: quarter frame at 3729, 7457, 11186; half frame at 7457, 14915
        if (cycleInFrame == 3729 || cycleInFrame == 11186) {
            clockQuarterFrame();
        }
        if (cycleInFrame == 7457) {
            clockQuarterFrame();
            clockHalfFrame();
        }
        if (cycleInFrame == 14915) {
            clockQuarterFrame();
            clockHalfFrame();
            if (!inhibitIRQ_) { frameIRQ_ = true; if (irqCallback_) irqCallback_(); }
        }
    } else {
        // 5-step mode: quarter frame at 3729, 7457, 11186, 14915; half frame at 7457, 18641
        if (cycleInFrame == 3729 || cycleInFrame == 11186 || cycleInFrame == 14915) {
            clockQuarterFrame();
        }
        if (cycleInFrame == 7457 || cycleInFrame == 18641) {
            clockQuarterFrame();
            clockHalfFrame();
        }
    }
}

float APU::mixSample() {
    // Nonlinear mixing per NES dev wiki
    float p1 = pulse_[0].output();
    float p2 = pulse_[1].output();
    float tri = triangle_.output();
    float nse = noise_.output();
    float dmc = dmc_.output();

    float pulse_out = (p1+p2 > 0) ? 95.88f / (8128.f/(p1+p2) + 100.f) : 0;
    float tnd_out   = (tri+nse+dmc > 0) ? 159.79f / (1.f/(tri/8227.f+nse/12241.f+dmc/22638.f) + 100.f) : 0;
    return pulse_out + tnd_out;
}

void APU::tick() {
    cpuCycles_++;
    clockFrameCounter();

    // Clock timers (pulse/triangle/noise at CPU/2 rate = every other cycle)
    if (cpuCycles_ & 1) {
        pulse_[0].clockTimer();
        pulse_[1].clockTimer();
        triangle_.clockTimer();
        noise_.clockTimer();
    }
    // DMC clocks at CPU rate (every cycle)
    dmc_.clockTimer();

    // Accumulate sample
    sampleAcc_ += 1.0;
    if (sampleAcc_ >= samplePeriod_) {
        sampleAcc_ -= samplePeriod_;
        sampleBuf_.push_back(mixSample());
    }
}

void APU::getSamples(std::vector<float>& out) {
    out = std::move(sampleBuf_);
    sampleBuf_.clear();
    sampleBuf_.reserve(4096);
}
