#include "dsp/VocalEngine.h"
#include "model/PresetManager.h"
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <new>
#include <random>
#include <stdexcept>
#if defined(_MSC_VER)
#include <malloc.h>
#endif

static std::atomic<bool> countAllocations {false};
static std::atomic<int> allocations {0};
void* operator new(std::size_t size)
{
    if (countAllocations.load(std::memory_order_relaxed)) allocations.fetch_add(1, std::memory_order_relaxed);
    if (auto* p = std::malloc(size == 0 ? 1 : size)) return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t s) { return ::operator new(s); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
void* operator new(std::size_t size, std::align_val_t align)
{
    if (countAllocations.load(std::memory_order_relaxed)) allocations.fetch_add(1, std::memory_order_relaxed);
    const auto alignment = static_cast<std::size_t>(align);
#if defined(_MSC_VER)
    void* p = _aligned_malloc(size == 0 ? 1 : size, alignment);
#else
    const auto padded = ((std::max<std::size_t>(1, size) + alignment - 1) / alignment) * alignment;
    void* p = std::aligned_alloc(alignment, padded);
#endif
    if (p) return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size, std::align_val_t align) { return ::operator new(size, align); }
void operator delete(void* p, std::align_val_t) noexcept
{
#if defined(_MSC_VER)
    _aligned_free(p);
#else
    std::free(p);
#endif
}
void operator delete[](void* p, std::align_val_t align) noexcept { ::operator delete(p, align); }
void operator delete(void* p, std::size_t, std::align_val_t align) noexcept { ::operator delete(p, align); }
void operator delete[](void* p, std::size_t, std::align_val_t align) noexcept { ::operator delete(p, align); }
namespace
{
int checks = 0;
void require(bool condition, const char* text)
{
    ++checks;
    if (!condition) throw std::runtime_error(text);
}
cris::AudioData tone(double hz, double rate, double duration = 1.2, bool harmonics = false, bool vibrato = false)
{
    cris::AudioData a; a.sampleRate = rate;
    a.channels.resize(1); a.channels[0].resize(static_cast<std::size_t>(rate * duration));
    double phase = 0.0;
    for (std::size_t i = 0; i < a.size(); ++i)
    {
        const double t = i / rate;
        const double f = hz * std::exp2((vibrato ? 0.25 * std::sin(2.0 * cris::pi * 5.0 * t) : 0.0) / 12.0);
        phase += 2.0 * cris::pi * f / rate;
        double sample = 0.22 * std::sin(phase);
        if (harmonics) for (int h = 2; h <= 10; ++h) sample += 0.22 / h * std::sin(phase * h);
        const double fade = std::min({1.0, t / 0.02, (duration - t) / 0.02});
        a.channels[0][i] = static_cast<float>(sample * std::max(0.0, fade));
    }
    return a;
}
double pitch(const cris::AudioData& a)
{
    const auto analysis = cris::analyse(a);
    std::vector<double> values;
    for (const auto& f : analysis.frames) if (f.voiced && f.time > 0.25 && f.time < a.duration() - 0.2) values.push_back(f.hz);
    require(!values.empty(), "Pitch not detected in output");
    std::sort(values.begin(), values.end());
    return values[values.size() / 2];
}
void writeWav(const cris::AudioData& a, const char* name)
{
    std::ofstream f(name, std::ios::binary);
    auto w16 = [&](unsigned v) { f.put(static_cast<char>(v & 255)); f.put(static_cast<char>((v >> 8) & 255)); };
    auto w32 = [&](unsigned v) { w16(v & 65535); w16(v >> 16); };
    const unsigned bytes = static_cast<unsigned>(a.size() * 2);
    f.write("RIFF", 4); w32(36 + bytes); f.write("WAVEfmt ", 8); w32(16); w16(1); w16(1);
    w32(static_cast<unsigned>(a.sampleRate)); w32(static_cast<unsigned>(a.sampleRate) * 2); w16(2); w16(16);
    f.write("data", 4); w32(bytes);
    for (float x : a.channels[0]) w16(static_cast<unsigned>(static_cast<int>(std::clamp(x, -1.0f, 1.0f) * 32767.0f)) & 65535);
}
}
int main()
{
    try
    {
        for (double rate : {44100.0, 48000.0, 96000.0})
            for (double hz : {60.0, 82.4069, 110.0, 196.0, 440.0})
            {
                auto a = tone(hz, rate, 0.65, true);
                const double measured = pitch(a);
                require(std::abs(cris::centsBetween(measured, hz)) < 8.0, "Low male F0 detection error");
            }
        auto silence = tone(110.0, 48000.0);
        std::fill(silence.channels[0].begin(), silence.channels[0].end(), 0.0f);
        const auto silentAnalysis = cris::analyse(silence);
        for (const auto& f : silentAnalysis.frames) require(!f.voiced, "Silence marked voiced");
        std::mt19937 random(7); std::uniform_real_distribution<float> noise(-0.03f, 0.03f);
        auto breath = silence;
        for (auto& s : breath.channels[0]) s = noise(random);
        int voiced = 0;
        const auto noisyAnalysis = cris::analyse(breath);
        for (const auto& f : noisyAnalysis.frames) voiced += f.voiced ? 1 : 0;
        require(voiced < static_cast<int>(noisyAnalysis.frames.size() / 10), "Noise rejected poorly");
        require(cris::PitchCorrection::allowed(69, 9, 2), "A minor excludes A");
        require(!cris::PitchCorrection::allowed(70, 9, 2), "A minor includes A sharp");
        require(cris::PitchCorrection::nearest(69.25, 9, 2) == 69.0, "Scale target wrong");
        require(std::string(cris::PresetManager::all()[0].name) == "NATURAL MALE VOCAL", "Wrong default preset");
        for (double rate : {44100.0, 48000.0, 96000.0})
        {
            const double inputHz = 110.0 * std::exp2(0.35 / 12.0);
            auto a = tone(inputHz, rate, 1.5, true);
            auto analysis = cris::analyse(a);
            auto p = cris::PresetManager::all()[5].parameters;
            auto b = cris::render(a, analysis, p);
            if (rate == 48000.0) { writeWav(a, "synthetic_original.wav"); writeWav(b, "synthetic_corrected.wav"); }
            require(a.size() == b.size(), "Offline render changes duration");
            const double measured = pitch(b);
            const auto dbgCurve = cris::correctionCurve(analysis, p);
            std::cout << "Corrected " << rate << " Hz: " << inputHz << " (detected " << pitch(a) << ", shift " << dbgCurve[dbgCurve.size()/2] << ") -> " << measured << " Hz\n";
            require(std::abs(cris::centsBetween(measured, 110.0)) < 12.0, "Rendered audio is not tuned");
            for (float s : b.channels[0]) require(std::isfinite(s) && std::abs(s) < 4.0f, "Non-finite/explosive render");
            p.correction = 0.0f;
            require(cris::render(a, analysis, p).channels == a.channels, "Correction zero not transparent");
            if (rate == 48000.0) { writeWav(a, "synthetic_original.wav"); writeWav(b, "synthetic_corrected.wav"); }
        }
        auto vibrato = cris::analyse(tone(110.0, 48000.0, 1.5, true, true));
        auto natural = cris::PresetManager::all()[0].parameters;
        natural.flexCents = 0.0f;
        const auto curve = cris::correctionCurve(vibrato, natural);
        double low = 999.0, high = -999.0;
        for (std::size_t i = 0; i < curve.size(); ++i)
            if (vibrato.frames[i].time > 0.5 && vibrato.frames[i].time < 1.25)
            { const double m = vibrato.frames[i].midi + curve[i]; low = std::min(low, m); high = std::max(high, m); }
        require(high - low > 0.20, "Natural preset flattens vibrato");
        for (double targetHz : {61.7354, 82.4069, 195.9977})
            for (double detune : {-0.35, 0.35})
            {
                auto lowVoice = tone(targetHz * std::exp2(detune / 12.0), 48000.0, 1.5, true);
                auto analysis = cris::analyse(lowVoice);
                auto p = cris::PresetManager::all()[5].parameters;
                auto corrected = cris::render(lowVoice, analysis, p);
                const double error = std::abs(cris::centsBetween(pitch(corrected), targetHz));
                std::cout << "Register check " << targetHz << " Hz, " << detune * 100 << " ct: residual " << error << " ct\n";
                require(error < 15.0, "Register correction error exceeds 15 cents");
            }
        {
            auto source = tone(112.0, 48000.0, 1.5, true);
            auto a = cris::analyse(source);
            require(!a.notes.empty(), "No note for manual transposition test");
            for (auto& note : a.notes) note.manualMidi = 46.0;
            auto rendered = cris::render(source, a, cris::PresetManager::all()[5].parameters);
            require(std::abs(cris::centsBetween(pitch(rendered), cris::hzFromMidi(46.0))) < 15.0, "Manual target not applied to audio");
        }
        for (double rate : {44100.0, 48000.0, 96000.0})
        {
            cris::LiveEngine live; live.prepare(2, rate);
            auto source = tone(112.0, rate, 1.0, true);
            std::array<std::array<float, 128>, 2> buffer {};
            float* channels[] {buffer[0].data(), buffer[1].data()};
            auto parameters = cris::PresetManager::all()[5].parameters;
            allocations.store(0); countAllocations.store(true);
            live.process(channels, 2, 128, parameters, 1.0f, 1.0f, false);
            countAllocations.store(false);
            require(allocations.load() == 0, "Heap allocation on first prepared LIVE block");
            allocations.store(0); countAllocations.store(true); live.reset(); countAllocations.store(false);
            require(allocations.load() == 0, "Heap allocation while clearing LIVE state");
            // Exercise all phase/formant paths before measuring sustained throughput.
            for (int block = 0; block < 100; ++block)
            {
                for (int i = 0; i < 128; ++i) buffer[0][i] = buffer[1][i] = source.channels[0][static_cast<std::size_t>(block * 128 + i) % source.size()];
                live.process(channels, 2, 128, parameters, 0.5f, 1.0f, false);
            }
            allocations.store(0); countAllocations.store(true);
            const auto begin = std::chrono::steady_clock::now();
            for (int block = 0; block < 300; ++block)
            {
                for (int i = 0; i < 128; ++i) buffer[0][i] = buffer[1][i] = source.channels[0][static_cast<std::size_t>(block * 128 + i) % source.size()];
                live.process(channels, 2, 128, parameters, block % 2 == 0 ? 0.5f : 1.0f, 1.0f, false);
            }
            const auto end = std::chrono::steady_clock::now();
            countAllocations.store(false);
            const double elapsed = std::chrono::duration<double>(end - begin).count();
            std::cout << "LIVE stereo " << rate << " Hz: " << elapsed * 1000.0 << " ms for " << 300.0 * 128 / rate << " s, allocations=" << allocations.load() << "\n";
            require(allocations.load() == 0, "Heap allocation during warmed LIVE processing");
            require(elapsed < 300.0 * 128 / rate, "LIVE slower than real time on test machine");
        }
        {
            cris::LiveEngine dry;
            dry.prepare(1, 48000.0);
            cris::Parameters p;
            p.correction = 0.0f;
            std::array<float, 64> block {};
            float* channels[] {block.data()};
            int impulseAt = -1;
            for (int offset = 0; offset < dry.latency() + 256; offset += 64)
            {
                block.fill(0.0f);
                if (offset == 0) block[0] = 0.5f;
                dry.process(channels, 1, 64, p, 0.0f, 1.0f, true);
                for (int i = 0; i < 64; ++i) if (block[static_cast<std::size_t>(i)] > 0.49f) impulseAt = offset + i;
            }
            require(impulseAt == dry.latency(), "Dry bypass not aligned to reported latency");
        }
        bool stop = true;
        require(cris::analyse(tone(110, 48000), [&] { return stop; }).frames.empty(), "Analysis cancellation failed");
        std::cout << "PASS: " << checks << " checks (synthetic signals; not a real-vocal acceptance test).\n";
        return 0;
    }
    catch (const std::exception& e) { countAllocations.store(false); std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
