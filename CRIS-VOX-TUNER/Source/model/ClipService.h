#pragma once
#include "dsp/VocalEngine.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <atomic>
#include <memory>
#include <mutex>
#include <optional>

namespace cris
{
struct ClipSnapshot
{
    std::shared_ptr<const AudioData> original;
    AudioData corrected;
    Analysis analysis;
    std::vector<double> curve;
    juce::String path;
    std::uint64_t revision = 0;
};

// One writer. Audio reads a hazard-protected raw pointer: no shared_ptr
// refcount, deallocation, mutex, disk access or DSP render on the audio thread.
class ClipService final : private juce::Thread
{
public:
    explicit ClipService(std::function<Parameters()> parameters);
    ~ClipService() override;
    void importFile(const juce::File& file, std::vector<Note> restored = {});
    void clear();
    void requestRender();
    void moveNote(int id, double midi);
    void resetEdits();
    void exportFile(const juce::File& file, float mix, float gain, bool bypass);
    std::shared_ptr<const ClipSnapshot> snapshot() const;
    const ClipSnapshot* acquireAudio();
    void releaseAudio();
    juce::String status() const;
    juce::String sourcePath() const;
    std::vector<Note> edits() const;
    bool busy() const { return working.load(); }
private:
    void run() override;
    void publish(std::shared_ptr<const ClipSnapshot> result);
    void setStatus(juce::String text);
    struct ExportJob
    {
        juce::File file;
        std::shared_ptr<const ClipSnapshot> clip;
        float mix = 1.0f, gain = 1.0f;
        bool bypass = false;
    };
    void runExport(const ExportJob& job);
    std::function<Parameters()> getParameters;
    mutable std::mutex mutex;
    std::shared_ptr<const AudioData> source;
    Analysis analysis;
    juce::String path, pendingImport, statusText {"LIVE ready - import a WAV for note editing"};
    std::vector<Note> pendingEdits;
    std::optional<ExportJob> pendingExport;
    std::shared_ptr<const ClipSnapshot> active;
    std::vector<std::shared_ptr<const ClipSnapshot>> retired;
    std::atomic<const ClipSnapshot*> published {nullptr}, hazard {nullptr};
    std::atomic<std::uint64_t> generation {0};
    std::atomic<bool> working {false};
};
}
