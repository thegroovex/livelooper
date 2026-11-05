#include "DrumSampleLoader.h"
#include "BinaryData.h"

#include <juce_audio_formats/juce_audio_formats.h>

namespace livelooper
{
namespace
{
    struct Entry { int kit, voice; float layer; const char* file; };
    constexpr Entry kEntries[] = {
       #include "engine/DrumSamples.inc"
    };

    const char* findResource (const char* fileName, int& size)
    {
        for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
            if (juce::String (BinaryData::getNamedResourceOriginalFilename (BinaryData::namedResourceList[i])) == fileName)
                return BinaryData::getNamedResource (BinaryData::namedResourceList[i], size);
        size = 0;
        return nullptr;
    }

    std::shared_ptr<const DrumBank> load()
    {
        const int numKits = RhythmGenerator::getNumKits();
        std::vector<std::shared_ptr<DrumBank::Kit>> kits;
        for (int k = 0; k < numKits; ++k)
        {
            auto kit = std::make_shared<DrumBank::Kit>();
            kit->name = RhythmGenerator::getKitName (k);
            for (int v = 0; v < DrumBank::kNumVoices; ++v)
            {
                kit->trim[(size_t) v] = RhythmGenerator::builtInTrim (k, v);
                kit->voiceNames[(size_t) v] = RhythmGenerator::getKitVoiceName (k, v);
            }
            kit->room = RhythmGenerator::builtInRoom (k);
            kit->fillStyle = RhythmGenerator::kitFillStyle (k);
            kits.push_back (kit);
        }
        juce::FlacAudioFormat flac;
        for (const auto& e : kEntries)
        {
            int size = 0;
            const char* data = findResource (e.file, size);
            if (data == nullptr || e.kit < 0 || e.kit >= numKits || e.voice < 0 || e.voice >= DrumBank::kNumVoices)
                continue;
            std::unique_ptr<juce::AudioFormatReader> reader (
                flac.createReaderFor (new juce::MemoryInputStream (data, (size_t) size, false), true));
            if (reader == nullptr || reader->lengthInSamples <= 0)
                continue;
            juce::AudioBuffer<float> buffer ((int) reader->numChannels, (int) reader->lengthInSamples);
            reader->read (&buffer, 0, buffer.getNumSamples(), 0, true, true);
            DrumBank::Sample s;
            s.sampleRate = reader->sampleRate;
            s.layer = e.layer;
            s.data.assign (buffer.getReadPointer (0), buffer.getReadPointer (0) + buffer.getNumSamples());
            s.data.push_back (0.0f);    // guard sample for the interpolation
            kits[(size_t) e.kit]->voices[(size_t) e.voice].push_back (std::move (s));
        }
        auto bank = std::make_shared<DrumBank>();
        for (auto& k : kits)
            bank->kits.push_back (k);
        return bank;
    }
}

std::shared_ptr<const DrumBank> getEmbeddedDrumBank()
{
    static std::shared_ptr<const DrumBank> bank = load();
    return bank;
}
} // namespace livelooper
