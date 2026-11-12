#include "PluginProcessor.h"
#include <juce_graphics/juce_graphics.h>
#include <chrono>
#include <iostream>
#include <stdexcept>

namespace
{
void require(bool v, const char* message) { if (!v) throw std::runtime_error(message); }
template <class F> bool waitFor(F predicate)
{
    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    while (std::chrono::steady_clock::now() < end)
    {
        if (predicate()) return true;
        juce::Thread::sleep(25);
    }
    return false;
}
double noteError(const cris::Analysis& a)
{
    std::vector<double> errors;
    for (const auto& f : a.frames) if (f.voiced && f.sustained) errors.push_back(std::abs(f.midi - std::round(f.midi)) * 100.0);
    if (errors.empty()) return -1.0;
    std::sort(errors.begin(), errors.end());
    return errors[errors.size() / 2];
}
}
int main(int argc, char** argv)
{
    try
    {
        juce::ScopedJuceInitialiser_GUI initialise;
        CrisVoxTunerProcessor processor;
        processor.prepareToPlay(48000.0, 256);
        require(processor.getLatencySamples() > 0, "Latency not reported");
        require(processor.getNumPrograms() == 9, "Presets missing");
        processor.setCurrentProgram(6);
        require(processor.state.getRawParameterValue("speed")->load() == 7.0f, "Preset does not update processing");
        processor.setCurrentProgram(5);
        juce::AudioBuffer<float> buffer(2, 256);
        juce::MidiBuffer midi;
        buffer.clear(); processor.processBlock(buffer, midi);
        const juce::File vocal(argc > 1 ? juce::String(argv[1]) : juce::String(CRIS_TEST_VOCAL));
        require(vocal.existsAsFile(), "Missing real singing fixture");
        processor.clips.importFile(vocal);
        require(waitFor([&] { return processor.clips.snapshot() != nullptr; }), "Clip analysis/render failed or timed out");
        auto first = processor.clips.snapshot();
        require(!first->analysis.notes.empty(), "No notes found in singing fixture");
        require(first->original->size() == first->corrected.size(), "Singing duration changed");
        double difference = 0.0;
        for (std::size_t c = 0; c < first->corrected.channels.size(); ++c)
            for (std::size_t i = 0; i < first->corrected.size(); ++i)
            {
                const double d = first->corrected.channels[c][i] - first->original->channels[c][i];
                difference += d * d;
                require(std::isfinite(first->corrected.channels[c][i]), "Non-finite singing output");
            }
        require(difference > 0.001, "Rendered singing identical to input");
        const auto measured = cris::analyse(first->corrected);
        require(noteError(measured) >= 0.0 && noteError(measured) < noteError(first->analysis), "Singing note error did not improve");
        std::cout << "Real singing fixture: " << vocal.getFileName() << ", " << first->original->duration()
            << " s, " << first->analysis.notes.size() << " notes; sustained-note median chromatic error "
            << noteError(first->analysis) << " -> " << noteError(measured) << " cents\n";
        const auto revision = first->revision;
        const auto editedNote = first->analysis.notes.front();
        processor.clips.moveNote(editedNote.id, std::round(editedNote.originalMidi) + 1.0);
        require(waitFor([&] { auto s = processor.clips.snapshot(); return s && s->revision != revision; }), "Manual edit did not render");
        auto edited = processor.clips.snapshot();
        require(edited->analysis.notes.front().manualMidi >= 0.0, "Manual edit lost");
        juce::MemoryBlock state;
        processor.setParameter("key", 9.0f); processor.setParameter("scale", 2.0f);
        processor.getStateInformation(state);
        {
            CrisVoxTunerProcessor restored;
            restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
            require(restored.state.getRawParameterValue("key")->load() == 9.0f, "Key not restored");
            require(restored.state.getRawParameterValue("scale")->load() == 2.0f, "Scale not restored");
            require(waitFor([&] { return restored.clips.snapshot() != nullptr; }), "Clip not restored");
            require(restored.clips.snapshot()->analysis.notes.front().manualMidi >= 0.0, "Manual target not restored");
        }
        // Render the actual JUCE editor to PNG, rather than the image concept.
        processor.setParameter("mode", 1.0f);
        {
            std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
            editor->setSize(1180, 820);
            auto snapshot = editor->createComponentSnapshot(editor->getLocalBounds(), true, 1.0f);
            const auto imageFile = juce::File::getCurrentWorkingDirectory().getChildFile("actual-interface.png");
            imageFile.deleteFile();
            auto stream = imageFile.createOutputStream();
            juce::PNGImageFormat png;
            require(stream && png.writeImageToStream(snapshot, *stream), "UI snapshot failed");
            editor->setSize(960, 740);
            auto small = editor->createComponentSnapshot(editor->getLocalBounds(), true, 1.0f);
            const auto smallFile = juce::File::getCurrentWorkingDirectory().getChildFile("actual-interface-small.png");
            smallFile.deleteFile();
            auto smallStream = smallFile.createOutputStream();
            require(smallStream && png.writeImageToStream(small, *smallStream), "Small UI snapshot failed");
        }
        const auto exported = juce::File::getCurrentWorkingDirectory().getChildFile("real-vocal-corrected-test.wav");
        exported.deleteFile();
        processor.exportClip(exported);
        require(waitFor([&] { return exported.existsAsFile() && !processor.clips.busy(); }), "WAV export failed");
        juce::AudioFormatManager formats; formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(exported));
        require(reader && reader->lengthInSamples == static_cast<juce::int64>(first->original->size()), "Export length wrong");
        processor.setParameter("mode", 1.0f);
        processor.preview.store(true);
        double energy = 0.0;
        for (int block = 0; block < 100; ++block)
        {
            buffer.clear(); processor.processBlock(buffer, midi);
            energy += buffer.getRMSLevel(0, 0, buffer.getNumSamples());
        }
        require(energy > 0.01, "CLIP playback silent");
        std::cout << "PASS: JUCE integration, real singing render, presets, manual edit, state, export, playback and UI snapshots.\n";
        std::cout << "Listening assessment and FL Studio/Windows acceptance remain required.\n";
        return 0;
    }
    catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
