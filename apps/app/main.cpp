#include "game.hpp"
#include "walkthrough.hpp"

#include <raylib.h>

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
    void printDiagnostics(const hollow::Diagnostics& diag)
    {
        for (const auto& e : diag.errors)
            std::cerr << "error: " << e << '\n';
        for (const auto& w : diag.warnings)
            std::cerr << "warning: " << w << '\n';
    }

    void printUsage()
    {
        std::cout << "hollow_app [options]\n"
                     "  --check              read and check every data file, no window\n"
                     "  --windowed           a 1280 x 720 window instead of fullscreen\n"
                     "For testing:\n"
                     "  --room <id>          start a new game in that room (skips the title)\n"
                     "  --set <var>=<value>  set a story variable first (repeatable; true/false allowed)\n"
                     "  --screenshot <file>  save the 480 x 270 canvas as a PNG and quit\n"
                     "  --after <seconds>    when to take the screenshot (default 2)\n";
    }
} // namespace

int main(int argc, char* argv[])
{
    bool check = false;
    bool windowed = false;
    std::string room;
    std::vector<std::pair<std::string, int>> values;
    std::string screenshot;
    float after = 2.0f;
    std::string walkthroughFile;

    for (int i = 1; i < argc; ++i)
    {
        const std::string_view arg = argv[i];
        const bool hasValue = i + 1 < argc;
        if (arg == "--check") check = true;
        else if (arg == "--windowed") windowed = true;
        else if (arg == "--room" && hasValue) room = argv[++i];
        else if (arg == "--screenshot" && hasValue) screenshot = argv[++i];
        else if (arg == "--after" && hasValue) after = std::strtof(argv[++i], nullptr);
        else if (arg == "--walkthrough" && hasValue) walkthroughFile = argv[++i];
        else if (arg == "--set" && hasValue)
        {
            const std::string pair = argv[++i];
            const auto eq = pair.find('=');
            const std::string value = eq == std::string::npos ? "1" : pair.substr(eq + 1);
            values.emplace_back(pair.substr(0, eq),
                                value == "true" ? 1 : value == "false" ? 0 : std::atoi(value.c_str()));
        }
        else
        {
            printUsage();
            return arg == "--help" || arg == "-h" ? 0 : 2;
        }
    }

    // The assets/ folder is copied next to the program (apps/app/CMakeLists.txt).
    const std::string assets = std::string(GetApplicationDirectory()) + "assets/";

    Game game(assets);
    hollow::Diagnostics diag;
    const bool loaded = game.load(diag);

    // --check: read and check every data file, no window. Each problem is
    // printed with its file and place.
    if (check)
    {
        printDiagnostics(diag);
        std::cout << "Hollow " << hollow::version() << ", raylib " << RAYLIB_VERSION << ", assets: " << assets << '\n'
                  << (loaded ? "data OK" : "data has errors") << " (" << diag.errors.size() << " errors, "
                  << diag.warnings.size() << " warnings)\n";
        return loaded ? 0 : 1;
    }
    printDiagnostics(diag);
    if (!loaded) return 1;

    Walkthrough walkthrough;
    if (!walkthroughFile.empty())
    {
        std::string error;
        if (!walkthrough.load(walkthroughFile, error))
        {
            std::cerr << "error: " << error << '\n';
            return 1;
        }
    }
    const bool scripted = !screenshot.empty() || !walkthroughFile.empty();

    SetConfigFlags(scripted ? FLAG_WINDOW_HIDDEN : FLAG_VSYNC_HINT);
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(1280, 720, "The Last Light");
    SetExitKey(KEY_NULL); // Esc pauses; quitting is in the menus
    if (!windowed && !scripted) ToggleBorderlessWindowed();
    SetTargetFPS(scripted ? 0 : 60); // scripted runs go as fast as they can

    game.initGraphics();
    if ((!room.empty() || !values.empty() || !walkthroughFile.empty()) && !game.startAt(room, values))
    {
        game.shutdownGraphics();
        CloseWindow();
        return 1;
    }

    float time = 0.0f;
    int status = 0;
    while (!WindowShouldClose() && !game.quitRequested())
    {
        // A fixed step in scripted runs, so their times are game time.
        const float dt = scripted ? 1.0f / 60.0f : GetFrameTime();
        if (!walkthroughFile.empty() && !walkthrough.step(game, dt))
        {
            std::cout << walkthrough.message() << '\n';
            status = walkthrough.failed() ? 1 : 0;
            break;
        }
        game.frame(dt);
        time += dt;
        if (!screenshot.empty() && time >= after)
        {
            status = game.saveCanvas(screenshot) ? 0 : 1;
            break;
        }
    }
    game.shutdownGraphics();

    CloseWindow();
    return status;
}
