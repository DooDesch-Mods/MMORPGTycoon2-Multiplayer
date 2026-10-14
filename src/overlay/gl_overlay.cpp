#include "mt2mp/overlay/gl_overlay.h"
#include "mt2mp/overlay/cursor_renderer.h"
#include "mt2mp/overlay/lobby_overlay.h"
#include "mt2mp/overlay/chat_overlay.h"
#include "mt2mp/overlay/notification_overlay.h"
#include "mt2mp/hooks/hook_window.h"
#include "mt2mp/hooks/hook_input.h"
#include "mt2mp/net/steam_networking.h"
#include "mt2mp/core/logger.h"

#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>
#include <SDL3/SDL.h>
#include <windows.h>
#include <gl/GL.h>

// GL extension types
typedef void (APIENTRY* PFNGLBINDVERTEXARRAYPROC)(GLuint);
typedef void (APIENTRY* PFNGLUSEPROGRAMPROC)(GLuint);
typedef void (APIENTRY* PFNGLACTIVETEXTUREPROC)(GLenum);
typedef void (APIENTRY* PFNGLBINDFRAMEBUFFERPROC)(GLenum, GLuint);
typedef void (APIENTRY* PFNGLBINDBUFFERPROC)(GLenum, GLuint);
typedef void (APIENTRY* PFNGLBLENDEQUATIONPROC)(GLenum);
typedef void (APIENTRY* PFNGLBLENDFUNCSEPARATEPROC)(GLenum, GLenum, GLenum, GLenum);

#define GL_CURRENT_PROGRAM      0x8B8D
#define GL_VERTEX_ARRAY_BINDING 0x85B5
#define GL_ACTIVE_TEXTURE       0x84E1
#define GL_ARRAY_BUFFER_BINDING 0x8894
#define GL_FRAMEBUFFER          0x8D40
#define GL_DRAW_FRAMEBUFFER_BINDING 0x8CA6
#define GL_BLEND_SRC_RGB        0x80C9
#define GL_BLEND_DST_RGB        0x80C8
#define GL_BLEND_SRC_ALPHA      0x80CB
#define GL_BLEND_DST_ALPHA      0x80CA
#define GL_BLEND_EQUATION_RGB   0x8009

namespace mt2mp::overlay {

static PFNGLBINDVERTEXARRAYPROC pglBindVertexArray = nullptr;
static PFNGLUSEPROGRAMPROC pglUseProgram = nullptr;
static PFNGLACTIVETEXTUREPROC pglActiveTexture = nullptr;
static PFNGLBINDFRAMEBUFFERPROC pglBindFramebuffer = nullptr;
static PFNGLBINDBUFFERPROC pglBindBuffer = nullptr;
static PFNGLBLENDEQUATIONPROC pglBlendEquation = nullptr;
static PFNGLBLENDFUNCSEPARATEPROC pglBlendFuncSeparate = nullptr;
static bool g_gl_funcs_loaded = false;

static void load_gl_funcs() {
    if (g_gl_funcs_loaded) return;
    pglBindVertexArray = (PFNGLBINDVERTEXARRAYPROC)wglGetProcAddress("glBindVertexArray");
    pglUseProgram = (PFNGLUSEPROGRAMPROC)wglGetProcAddress("glUseProgram");
    pglActiveTexture = (PFNGLACTIVETEXTUREPROC)wglGetProcAddress("glActiveTexture");
    pglBindFramebuffer = (PFNGLBINDFRAMEBUFFERPROC)wglGetProcAddress("glBindFramebuffer");
    pglBindBuffer = (PFNGLBINDBUFFERPROC)wglGetProcAddress("glBindBuffer");
    pglBlendEquation = (PFNGLBLENDEQUATIONPROC)wglGetProcAddress("glBlendEquation");
    pglBlendFuncSeparate = (PFNGLBLENDFUNCSEPARATEPROC)wglGetProcAddress("glBlendFuncSeparate");
    g_gl_funcs_loaded = true;
}

// ── Desktop Icon ─────────────────────────────────────────────────
static void render_desktop_icon() {
    ImGuiIO& io = ImGui::GetIO();

    // Position bottom-left, matching the game's icon column
    float icon_x = 8.0f;
    float icon_y = io.DisplaySize.y - 110.0f;

    ImGui::SetNextWindowPos(ImVec2(icon_x, icon_y), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(96, 100));
    ImGui::SetNextWindowBgAlpha(0.0f); // transparent background

    // Key: NO NoInputs flag, NO NoMove flag -> clickable AND draggable
    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
    ImGui::Begin("##mp_desktop_icon", nullptr, flags);

    bool has_peers = !net::SteamNetworking::instance().peers().empty();
    ImU32 icon_color = has_peers ? IM_COL32(118, 196, 129, 255) : IM_COL32(198, 166, 103, 255); // green or brass
    ImU32 icon_bg = IM_COL32(20, 27, 39, 220);

    // Get window draw list and position
    ImVec2 wpos = ImGui::GetWindowPos();
    auto* draw = ImGui::GetWindowDrawList();
    ImVec2 center(wpos.x + 48, wpos.y + 30);

    // Draw icon background circle
    draw->AddCircleFilled(center, 24, icon_bg, 32);
    draw->AddCircle(center, 24, icon_color, 32, 2.0f);

    // Draw multiplayer symbol (two people)
    draw->AddCircleFilled(ImVec2(center.x - 8, center.y - 8), 5, icon_color, 12);
    draw->AddRectFilled(ImVec2(center.x - 13, center.y - 1), ImVec2(center.x - 3, center.y + 10), icon_color, 2.0f);
    draw->AddCircleFilled(ImVec2(center.x + 8, center.y - 8), 5, icon_color, 12);
    draw->AddRectFilled(ImVec2(center.x + 3, center.y - 1), ImVec2(center.x + 13, center.y + 10), icon_color, 2.0f);

    // Peer count badge
    if (has_peers) {
        size_t count = net::SteamNetworking::instance().peers().size();
        ImVec2 badge_pos(center.x + 18, center.y - 18);
        draw->AddCircleFilled(badge_pos, 9, IM_COL32(220, 50, 50, 255), 12);
        char badge[8];
        snprintf(badge, sizeof(badge), "%zu", count);
        auto bs = ImGui::CalcTextSize(badge);
        draw->AddText(ImVec2(badge_pos.x - bs.x * 0.5f, badge_pos.y - bs.y * 0.5f),
            IM_COL32(255, 255, 255, 255), badge);
    }

    // Clickable button covering the icon area
    ImGui::SetCursorPos(ImVec2(4, 4));
    if (ImGui::InvisibleButton("##mp_btn", ImVec2(88, 56))) {
        LobbyOverlay::instance().toggle();

        // Cursor management
        bool opening = LobbyOverlay::instance().is_visible();
        if (opening) SDL_ShowCursor(); else SDL_HideCursor();
        if (opening) SDL_StartTextInput(hooks::get_game_window());
        else SDL_StopTextInput(hooks::get_game_window());
    }

    // Hover glow
    if (ImGui::IsItemHovered()) {
        draw->AddCircle(center, 26, IM_COL32(255, 255, 255, 100), 32, 2.0f);
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }

    // Label
    const char* label = "Multiplayer";
    auto ts = ImGui::CalcTextSize(label);
    ImGui::SetCursorPos(ImVec2((96 - ts.x) * 0.5f, 62));
    ImGui::TextColored(ImVec4(1, 1, 1, 0.9f), "%s", label);

    ImGui::End();
    ImGui::PopStyleVar();
}

// ──────────────────────────────────────────────────────────────────

GLOverlay& GLOverlay::instance() {
    static GLOverlay inst;
    return inst;
}

bool GLOverlay::init() {
    if (initialized_) return true;
    SDL_Window* window = hooks::get_game_window();
    if (!window) return false;

    load_gl_funcs();
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    io.IniFilename = nullptr;

    // MT2 "Guild Ledger" theme - matches MMORPG Tycoon 2's dark navy UI
    auto RGBA = [](int r, int g, int b, int a = 255) -> ImVec4 {
        return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
    };

    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowPadding           = ImVec2(14, 12);
    s.FramePadding            = ImVec2(10, 6);
    s.CellPadding             = ImVec2(8, 4);
    s.ItemSpacing             = ImVec2(10, 8);
    s.ItemInnerSpacing        = ImVec2(6, 4);
    s.IndentSpacing           = 18.0f;
    s.ScrollbarSize           = 12.0f;
    s.GrabMinSize             = 10.0f;
    s.WindowRounding          = 8.0f;
    s.ChildRounding           = 6.0f;
    s.FrameRounding           = 5.0f;
    s.PopupRounding           = 6.0f;
    s.ScrollbarRounding       = 8.0f;
    s.GrabRounding            = 4.0f;
    s.TabRounding             = 5.0f;
    s.WindowBorderSize        = 1.0f;
    s.FrameBorderSize         = 1.0f;

    auto* c = s.Colors;
    c[ImGuiCol_Text]                  = RGBA(224, 216, 195);
    c[ImGuiCol_TextDisabled]          = RGBA(149, 151, 156);
    c[ImGuiCol_WindowBg]              = RGBA(20, 27, 39, 242);
    c[ImGuiCol_ChildBg]               = RGBA(15, 22, 33, 210);
    c[ImGuiCol_PopupBg]               = RGBA(18, 24, 35, 245);
    c[ImGuiCol_Border]                = RGBA(77, 88, 108, 185);
    c[ImGuiCol_TitleBg]               = RGBA(29, 38, 55);
    c[ImGuiCol_TitleBgActive]         = RGBA(41, 53, 76);
    c[ImGuiCol_MenuBarBg]             = RGBA(25, 33, 47);
    c[ImGuiCol_FrameBg]               = RGBA(30, 40, 57);
    c[ImGuiCol_FrameBgHovered]        = RGBA(40, 52, 73);
    c[ImGuiCol_FrameBgActive]         = RGBA(50, 66, 90);
    c[ImGuiCol_Button]                = RGBA(62, 82, 112);
    c[ImGuiCol_ButtonHovered]         = RGBA(79, 101, 136);
    c[ImGuiCol_ButtonActive]          = RGBA(94, 117, 154);
    c[ImGuiCol_Header]                = RGBA(56, 73, 100);
    c[ImGuiCol_HeaderHovered]         = RGBA(71, 92, 123);
    c[ImGuiCol_HeaderActive]          = RGBA(84, 108, 143);
    c[ImGuiCol_Tab]                   = RGBA(39, 52, 75);
    c[ImGuiCol_TabHovered]            = RGBA(62, 80, 108);
    c[ImGuiCol_TabSelected]           = RGBA(78, 99, 131);
    c[ImGuiCol_TabDimmed]             = RGBA(30, 40, 58);
    c[ImGuiCol_TabDimmedSelected]     = RGBA(57, 74, 100);
    c[ImGuiCol_Separator]             = RGBA(79, 90, 111);
    c[ImGuiCol_ResizeGrip]            = RGBA(110, 130, 160, 90);
    c[ImGuiCol_ResizeGripHovered]     = RGBA(130, 153, 188, 160);
    c[ImGuiCol_ResizeGripActive]      = RGBA(151, 175, 212, 220);
    c[ImGuiCol_ScrollbarBg]           = RGBA(16, 22, 32, 170);
    c[ImGuiCol_ScrollbarGrab]         = RGBA(68, 83, 109);
    c[ImGuiCol_ScrollbarGrabHovered]  = RGBA(86, 105, 136);
    c[ImGuiCol_ScrollbarGrabActive]   = RGBA(102, 123, 157);
    c[ImGuiCol_CheckMark]             = RGBA(198, 166, 103);
    c[ImGuiCol_SliderGrab]            = RGBA(188, 154, 94);
    c[ImGuiCol_SliderGrabActive]      = RGBA(212, 176, 112);

    SDL_GLContext gl_ctx = SDL_GL_GetCurrentContext();
    ImGui_ImplSDL3_InitForOpenGL(window, gl_ctx);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    initialized_ = true;
    LOG_INFO("GL Overlay initialized");
    return true;
}

void GLOverlay::shutdown() {
    if (!initialized_) return;
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    initialized_ = false;
}

void GLOverlay::render() {
    if (!initialized_) { init(); if (!initialized_) return; }

    // Save GL state
    GLint sv[14]; GLboolean sb[4];
    glGetIntegerv(GL_CURRENT_PROGRAM, &sv[0]);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &sv[1]);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &sv[2]);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &sv[3]);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &sv[4]);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &sv[5]);
    glGetIntegerv(GL_VIEWPORT, &sv[6]); // 6,7,8,9
    glGetIntegerv(GL_SCISSOR_BOX, &sv[10]); // 10,11,12,13
    sb[0] = glIsEnabled(GL_BLEND);
    sb[1] = glIsEnabled(GL_CULL_FACE);
    sb[2] = glIsEnabled(GL_DEPTH_TEST);
    sb[3] = glIsEnabled(GL_SCISSOR_TEST);

    if (pglBindFramebuffer) pglBindFramebuffer(GL_FRAMEBUFFER, 0);

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    // Desktop icon is the native Discord icon (hijacked via ShellExecuteA + PhysFS hooks)
    // No ImGui desktop icon needed - the game's own icon system handles it

    // Lobby window (opened by icon click or F10)
    LobbyOverlay::instance().render();
    ChatOverlay::instance().render();

    // Other overlays
    CursorRenderer::instance().render();
    NotificationOverlay::instance().render();

    ImGui::Render();

    int w, h;
    SDL_Window* window = hooks::get_game_window();
    if (window) { SDL_GetWindowSizeInPixels(window, &w, &h); glViewport(0, 0, w, h); }

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // Restore GL state
    if (pglUseProgram) pglUseProgram(sv[0]);
    if (pglBindVertexArray) pglBindVertexArray(sv[1]);
    if (pglActiveTexture) pglActiveTexture(sv[2]);
    glBindTexture(GL_TEXTURE_2D, sv[3]);
    if (pglBindBuffer) pglBindBuffer(0x8892, sv[4]);
    if (pglBindFramebuffer) pglBindFramebuffer(GL_FRAMEBUFFER, sv[5]);
    if (sb[0]) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    if (sb[1]) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    if (sb[2]) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (sb[3]) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
    glViewport(sv[6], sv[7], sv[8], sv[9]);
    glScissor(sv[10], sv[11], sv[12], sv[13]);
}

bool GLOverlay::wants_input() const {
    if (!initialized_) return false;
    return ImGui::GetIO().WantCaptureMouse || ImGui::GetIO().WantCaptureKeyboard;
}

bool GLOverlay::process_event(void* sdl_event) {
    if (!initialized_) return false;
    return ImGui_ImplSDL3_ProcessEvent(reinterpret_cast<SDL_Event*>(sdl_event));
}

} // namespace mt2mp::overlay
