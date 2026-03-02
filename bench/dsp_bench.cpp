// Benchmarks the Warm waveshaper DSP inner loop.
// Simulates stereo processBlock at several drive levels and block sizes,
// reports CPU% relative to real-time audio budget.

#include <cmath>
#include <chrono>
#include <cstdio>
#include <array>

// Minimal SmoothedValue stand-in (matches JUCE behaviour: linear ramp)
struct SmoothedValue
{
    float current = 0.5f;
    float target  = 0.5f;
    float step    = 0.0f;
    int   remain  = 0;

    void reset (double sampleRate, double rampSeconds)
    {
        int rampSamples = static_cast<int> (sampleRate * rampSeconds);
        step   = (target - current) / static_cast<float> (rampSamples);
        remain = rampSamples;
    }

    void setTargetValue (float t) { target = t; }

    float getNextValue()
    {
        if (remain > 0) { current += step; --remain; }
        else              current = target;
        return current;
    }
};

static void runBench (const char* label,
                      float       driveTarget,
                      int         blockSize,
                      int         numBlocks,
                      double      sampleRate)
{
    const int numChannels = 2;

    // Fill input with a sine wave at -12 dBFS
    const float amplitude = 0.25f;
    std::array<float, 4096> buf{};
    for (int i = 0; i < blockSize; ++i)
        buf[static_cast<size_t>(i)] = amplitude * std::sin (2.0f * 3.14159265f * 440.0f
                                                             * static_cast<float>(i) / static_cast<float>(sampleRate));

    std::array<SmoothedValue, 2> smoothed{};
    for (auto& s : smoothed)
    {
        s.current = driveTarget;
        s.target  = driveTarget;
    }

    const float normAmount  = std::log (driveTarget / 0.5f) / std::log (40.0f);
    const float makeupGain  = 1.0f - normAmount * 0.109f;

    // --- timed section ---
    auto t0 = std::chrono::high_resolution_clock::now();

    for (int block = 0; block < numBlocks; ++block)
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            smoothed[static_cast<size_t>(ch)].setTargetValue (driveTarget);
            for (int i = 0; i < blockSize; ++i)
            {
                float drive     = smoothed[static_cast<size_t>(ch)].getNextValue();
                float tanhDrive = std::tanh (drive);
                float shaped    = std::tanh (drive * buf[static_cast<size_t>(i)]) / tanhDrive;
                buf[static_cast<size_t>(i)] = shaped * makeupGain;
            }
        }
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    // --- end timed section ---

    double elapsedMs  = std::chrono::duration<double, std::milli>(t1 - t0).count();
    double audioMs    = (static_cast<double>(blockSize) * numBlocks / sampleRate) * 1000.0;
    double cpuPercent = (elapsedMs / audioMs) * 100.0;

    std::printf ("  %-28s  drive=%5.1f  block=%4d  elapsed=%6.2f ms  audio=%6.0f ms  CPU=%.2f%%\n",
                 label, driveTarget, blockSize,
                 elapsedMs, audioMs, cpuPercent);
}

int main()
{
    constexpr double kSR      = 44100.0;
    constexpr int    kBlocks  = 10000;   // 10k blocks ≈ realistic session workload

    std::puts ("Warm DSP benchmark — stereo, 44100 Hz, 10 000 blocks\n");

    //            label               drive   blockSize  numBlocks  sampleRate
    runBench ("0%  (drive=0.5)",      0.5f,    512, kBlocks, kSR);
    runBench ("50% (drive=1.8)",      1.8f,    512, kBlocks, kSR);
    runBench ("75% (drive=5.5)",      5.5f,    512, kBlocks, kSR);
    runBench ("100% (drive=20.0)",   20.0f,    512, kBlocks, kSR);
    std::puts ("");
    runBench ("100% small block=64", 20.0f,     64, kBlocks, kSR);
    runBench ("100% large block=4096",20.0f,  4096, kBlocks, kSR);

    return 0;
}
