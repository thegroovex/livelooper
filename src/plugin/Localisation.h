#pragma once

#include <juce_core/juce_core.h>

namespace livelooper::i18n
{
/** User interface language. Automatic follows the language of the operating system. */
enum class Language
{
    Automatic = 0,
    English,
    German
};

/** The language chosen by the user (stored for all livelooper instances on this computer). */
Language getSetting();

/** Changes the language. When persist is true the choice is written to the user settings file
    so every livelooper instance (and the next session) uses it. Notifies all listeners. */
void setSetting (Language language, bool persist = true);

/** The language actually in use (Automatic resolved; the environment variable
    LIVELOOPER_LANGUAGE=en|de overrides everything, which tests and CI use). */
Language getEffective();

bool isGerman();

/** Translates an English UI text into the current language. Unknown texts come back unchanged.
    Texts may contain %1, %2 placeholders, see tr (text, arg1, ...). */
juce::String tr (const juce::String& english);
juce::String tr (const juce::String& english, const juce::String& arg1);
juce::String tr (const juce::String& english, const juce::String& arg1, const juce::String& arg2);

/** Upper case that also handles the German umlauts on every platform (juce::String::toUpperCase
    relies on towupper, which leaves ä/ö/ü alone in the "C" locale). */
juce::String upper (const juce::String& text);

/** Translation of a single text into German (empty if there is none) - for tests. */
juce::String germanFor (const juce::String& english);

/** Number of entries in the translation table - for tests. */
int getNumTranslations();

/** Notified (synchronously, on the thread that called setSetting - the message thread)
    whenever the language changes. */
struct Listener
{
    virtual ~Listener() = default;
    virtual void languageChanged() = 0;
};
void addListener (Listener*);
void removeListener (Listener*);
} // namespace livelooper::i18n
