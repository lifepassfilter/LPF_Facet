#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "FacetEngine.h"

#ifndef FACET_IS_SYNTH
 #define FACET_IS_SYNTH 0
#endif

class FacetProcessor : public juce::AudioProcessor,
                       private juce::Timer
{
public:
    FacetProcessor();
    ~FacetProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- used by the editor ----------------------------------------------
    bool loadAudioFile (const juce::File&);
    void startMidiLearn (const juce::String& paramID);
    void clearMidiLearn (const juce::String& paramID);
    juce::String midiLearnInfo (const juce::String& paramID) const;
    bool isLearning (const juce::String& paramID) const;

    static constexpr bool isInstrument = FACET_IS_SYNTH != 0;

    juce::AudioProcessorValueTreeState apvts;
    facet::Engine engine;
    juce::String loadedFileName;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void readParams (facet::Params&) const;
    int  paramIndex (const juce::String& id) const;
    void timerCallback() override;

    juce::AudioFormatManager formatManager;
    facet::Params params;

    // MIDI learn: CC -> parameter index. Audio thread queues values,
    // the message thread applies them so the host sees normal automation.
    std::array<std::atomic<int>, 128> ccMap;
    std::atomic<int> learnIndex { -1 };
    juce::AbstractFifo ccFifo { 512 };
    std::array<std::pair<int, float>, 512> ccQueue;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FacetProcessor)
};
