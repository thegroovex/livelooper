// Tests of the English / German user interface texts.

#include "plugin/Localisation.h"
#include "plugin/Parameters.h"
#include "plugin/PluginEditor.h"
#include "plugin/PluginProcessor.h"
#include "plugin/Widgets.h"
#include "engine/Effects.h"
#include "engine/BeatFx.h"

using namespace livelooper;

namespace
{
struct ScopedLanguage
{
    explicit ScopedLanguage (i18n::Language l) : previous (i18n::getSetting()) { i18n::setSetting (l, false); }
    ~ScopedLanguage() { i18n::setSetting (previous, false); }
    i18n::Language previous;
};

struct CountingListener : i18n::Listener
{
    int calls = 0;
    void languageChanged() override { ++calls; }
};
}

class LocalisationTests : public juce::UnitTest
{
public:
    LocalisationTests() : juce::UnitTest ("Localisation", "livelooper") {}

    void runTest() override
    {
        beginTest ("English is passed through, German is translated");
        {
            ScopedLanguage en (i18n::Language::English);
            expect (! i18n::isGerman());
            expectEquals (i18n::tr ("CLEAR ALL"), juce::String ("CLEAR ALL"));
            expectEquals (i18n::tr ("TRACK %1", "3"), juce::String ("TRACK 3"));

            ScopedLanguage de (i18n::Language::German);
            expect (i18n::isGerman());
            expectEquals (i18n::tr ("CLEAR ALL"), juce::String::fromUTF8 ("ALLE L\xc3\x96SCHEN"));
            expectEquals (i18n::tr ("TRACK %1", "3"), juce::String ("SPUR 3"));
            expectEquals (i18n::tr ("BAR %1 . %2", "4", "2"), juce::String ("TAKT 4 . 2"));
            expectEquals (i18n::tr ("unknown text stays"), juce::String ("unknown text stays"));
            expect (i18n::getNumTranslations() > 150);
        }

        beginTest ("Every choice, effect and effect parameter has a German text");
        {
            juce::StringArray all;
            for (auto list : { params::quantizeChoices(), params::inputChoices(), params::startModeChoices(),
                               params::stopModeChoices(), params::dubModeChoices(), params::fxTypeChoices(),
                               params::clickSoundChoices(),
                               // v1.5
                               params::beatFxChoices(), params::colorFxChoices(), params::fxTargetChoices(),
                               params::grooveChoices(), params::fillChoices(), params::kitChoices() })
                all.addArray (list);
            for (int i = 0; i < (int) FxType::NumTypes; ++i)
            {
                all.add (getFxParam1Name ((FxType) i));
                all.add (getFxParam2Name ((FxType) i));
            }
            for (int i = 0; i < (int) BeatFxType::NumTypes; ++i)
                all.add (getBeatFxDepthName ((BeatFxType) i));
            for (const auto& s : all)
                expect (i18n::germanFor (s).isNotEmpty(), "missing German text for: " + s);
        }

        beginTest ("Parameter names: English names stay stable, German names are localised");
        {
            ScopedLanguage de (i18n::Language::German);
            expectEquals (params::englishName ("t1_level"), juce::String ("T1 Level"));
            expectEquals (params::englishName ("inB_mix"), juce::String ("In FX B Mix"));
            expectEquals (params::englishName (params::gMaster), juce::String ("Master Level"));
            expectEquals (params::localisedName ("t1_level"), juce::String ("S1 Pegel"));
            expectEquals (params::localisedName ("trC_type"), juce::String ("Sp-FX C Typ"));
            expectEquals (params::localisedName (params::gQuantize), juce::String ("Quantisierung"));

            // every parameter gets a real name (not its id) in both languages
            LiveLooperProcessor proc;
            for (auto* p : proc.getParameters())
                if (auto* wp = dynamic_cast<juce::AudioProcessorParameterWithID*> (p))
                {
                    expect (params::englishName (wp->paramID) != wp->paramID, "no name for " + wp->paramID);
                    expectEquals (wp->getName (64), params::localisedName (wp->paramID));
                }
            auto* q = dynamic_cast<juce::AudioParameterChoice*> (proc.getState().getParameter (params::gQuantize));
            expectEquals (q->choices[2], juce::String ("Takt"));
        }

        beginTest ("Listeners hear language changes, the editor switches live");
        {
            ScopedLanguage en (i18n::Language::English);
            CountingListener l;
            i18n::addListener (&l);
            i18n::setSetting (i18n::Language::German, false);
            i18n::setSetting (i18n::Language::English, false);
            i18n::removeListener (&l);
            i18n::setSetting (i18n::Language::German, false);
            expectEquals (l.calls, 2);

            i18n::setSetting (i18n::Language::English, false);
            LiveLooperProcessor proc;
            {
                std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
                LLCombo combo;
                combo.setEnglishItems (params::quantizeChoices());
                combo.setSelectedId (3, juce::dontSendNotification);
                i18n::setSetting (i18n::Language::German, false);
                expectEquals (combo.getSelectedId(), 3);
                expectEquals (combo.getText(), juce::String ("Takt"));
                i18n::setSetting (i18n::Language::English, false);
                expect (ed != nullptr);
            }
        }
    }
};

static LocalisationTests localisationTests;
