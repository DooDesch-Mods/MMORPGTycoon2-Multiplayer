from pathlib import Path
import zipfile

ROOT = Path(__file__).resolve().parents[1]
with zipfile.ZipFile(ROOT / "artifacts/SDL3-devel.zip") as archive:
    for info in archive.infolist():
        parts = Path(info.filename).parts
        if len(parts) > 1 and parts[1] in ("include", "lib") and not info.is_dir():
            target = ROOT / "include/vendor/SDL3" / Path(*parts[1:])
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(archive.read(info))
for base in (ROOT / "src", ROOT / "include/mt2mp"):
    for path in base.rglob("*"):
        if path.suffix not in (".cpp", ".h"):
            continue
        text = path.read_text(encoding="utf-8")
        text = text.replace("\u2014", "-").replace("\u2013", "-")
        text = text.replace("<SDL.h>", "<SDL3/SDL.h>")
        text = text.replace("imgui_impl_sdl2", "imgui_impl_sdl3").replace("ImGui_ImplSDL2", "ImGui_ImplSDL3")
        text = text.replace('"SDL2.dll"', '"SDL3.dll"').replace("SDL_GL_GetDrawableSize", "SDL_GetWindowSizeInPixels")
        path.write_text(text, encoding="utf-8")
for name in ("src/overlay/gl_overlay.cpp", "src/hooks/hook_shell.cpp"):
    path = ROOT / name
    text = path.read_text(encoding="utf-8")
    text = text.replace("SDL_ShowCursor(opening ? 1 : 0);", "if (opening) SDL_ShowCursor(); else SDL_HideCursor();")
    text = text.replace("SDL_ShowCursor(1);", "SDL_ShowCursor();")
    text = text.replace("SDL_StartTextInput();", "SDL_StartTextInput(hooks::get_game_window());")
    text = text.replace("SDL_StopTextInput();", "SDL_StopTextInput(hooks::get_game_window());")
    if name.endswith("hook_shell.cpp"):
        text = text.replace('#include "mt2mp/hooks/hook_engine.h"', '#include "mt2mp/hooks/hook_engine.h"\n#include "mt2mp/hooks/hook_window.h"')
    path.write_text(text, encoding="utf-8")
path = ROOT / "src/hooks/hook_window.cpp"
text = path.read_text(encoding="utf-8").replace("#include <cstdlib>", "#include <cstdlib>\n#include <SDL3/SDL.h>")
text = text.replace("const char*, int, int, int, int, uint32_t", "const char*, int, int, SDL_WindowFlags")
text = text.replace("void(*)(SDL_Window*, const char*)", "bool(*)(SDL_Window*, const char*)")
text = text.replace("const char* title, int x, int y, int w, int h, uint32_t flags", "const char* title, int w, int h, SDL_WindowFlags flags")
text = text.replace("orig_createWindow(title, x, y, w, h, flags)", "orig_createWindow(title, w, h, flags)")
text = text.replace("static void detour_setWindowTitle", "static bool detour_setWindowTitle")
text = text.replace("    orig_setWindowTitle(window, title);", "    const bool result = orig_setWindowTitle(window, title);")
needle = "        if (g_on_title) g_on_title(g_time_info);\n    }\n}"
assert needle in text
text = text.replace(needle, "        if (g_on_title) g_on_title(g_time_info);\n    }\n    return result;\n}")
path.write_text(text, encoding="utf-8")
path = ROOT / "src/hooks/hook_renderer.cpp"
text = path.read_text(encoding="utf-8").replace("void(*)(SDL_Window* window)", "bool(*)(SDL_Window* window)")
text = text.replace("static void detour_swap_window", "static bool detour_swap_window").replace("    orig_swap_window(window);", "    return orig_swap_window(window);")
path.write_text(text, encoding="utf-8")
path = ROOT / "CMakeLists.txt"
text = path.read_text(encoding="utf-8").replace("imgui_impl_sdl2.cpp", "imgui_impl_sdl3.cpp")
text = text.replace("include/vendor/SDL2/SDL2.lib", "include/vendor/SDL3/lib/x64/SDL3.lib").replace("include/vendor/SDL2", "include/vendor/SDL3/include").replace("SDL2", "SDL3")
text = text.replace("file(GLOB_RECURSE MOD_SOURCES", "file(GLOB_RECURSE MOD_SOURCES CONFIGURE_DEPENDS")
path.write_text(text, encoding="utf-8")
print("SDL3 headers extracted; renderer, window, overlay and CMake ported")
