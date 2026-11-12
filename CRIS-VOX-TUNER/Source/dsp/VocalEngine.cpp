#include "VocalEngine.h"
namespace cris
{
Analysis analyse(const AudioData& in, const Cancel& cancel)
{
    Analysis result;
    if (in.size() == 0) return result;
    PitchDetector detector;
    PitchTracker tracker;
    detector.prepare(in.sampleRate);
    // Select strongest channel, avoiding cancellation from stereo phase differences.
    std::size_t channel = 0;
    double maximum = -1.0;
    for (std::size_t c = 0; c < in.channels.size(); ++c)
    {
        double energy = 0.0;
        for (float sample : in.channels[c]) energy += sample * static_cast<double>(sample);
        if (energy > maximum) { maximum = energy; channel = c; }
    }
    const auto pad = static_cast<std::size_t>(std::ceil(detector.frameDelaySeconds() * in.sampleRate));
    for (std::size_t i = 0; i < in.size() + pad; ++i)
    {
        if ((i & 4095u) == 0 && cancelled(cancel)) return {};
        PitchFrame f;
        if (detector.push(i < in.size() ? in.channels[channel][i] : 0.0f, f))
        {
            if (result.frames.empty() && f.time > 0.0)
            {
                auto first = tracker.update(f);
                first.time = 0.0;
                result.frames.push_back(first);
            }
            const double dt = result.frames.empty() ? 0.01 : std::max(0.001, f.time - result.frames.back().time);
            if (f.time <= in.duration()) result.frames.push_back(tracker.update(f, dt));
        }
    }
    if (result.frames.size() > 2) result.hopSeconds = result.frames[2].time - result.frames[1].time;
    result.notes = PitchTracker::segment(result.frames, result.hopSeconds);
    return result;
}
std::vector<double> correctionCurve(const Analysis& a, const Parameters& p)
{
    std::vector<double> curve;
    curve.reserve(a.frames.size());
    PitchCorrection correction;
    std::size_t note = 0;
    for (std::size_t i = 0; i < a.frames.size(); ++i)
    {
        const auto& f = a.frames[i];
        while (note < a.notes.size() && a.notes[note].end < f.time) ++note;
        double manual = -1.0;
        if (note < a.notes.size() && f.time >= a.notes[note].start && f.time <= a.notes[note].end) manual = a.notes[note].manualMidi;
        const double dt = i == 0 ? a.hopSeconds : std::max(0.001, f.time - a.frames[i - 1].time);
        curve.push_back(correction.update(f, p, dt, manual));
    }
    return curve;
}
AudioData render(const AudioData& in, const Analysis& a, const Parameters& p, const Cancel& cancel)
{
    if (in.size() == 0) return in;
    if (p.correction <= 0.0f) return in;
    const auto curve = correctionCurve(a, p);
    if (curve.empty()) return in;
    AudioData result;
    result.sampleRate = in.sampleRate;
    result.channels.assign(in.channels.size(), std::vector<float>(in.size(), 0.0f));
    FormantProcessor shifter;
    shifter.prepare(static_cast<int>(in.channels.size()), in.sampleRate, true);
    const int latency = shifter.latency();
    std::array<std::array<float, 128>, 2> input {}, output {};
    std::array<const float*, 2> ip {input[0].data(), input[1].data()};
    std::array<float*, 2> op {output[0].data(), output[1].data()};
    std::size_t frameIndex = 0;
    float voice = 0.0f;
    auto frameAt = [&](double seconds)
    {
        const auto it = std::lower_bound(a.frames.begin(), a.frames.end(), seconds,
            [](const PitchFrame& f, double time) { return f.time < time; });
        return static_cast<std::size_t>(std::min<std::ptrdiff_t>(static_cast<std::ptrdiff_t>(a.frames.size() - 1), it - a.frames.begin()));
    };
    for (std::size_t pos = 0; pos < in.size() + static_cast<std::size_t>(latency); pos += 128)
    {
        if (cancelled(cancel)) return {};
        const double controlTime = (static_cast<double>(pos) - shifter.inputLatency()) / in.sampleRate;
        while (frameIndex + 1 < a.frames.size() && a.frames[frameIndex + 1].time <= controlTime) ++frameIndex;
        shifter.setPitch(curve[frameIndex], p.formant, a.frames[frameIndex].hz);
        for (std::size_t c = 0; c < in.channels.size(); ++c)
            for (std::size_t j = 0; j < 128; ++j) input[c][j] = pos + j < in.size() ? in.channels[c][pos + j] : 0.0f;
        shifter.process(ip.data(), op.data(), 128);
        for (std::size_t j = 0; j < 128; ++j)
        {
            const auto dest = static_cast<std::int64_t>(pos + j) - latency;
            if (dest < 0 || dest >= static_cast<std::int64_t>(in.size())) continue;
            const auto fi = frameAt(dest / in.sampleRate);
            const float wanted = a.frames[fi].voiced ? 1.0f : 0.0f;
            voice += static_cast<float>(1.0 - std::exp(-1.0 / (in.sampleRate * 0.008))) * (wanted - voice);
            for (std::size_t c = 0; c < in.channels.size(); ++c)
            {
                const auto n = static_cast<std::size_t>(dest);
                result.channels[c][n] = in.channels[c][n] + voice * (output[c][j] - in.channels[c][n]);
            }
        }
    }
    return result;
}
void LiveEngine::prepare(int channels, double sampleRate)
{
    rate = sampleRate;
    detector.prepare(rate);
    tracker.reset(); corrector.reset(); frame = {}; shift = 0.0;
    shifter.prepare(channels, rate, false);
    delaySize = shifter.latency() + 1;
    dryDelay.assign(static_cast<std::size_t>(channels), std::vector<float>(static_cast<std::size_t>(delaySize), 0.0f));
    voiceDelay.assign(static_cast<std::size_t>(delaySize), 0.0f);
    write = 0; voicedMix = 0.0f; wetMix = outputGain = 1.0f;
}
void LiveEngine::reset()
{
    detector.reset(); tracker.reset(); corrector.reset(); shifter.reset();
    frame = {}; shift = 0.0; write = 0; voicedMix = 0.0f;
    for (auto& channel : dryDelay) std::fill(channel.begin(), channel.end(), 0.0f);
    std::fill(voiceDelay.begin(), voiceDelay.end(), 0.0f);
}
void LiveEngine::process(float* const* data, int channels, int samples, const Parameters& p, float mix, float gain, bool bypass)
{
    std::array<const float*, 2> ip {input[0].data(), input[1].data()};
    std::array<float*, 2> op {output[0].data(), output[1].data()};
    const float smoothing = static_cast<float>(1.0 - std::exp(-1.0 / (rate * 0.01)));
    for (int start = 0; start < samples; start += 128)
    {
        const int count = std::min(128, samples - start);
        for (int j = 0; j < count; ++j)
        {
            PitchFrame next;
            if (detector.push(data[0][start + j], next))
            {
                frame = tracker.update(next);
                shift = corrector.update(frame, p, 0.01);
            }
            const int read = (write + 1) % delaySize;
            // Detector reports the centre of an earlier window; delay this mask
            // only by the remaining signal latency, not the full latency twice.
            const int detectorDelay = static_cast<int>(detector.frameDelaySeconds() * rate);
            const int maskDelay = std::max(0, shifter.latency() - detectorDelay);
            const int maskRead = (write - maskDelay + delaySize) % delaySize;
            voiceDelay[static_cast<std::size_t>(write)] = frame.voiced ? 1.0f : 0.0f;
            voicedMix += smoothing * (voiceDelay[static_cast<std::size_t>(maskRead)] - voicedMix);
            mask[static_cast<std::size_t>(j)] = voicedMix;
            for (int c = 0; c < channels; ++c)
            {
                input[static_cast<std::size_t>(c)][static_cast<std::size_t>(j)] = std::isfinite(data[c][start + j]) ? data[c][start + j] : 0.0f;
                dry[static_cast<std::size_t>(c)][static_cast<std::size_t>(j)] = dryDelay[static_cast<std::size_t>(c)][static_cast<std::size_t>(read)];
                dryDelay[static_cast<std::size_t>(c)][static_cast<std::size_t>(write)] = input[static_cast<std::size_t>(c)][static_cast<std::size_t>(j)];
            }
            write = read;
        }
        shifter.setPitch(shift, p.formant, frame.hz);
        shifter.process(ip.data(), op.data(), count);
        for (int j = 0; j < count; ++j)
        {
            wetMix += smoothing * ((bypass ? 0.0f : mix) - wetMix);
            outputGain += smoothing * (gain - outputGain);
            for (int c = 0; c < channels; ++c)
            {
                const float d = dry[static_cast<std::size_t>(c)][static_cast<std::size_t>(j)];
                const float amount = p.correction <= 0.0f ? 0.0f : wetMix * mask[static_cast<std::size_t>(j)];
                data[c][start + j] = (d + amount * (output[static_cast<std::size_t>(c)][static_cast<std::size_t>(j)] - d)) * outputGain;
            }
        }
    }
}
}
