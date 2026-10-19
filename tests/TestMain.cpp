// livelooper unit test runner.
// Runs every juce::UnitTest registered in the test sources and returns the number of
// failed tests as exit code (0 = all passed), so it can be used directly by CTest / CI.
//
//   livelooper_tests                 all test groups
//   livelooper_tests <category>      only the groups of one category (engine, plugin, midi, livelooper)
//   livelooper_tests --shard i/n     every n-th group (sorted by name), starting with the i-th (1-based);
//                                    CI runs the n shards as parallel jobs, together they cover every group
//   livelooper_tests --list          print the group names (sorted) and exit

#include <juce_gui_basics/juce_gui_basics.h>

#include <algorithm>
#include <iostream>

#include "plugin/Localisation.h"

namespace
{
    juce::Array<juce::UnitTest*> sortedTests()
    {
        juce::Array<juce::UnitTest*> tests (juce::UnitTest::getAllTests());
        std::sort (tests.begin(), tests.end(),
                   [] (const juce::UnitTest* a, const juce::UnitTest* b) { return a->getName() < b->getName(); });
        return tests;
    }

    bool parseShard (const juce::String& s, int& index, int& count)
    {
        if (! s.containsChar ('/'))
            return false;
        index = s.upToFirstOccurrenceOf ("/", false, false).getIntValue();
        count = s.fromFirstOccurrenceOf ("/", false, false).getIntValue();
        return count > 0 && index >= 1 && index <= count;
    }
}

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI gui;

    // tests expect the English texts unless they switch the language themselves (never saved)
    livelooper::i18n::setSetting (livelooper::i18n::Language::English, false);

    juce::String category, shardArg, nameFilter;
    bool list = false;
    for (int i = 1; i < argc; ++i)
    {
        const juce::String a (argv[i]);
        if (a == "--shard" && i + 1 < argc)  shardArg = argv[++i];
        else if (a.startsWith ("--shard="))  shardArg = a.fromFirstOccurrenceOf ("=", false, false);
        else if (a == "--list")              list = true;
        else if (a == "--name" && i + 1 < argc) nameFilter = argv[++i];   // groups whose name contains this text
        else                                 category = a;
    }

    if (list)
    {
        for (auto* t : sortedTests())
            std::cout << t->getName() << "  [" << t->getCategory() << "]\n";
        return 0;
    }

    juce::UnitTestRunner runner;
    runner.setAssertOnFailure (false);
    runner.setPassesAreLogged (false);

    int failures = 0, passes = 0, groups = 0;
    const auto addResults = [&]
    {
        for (int i = 0; i < runner.getNumResults(); ++i)
        {
            const auto* r = runner.getResult (i);
            failures += r->failures;
            passes += r->passes;
        }
        groups += runner.getNumResults();
    };

    if (nameFilter.isNotEmpty())
    {
        juce::Array<juce::UnitTest*> chosen;
        for (auto* t : sortedTests())
            if (t->getName().containsIgnoreCase (nameFilter))
                chosen.add (t);
        runner.runTests (chosen);
        addResults();
    }
    else if (shardArg.isNotEmpty())
    {
        int index = 0, count = 0;
        if (! parseShard (shardArg, index, count))
        {
            std::cerr << "invalid --shard value '" << shardArg << "' (expected i/n, 1 <= i <= n)\n";
            return 2;
        }

        const auto all = sortedTests();
        std::cout << "shard " << index << "/" << count << " of " << all.size() << " test groups\n";
        for (int i = index - 1; i < all.size(); i += count)
        {
            // one group at a time, so every group's duration is logged (shard balancing)
            const auto start = juce::Time::getMillisecondCounterHiRes();
            runner.runTests ({ all[i] });
            addResults();
            std::cout << "  [" << juce::String ((juce::Time::getMillisecondCounterHiRes() - start) / 1000.0, 2)
                      << " s] " << all[i]->getName() << std::endl;
        }
    }
    else
    {
        if (category.isNotEmpty())
            runner.runTestsInCategory (category);
        else
            runner.runAllTests();
        addResults();
    }

    std::cout << "\n==============================\n"
              << "livelooper tests: " << groups << " groups, "
              << passes << " checks passed, " << failures << " failed\n"
              << "==============================" << std::endl;
    return failures == 0 ? 0 : 1;
}
