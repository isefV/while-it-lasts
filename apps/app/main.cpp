#include <hollow/hollow.hpp>

#include <raylib.h>

#include <iostream>
#include <string>
#include <string_view>

int main(int argc, char* argv[])
{
    // The assets/ folder is copied next to the program (apps/app/CMakeLists.txt).
    const std::string assets = std::string(GetApplicationDirectory()) + "assets/";
    const std::string greeting = hollow::greet("Hollow");

    // --check: no window, for the CTest run.
    if (argc > 1 && std::string_view(argv[1]) == "--check")
    {
        std::cout << greeting << " raylib " << RAYLIB_VERSION << ", assets: " << assets << '\n';
        return DirectoryExists(assets.c_str()) ? 0 : 1;
    }

    InitWindow(1280, 720, "Hollow");
    SetTargetFPS(60);

    while (!WindowShouldClose())
    {
        BeginDrawing();
        ClearBackground(RAYWHITE);
        DrawText(greeting.c_str(), 40, 40, 40, DARKGRAY);
        DrawText(assets.c_str(), 40, 100, 20, GRAY);
        DrawFPS(40, 140);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
