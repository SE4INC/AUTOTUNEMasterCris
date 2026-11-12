#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <fstream>
#include <cstring>

namespace
{
constexpr std::array<const char*, 16> ids {"correction", "speed", "humanize", "flex", "formant", "drift",
    "key", "scale", "mix", "output", "mode", "sync", "start", "bypass", "reserved1", "reserved2"};
constexpr std::array<const char*, 8> renderIds {"correction", "speed", "humanize", "flex", "formant", "drift", "key", "scale"};
}
juce::AudioProcessorValueTreeState::ParameterLayout CrisVoxTunerProcessor::layout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    auto add = [&](const char* id, const char* name, float low, float high, float step, float def)
    { l.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id, 1}, name,
              juce::NormalisableRange<float>(low, high, step), def)); };
    add("correction", "Correction", 0, 100, 0.1f, 80);
    add("speed", "Speed (ms)", 1, 200, 0.1f, 35);
    add("humanize", "Humanize", 0, 100, 0.1f, 60);
    add("flex", "Flex Tune (cents)", 0, 50, 0.1f, 12);
    add("formant", "Formant preservation", 0, 100, 0.1f, 100);
    add("drift", "Pitch Drift", 0, 100, 0.1f, 65);
    l.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"key", 1}, "Reference Key",
        juce::StringArray{"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"}, 0));
    l.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"scale", 1}, "Scale",
        juce::StringArray{"Chromatic", "Major", "Natural Minor", "Minor Pentatonic"}, 0));
    add("mix", "Mix", 0, 100, 0.1f, 100);
    add("output", "Output (dB)", -24, 12, 0.1f, 0);
    l.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"mode", 1}, "Mode", juce::StringArray{"LIVE INPUT", "CLIP EDITOR"}, 0));
    l.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"sync", 1}, "Host Sync", false));
    add("start", "Clip Start (seconds)", 0, 3600, 0.01f, 0);
    l.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"bypass", 1}, "Bypass", false));
    return l;
}
CrisVoxTunerProcessor::CrisVoxTunerProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      state(*this, nullptr, "CRIS_VOX_TUNER", layout()), clips([this] { return parameters(); })
{
    for (std::size_t i = 0; i < 14; ++i) raw[i] = state.getRawParameterValue(ids[i]);
    for (auto id : renderIds) state.addParameterListener(id, this);
}
CrisVoxTunerProcessor::~CrisVoxTunerProcessor()
{ for (auto id : renderIds) state.removeParameterListener(id, this); }
float CrisVoxTunerProcessor::value(const char* id) const
{
    // Cached atomics only: no string lookups on the audio callback.
    for (std::size_t i = 0; i < 14; ++i) if (ids[i] == id || std::strcmp(ids[i], id) == 0) return raw[i]->load();
    return 0.0f;
}
cris::Parameters CrisVoxTunerProcessor::parameters() const
{
    return {value("correction") / 100.0f, value("speed"), value("humanize") / 100.0f,
        value("flex"), value("formant") / 100.0f, value("drift") / 100.0f,
        static_cast<int>(value("key")), static_cast<int>(value("scale"))};
}
void CrisVoxTunerProcessor::parameterChanged(const juce::String&, float) { clips.requestRender(); }
void CrisVoxTunerProcessor::setParameter(const juce::String& id, float v)
{
    if (auto* p = state.getParameter(id)) { p->beginChangeGesture(); p->setValueNotifyingHost(p->convertTo0to1(v)); p->endChangeGesture(); }
}
void CrisVoxTunerProcessor::prepareToPlay(double sampleRate, int)
{
    live.prepare(getTotalNumOutputChannels(), sampleRate);
    setLatencySamples(live.latency());
    clipCursor = 0.0; clipRate = sampleRate; wasPreview = false; seenRevision = 0; activeMode = -1;
    clipMix = value("mix") / 100.0f; clipGain = juce::Decibels::decibelsToGain(value("output"));
}
bool CrisVoxTunerProcessor::isBusesLayoutSupported(const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo()) && out == l.getMainInputChannelSet();
}
void CrisVoxTunerProcessor::processBlock(juce::AudioBuffer<float>& b, juce::MidiBuffer& midi)
{ juce::ScopedNoDenormals noDenormals; midi.clear(); process(b, false); }
void CrisVoxTunerProcessor::processBlockBypassed(juce::AudioBuffer<float>& b, juce::MidiBuffer& midi)
{ juce::ScopedNoDenormals noDenormals; midi.clear(); process(b, true); }
void CrisVoxTunerProcessor::process(juce::AudioBuffer<float>& b, bool hostBypass)
{
    if (b.getNumSamples() == 0 || b.getNumChannels() == 0) return;
    const float in = b.getMagnitude(0, b.getNumSamples());
    inputLevel.store(in);
    const bool bypass = hostBypass || value("bypass") > 0.5f;
    const float gain = juce::Decibels::decibelsToGain(value("output"));
    const float mix = original.load() ? 0.0f : value("mix") / 100.0f;
    const int mode = value("mode") < 0.5f ? 0 : 1;
    if (activeMode != mode)
    {
        live.reset();
        clipCursor = 0.0; wasPreview = false; previewSeconds.store(0.0);
        activeMode = mode;
    }
    if (mode == 0)
    {
        live.process(b.getArrayOfWritePointers(), b.getNumChannels(), b.getNumSamples(), parameters(), mix, gain, bypass);
        detectedHz.store(static_cast<float>(live.lastFrame().hz));
        correctionCents.store(static_cast<float>(live.lastShift() * 100.0));
    }
    else
    {
        // CLIP is an explicit source mode: mute the incoming track to prevent doubling.
        b.clear();
        const auto* clip = clips.acquireAudio();
        bool playing = preview.load();
        bool syncing = value("sync") > 0.5f;
        double position = clipCursor;
        if (syncing)
        {
            playing = false;
            if (auto* playhead = getPlayHead())
                if (auto info = playhead->getPosition())
                {
                    playing = info->getIsPlaying();
                    if (auto seconds = info->getTimeInSeconds())
                        position = (*seconds - value("start")) * clipRate - getLatencySamples();
                    else playing = false;
                }
        }
        else if (playing && !wasPreview) position = 0.0;
        if (clip && seenRevision != clip->revision)
        {
            // Retain playback position on parameter renders; rewind on a new source.
            seenRevision = clip->revision;
        }
        if (clip && clip->original && playing)
        {
            const double ratio = clip->original->sampleRate / clipRate;
            const float smoothing = static_cast<float>(1.0 - std::exp(-1.0 / (clipRate * 0.01)));
            const auto length = clip->original->size();
            for (int i = 0; i < b.getNumSamples(); ++i)
            {
                clipMix += smoothing * ((bypass ? 0.0f : mix) - clipMix);
                clipGain += smoothing * (gain - clipGain);
                const double sourceIndex = (position + i) * ratio;
                if (sourceIndex < 0.0 || sourceIndex >= static_cast<double>(length - 1)) continue;
                const auto n = static_cast<std::size_t>(sourceIndex);
                const float fraction = static_cast<float>(sourceIndex - n);
                for (int c = 0; c < b.getNumChannels(); ++c)
                {
                    const auto channel = std::min(static_cast<std::size_t>(c), clip->original->channels.size() - 1);
                    const auto& dry = clip->original->channels[channel];
                    const auto& wet = clip->corrected.channels[channel];
                    const float d = dry[n] + fraction * (dry[n + 1] - dry[n]);
                    const float w = wet[n] + fraction * (wet[n + 1] - wet[n]);
                    b.setSample(c, i, (d + clipMix * (w - d)) * clipGain);
                }
            }
            clipCursor = position + b.getNumSamples();
            previewSeconds.store(std::max(0.0, position / clipRate));
            if (!syncing && clipCursor * ratio >= length) { preview.store(false); clipCursor = 0.0; }
        }
        wasPreview = preview.load();
        clips.releaseAudio();
    }
    outputLevel.store(b.getMagnitude(0, b.getNumSamples()));
}
void CrisVoxTunerProcessor::setCurrentProgram(int index)
{
    const auto& presets = cris::PresetManager::all();
    index = juce::jlimit(0, static_cast<int>(presets.size()) - 1, index);
    currentProgram.store(index);
    const auto& p = presets[static_cast<std::size_t>(index)].parameters;
    const std::array<float, 6> values {p.correction * 100, p.speedMs, p.humanize * 100, p.flexCents, p.formant * 100, p.drift * 100};
    // Program changes may be delivered by the host on its audio thread: no UI,
    // file access, mutex or gesture messages here.
    for (std::size_t i = 0; i < values.size(); ++i)
        if (auto* parameter = state.getParameter(ids[i])) parameter->setValueNotifyingHost(parameter->convertTo0to1(values[i]));
}
const juce::String CrisVoxTunerProcessor::getProgramName(int i)
{ return cris::PresetManager::all()[static_cast<std::size_t>(juce::jlimit(0, getNumPrograms() - 1, i))].name; }
void CrisVoxTunerProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    auto tree = state.copyState();
    tree.setProperty("preset", currentProgram.load(), nullptr);
    tree.setProperty("sourcePath", clips.sourcePath(), nullptr);
    juce::ValueTree notes("Edits");
    for (const auto& note : clips.edits()) if (note.manualMidi >= 0.0)
    {
        juce::ValueTree n("Note");
        n.setProperty("id", note.id, nullptr); n.setProperty("start", note.start, nullptr);
        n.setProperty("midi", note.manualMidi, nullptr); notes.appendChild(n, nullptr);
    }
    tree.appendChild(notes, nullptr);
    if (auto xml = tree.createXml()) copyXmlToBinary(*xml, dest);
}
void CrisVoxTunerProcessor::setStateInformation(const void* data, int size)
{
    if (auto xml = getXmlFromBinary(data, size))
    {
        auto tree = juce::ValueTree::fromXml(*xml);
        if (!tree.hasType("CRIS_VOX_TUNER")) return;
        std::vector<cris::Note> restored;
        for (const auto n : tree.getChildWithName("Edits"))
        {
            cris::Note note;
            note.id = static_cast<int>(n.getProperty("id"));
            note.start = static_cast<double>(n.getProperty("start"));
            note.manualMidi = static_cast<double>(n.getProperty("midi"));
            if (std::isfinite(note.start) && std::isfinite(note.manualMidi)) restored.push_back(note);
        }
        const auto source = tree.getProperty("sourcePath").toString();
        currentProgram.store(juce::jlimit(0, getNumPrograms() - 1, static_cast<int>(tree.getProperty("preset", 0))));
        tree.removeChild(tree.getChildWithName("Edits"), nullptr);
        state.replaceState(tree);
        preview.store(false);
        if (source.isNotEmpty()) clips.importFile(juce::File(source), std::move(restored)); else clips.clear();
    }
}
void CrisVoxTunerProcessor::exportClip(const juce::File& file)
{
    clips.exportFile(file, original.load() ? 0.0f : value("mix") / 100.0f,
        juce::Decibels::decibelsToGain(value("output")), value("bypass") > 0.5f);
}

juce::AudioProcessorEditor* CrisVoxTunerProcessor::createEditor() { return new CrisVoxTunerEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new CrisVoxTunerProcessor(); }
