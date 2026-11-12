#include "ClipService.h"
#include <limits>
#include <stdexcept>
namespace cris
{
ClipService::ClipService(std::function<Parameters()> p) : juce::Thread("CRIS clip worker"), getParameters(std::move(p))
{ startThread(juce::Thread::Priority::low); }
ClipService::~ClipService()
{
    signalThreadShouldExit(); notify(); stopThread(-1);
}
void ClipService::setStatus(juce::String text) { std::lock_guard<std::mutex> lock(mutex); statusText = std::move(text); }
juce::String ClipService::status() const { std::lock_guard<std::mutex> lock(mutex); return statusText; }
juce::String ClipService::sourcePath() const { std::lock_guard<std::mutex> lock(mutex); return path; }
std::vector<Note> ClipService::edits() const { std::lock_guard<std::mutex> lock(mutex); return analysis.notes; }
std::shared_ptr<const ClipSnapshot> ClipService::snapshot() const { std::lock_guard<std::mutex> lock(mutex); return active; }
void ClipService::importFile(const juce::File& file, std::vector<Note> restored)
{
    { std::lock_guard<std::mutex> lock(mutex); pendingImport = file.getFullPathName(); pendingEdits = std::move(restored); }
    requestRender();
}
void ClipService::requestRender() { generation.fetch_add(1); }
void ClipService::clear()
{
    { std::lock_guard<std::mutex> lock(mutex); pendingImport = "::CLEAR::"; pendingEdits.clear(); }
    requestRender();
}
void ClipService::moveNote(int id, double midi)
{
    { std::lock_guard<std::mutex> lock(mutex);
      for (auto& note : analysis.notes) if (note.id == id) note.manualMidi = midi < 0.0 ? -1.0 : std::clamp(midi, 24.0, 96.0); }
    requestRender();
}
void ClipService::resetEdits()
{
    { std::lock_guard<std::mutex> lock(mutex); for (auto& n : analysis.notes) n.manualMidi = -1.0; }
    requestRender();
}
void ClipService::exportFile(const juce::File& file, float mix, float gain, bool bypass)
{
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (!active || pendingExport) return;
        pendingExport = ExportJob {file, active, mix, gain, bypass};
    }
    working.store(true);
    notify();
}
void ClipService::runExport(const ExportJob& job)
{
    setStatus("Exporting 24-bit WAV...");
    juce::TemporaryFile temporary(job.file);
    std::unique_ptr<juce::OutputStream> stream(temporary.getFile().createOutputStream());
    if (!stream) throw std::runtime_error("Cannot create export file.");
    juce::WavAudioFormat format;
    const auto channels = job.clip->original->channels.size();
    const auto options = juce::AudioFormatWriterOptions()
        .withSampleRate(job.clip->original->sampleRate)
        .withNumChannels(static_cast<int>(channels)).withBitsPerSample(24);
    auto writer = format.createWriterFor(stream, options);
    if (!writer) throw std::runtime_error("Cannot initialise WAV writer.");
    juce::AudioBuffer<float> block(static_cast<int>(channels), 4096);
    for (std::size_t offset = 0; offset < job.clip->original->size(); offset += 4096)
    {
        if (threadShouldExit()) return;
        const int count = static_cast<int>(std::min<std::size_t>(4096, job.clip->original->size() - offset));
        for (std::size_t c = 0; c < channels; ++c)
            for (int i = 0; i < count; ++i)
            {
                const auto n = offset + static_cast<std::size_t>(i);
                const float d = job.clip->original->channels[c][n];
                block.setSample(static_cast<int>(c), i, (d + (job.bypass ? 0.0f : job.mix) * (job.clip->corrected.channels[c][n] - d)) * job.gain);
            }
        if (!writer->writeFromAudioSampleBuffer(block, 0, count)) throw std::runtime_error("Disk write failed.");
    }
    writer.reset();
    if (!temporary.overwriteTargetFileWithTemporary()) throw std::runtime_error("Cannot replace destination file.");
    setStatus("Exported: " + job.file.getFileName());
}
const ClipSnapshot* ClipService::acquireAudio()
{
    const ClipSnapshot* result;
    do { result = published.load(); hazard.store(result); } while (result != published.load());
    return result;
}
void ClipService::releaseAudio() { hazard.store(nullptr); }
void ClipService::publish(std::shared_ptr<const ClipSnapshot> result)
{
    std::lock_guard<std::mutex> lock(mutex);
    if (active) retired.push_back(active);
    active = std::move(result);
    published.store(active.get());
    const auto* inUse = hazard.load();
    retired.erase(std::remove_if(retired.begin(), retired.end(), [inUse](const auto& p) { return p.get() != inUse; }), retired.end());
}
void ClipService::run()
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::uint64_t completed = 0;
    while (!threadShouldExit())
    {
        wait(60);
        {
            std::lock_guard<std::mutex> lock(mutex);
            const auto* inUse = hazard.load();
            retired.erase(std::remove_if(retired.begin(), retired.end(), [inUse](const auto& p) { return p.get() != inUse; }), retired.end());
        }
        std::optional<ExportJob> exportJob;
        { std::lock_guard<std::mutex> lock(mutex); exportJob.swap(pendingExport); }
        if (exportJob)
        {
            working.store(true);
            try { runExport(*exportJob); }
            catch (const std::exception& e) { setStatus(juce::String("Export error: ") + e.what()); }
            working.store(false);
        }
        auto requested = generation.load();
        if (completed == requested) continue;
        // Settle rapid knob movements without rendering every intermediate value.
        wait(160);
        requested = generation.load();
        working.store(true);
        juce::String load;
        std::vector<Note> restored;
        { std::lock_guard<std::mutex> lock(mutex); load.swapWith(pendingImport); restored.swap(pendingEdits); }
        const Cancel cancel = [this, requested] { return threadShouldExit() || generation.load() != requested; };
        try
        {
            if (load == "::CLEAR::")
            {
                { std::lock_guard<std::mutex> lock(mutex); source.reset(); analysis = {}; path.clear(); }
                publish({}); setStatus("LIVE ready - no clip loaded"); completed = requested; working.store(false); continue;
            }
            if (load.isNotEmpty())
            {
                publish({});
                { std::lock_guard<std::mutex> lock(mutex); source.reset(); analysis = {}; path = load; }
                setStatus("Loading and analysing vocal...");
                std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(juce::File(load)));
                if (!reader || reader->lengthInSamples < 1024 || reader->sampleRate < 8000.0 || reader->sampleRate > 192000.0)
                    throw std::runtime_error("Cannot read this audio file (use WAV, mono/stereo).");
                if (reader->lengthInSamples / reader->sampleRate > 180.0 || reader->numChannels > 2)
                    throw std::runtime_error("Use a mono/stereo vocal clip of up to 180 seconds.");
                const int count = static_cast<int>(reader->lengthInSamples);
                const int channels = static_cast<int>(reader->numChannels);
                juce::AudioBuffer<float> buffer(channels, count);
                if (!reader->read(&buffer, 0, count, 0, true, true)) throw std::runtime_error("Audio read failed.");
                auto audio = std::make_shared<AudioData>();
                audio->sampleRate = reader->sampleRate;
                audio->channels.resize(static_cast<std::size_t>(channels));
                for (int c = 0; c < channels; ++c)
                {
                    auto& destination = audio->channels[static_cast<std::size_t>(c)];
                    destination.assign(buffer.getReadPointer(c), buffer.getReadPointer(c) + count);
                    for (auto& s : destination) s = std::isfinite(s) ? std::clamp(s, -8.0f, 8.0f) : 0.0f;
                }
                // Finish analysis even if controls move while importing. New controls
                // will render from this cache on the next worker iteration.
                auto detected = analyse(*audio, [this] { return threadShouldExit(); });
                if (threadShouldExit()) break;
                for (const auto& edit : restored)
                    for (auto& note : detected.notes)
                        if (note.id == edit.id && std::abs(note.start - edit.start) < 0.05) note.manualMidi = edit.manualMidi;
                { std::lock_guard<std::mutex> lock(mutex); source = audio; analysis = std::move(detected); path = load; }
            }
            std::shared_ptr<const AudioData> audio;
            Analysis detected;
            juce::String currentPath;
            { std::lock_guard<std::mutex> lock(mutex); audio = source; detected = analysis; currentPath = path; }
            if (!audio) { completed = requested; working.store(false); continue; }
            const auto parameters = getParameters();
            setStatus("Rendering correction in background...");
            auto result = std::make_shared<ClipSnapshot>();
            result->original = audio;
            result->analysis = std::move(detected);
            result->corrected = render(*audio, result->analysis, parameters, cancel);
            if (cancelled(cancel)) { working.store(false); continue; }
            result->curve = correctionCurve(result->analysis, parameters);
            result->path = currentPath; result->revision = requested;
            publish(result);
            setStatus("Clip ready - drag notes vertically; double-click to restore a note");
        }
        catch (const std::exception& e) { setStatus(juce::String("Error: ") + e.what()); }
        completed = requested;
        working.store(false);
    }
}
}
