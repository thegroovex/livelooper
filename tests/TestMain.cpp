// livelooper unit test runner.
// Runs every juce::UnitTest registered in the test sources and returns the number of
// failed tests as exit code (0 = all passed), so it can be used directly by CTest / CI.

#include <juce_gui_basics/juce_gui_basics.h>

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI gui;

    juce::UnitTestRunner runner;
    runner.setAssertOnFailure (false);
    runner.setPassesAreLogged (false);

    if (argc > 1)
        runner.runTestsInCategory (argv[1]);
    else
        runner.runAllTests();

    int failures = 0, passes = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
    {
        const auto* r = runner.getResult (i);
        failures += r->failures;
        passes += r->passes;
    }

    std::cout << "\n==============================\n"
              << "livelooper tests: " << runner.getNumResults() << " groups, "
              << passes << " checks passed, " << failures << " failed\n"
              << "==============================" << std::endl;
    return failures == 0 ? 0 : 1;
}
