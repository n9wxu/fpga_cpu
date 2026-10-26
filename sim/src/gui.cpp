// fpga_cpu simulator: SDL2 + Dear ImGui front end.
//
// Windows: display (with keyboard), controls, registers, disassembly,
// memory, serial console, board I/O and statistics.  The CPU core, the
// devices and the video renderer are the same C code the headless fsim uses.
// The core headers come first: their addressing-mode enum has an M_PI that
// <math.h> (pulled in by SDL and ImGui) would otherwise turn into a number.
extern "C" {
#include "front.h"
}

#include <SDL.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"

static cpu_t cpu;
static opts_t opt;
static uint32_t frame_px[SCREEN_W * SCREEN_H];

// ---- execution control ----

enum RunMode { STOPPED, RUNNING, STEP_OVER, STEP_OUT };
static RunMode mode = RUNNING;
static uint32_t over_target, over_sp, out_sp;
static std::vector<uint8_t> bp(1u << 23); // one flag per 2-byte address
static bool max_speed = false;
static std::string serial;
static double achieved_mhz;

static uint32_t sp_of(const cpu_t *c) { return c->r[14] | (uint32_t)(c->r[15] & 0xFF) << 16; }

static bool decode_at(uint32_t pc, dinst_t *d)
{
    return decode(cpu.var, cpu_peek16(&cpu, pc), cpu_peek16(&cpu, (pc + 2) & 0xFFFFFF), d) != 0;
}

static void serial_hook(void *, int ch)
{
    if (ch == '\r')
        return;
    if (ch == 8) {
        if (!serial.empty())
            serial.pop_back();
        return;
    }
    serial.push_back((char)ch);
    if (serial.size() > 200000)
        serial.erase(0, 100000);
}

static void reset_machine()
{
    memset(cpu.sram, 0, SRAM_SIZE);
    cpu_reset(&cpu);
    load_image(&cpu, opt.image, opt.load);
    cpu.pc = opt.entry == 0xFFFFFFFF ? opt.load : opt.entry;
    serial.clear();
}

static void step_one() { cpu_step(&cpu); }

static void start_step_over()
{
    dinst_t d;
    if (decode_at(cpu.pc, &d) && (d.op == OP_JAL || d.op == OP_JALR)) {
        over_target = (cpu.pc + d.len) & 0xFFFFFF;
        over_sp = sp_of(&cpu);
        mode = STEP_OVER;
        step_one(); // leave the breakpoint we may be sitting on
    } else {
        step_one();
    }
}

static void start_step_out()
{
    out_sp = sp_of(&cpu);
    mode = STEP_OUT;
}

// Run up to `budget` cycles.  Stops at breakpoints (except on the first
// instruction, so Run can leave a breakpoint) and at step-over/out targets.
static void run_cycles(uint64_t budget)
{
    uint64_t end = cpu.st.cycles + budget;
    bool first = true;
    while (cpu.st.cycles < end) {
        if (cpu.halted) {
            mode = STOPPED;
            return;
        }
        uint32_t pc = cpu.pc;
        if (!first && bp[pc >> 1]) {
            mode = STOPPED;
            return;
        }
        first = false;
        if (mode == STEP_OVER && pc == over_target && sp_of(&cpu) >= over_sp) {
            mode = STOPPED;
            return;
        }
        bool is_ret = false;
        if (mode == STEP_OUT) {
            dinst_t d;
            is_ret = decode_at(pc, &d) && d.op == OP_JR;
        }
        cpu_step(&cpu);
        if (is_ret && sp_of(&cpu) >= out_sp) {
            mode = STOPPED;
            return;
        }
    }
}

// ---- keyboard ----

static bool display_focused;

static void key_event(const SDL_Event &e)
{
    if (!display_focused)
        return;
    if (e.type == SDL_TEXTINPUT) {
        for (const char *p = e.text.text; *p; p++)
            if ((unsigned char)*p >= 32 && (unsigned char)*p < 127)
                dev_key(&cpu.dev, (uint16_t)*p);
        return;
    }
    if (e.type != SDL_KEYDOWN)
        return;
    SDL_Keycode k = e.key.keysym.sym;
    uint16_t mod = e.key.keysym.mod;
    int code = -1;
    switch (k) {
    case SDLK_RETURN: case SDLK_KP_ENTER: code = 13; break;
    case SDLK_BACKSPACE: code = 8; break;
    case SDLK_TAB: code = 9; break;
    case SDLK_ESCAPE: code = 27; break;
    case SDLK_DELETE: code = 0x7F; break;
    case SDLK_UP: code = 0x80; break;
    case SDLK_DOWN: code = 0x81; break;
    case SDLK_LEFT: code = 0x82; break;
    case SDLK_RIGHT: code = 0x83; break;
    case SDLK_HOME: code = 0x84; break;
    case SDLK_END: code = 0x85; break;
    case SDLK_PAGEUP: code = 0x86; break;
    case SDLK_PAGEDOWN: code = 0x87; break;
    case SDLK_INSERT: code = 0x88; break;
    default:
        if (k >= SDLK_F1 && k <= SDLK_F12)
            code = 0x90 + (k - SDLK_F1);
        else if ((mod & KMOD_CTRL) && k >= SDLK_a && k <= SDLK_z)
            code = 1 + (k - SDLK_a);
    }
    if (code >= 0)
        dev_key(&cpu.dev, (uint16_t)code);
}

// FSIM_GUI_SELFTEST=1: exercise breakpoints, step over and step out without
// a window (expects sim/demo/video.s and its symbols).  Prints the PCs.
static int selftest()
{
    uint32_t fill, text;
    if (!sym_find("fill32", &fill) || !sym_find("text", &text)) {
        fprintf(stderr, "selftest needs --sym with fill32 and text\n");
        return 1;
    }
    auto go = [](RunMode m) { mode = m; while (mode != STOPPED) run_cycles(1000000); };
    bp[fill >> 1] = 1;
    go(RUNNING);
    uint32_t call_ret = (uint32_t)(cpu.r[12] | cpu.r[13] << 16);
    printf("break at fill32: pc=%06x (want %06x)\n", cpu.pc, fill);
    bp[fill >> 1] = 0; // fill32 loops back to its own entry
    start_step_out();
    go(STEP_OUT);
    printf("step out:        pc=%06x (want %06x)\n", cpu.pc, call_ret);
    bp[text >> 1] = 1;
    go(RUNNING); // stops at text, entered by a JAL
    uint32_t after = (uint32_t)(cpu.r[12] | cpu.r[13] << 16);
    bp[text >> 1] = 0;
    start_step_out();
    go(STEP_OUT);
    printf("step out:        pc=%06x (want %06x)\n", cpu.pc, after);
    // step over the next JAL
    dinst_t d;
    while (decode_at(cpu.pc, &d) && d.op != OP_JAL)
        step_one();
    uint32_t want = cpu.pc + d.len;
    start_step_over();
    if (mode == STEP_OVER)
        go(STEP_OVER);
    printf("step over JAL:   pc=%06x (want %06x)\n", cpu.pc, want);
    // keyboard translation: text, Enter, Up, F1, Ctrl-C
    display_focused = true;
    SDL_Event e = {};
    e.type = SDL_TEXTINPUT;
    strcpy(e.text.text, "a");
    key_event(e);
    e = {};
    e.type = SDL_KEYDOWN;
    SDL_Keycode keys[] = { SDLK_RETURN, SDLK_UP, SDLK_F1, SDLK_c };
    for (SDL_Keycode k : keys) {
        e.key.keysym.sym = k;
        e.key.keysym.mod = k == SDLK_c ? KMOD_LCTRL : 0;
        key_event(e);
    }
    printf("keys:");
    for (int i = 0; i < 6; i++)
        printf(" %x", dev_read(&cpu, IO_KBD_DATA));
    printf(" (want 61 d 80 90 3 ffff)\n");
    return 0;
}

// ---- windows ----

static void win_display(SDL_Texture *tex)
{
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(980, 590), ImGuiCond_FirstUseEver);
    ImGui::Begin("Display (click to type)", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    display_focused = ImGui::IsWindowFocused();
    ImVec2 avail = ImGui::GetContentRegionAvail();
    float s = std::min(avail.x / SCREEN_W, avail.y / SCREEN_H);
    if (s <= 0)
        s = 0.1f;
    ImVec2 sz(SCREEN_W * s, SCREEN_H * s);
    ImVec2 cur = ImGui::GetCursorPos();
    ImGui::SetCursorPos(ImVec2(cur.x + (avail.x - sz.x) / 2, cur.y));
    ImGui::Image((ImTextureID)(intptr_t)tex, sz);
    if (display_focused)
        ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), IM_COL32(80, 160, 255, 255));
    ImGui::End();
}

static void win_controls()
{
    ImGui::SetNextWindowPos(ImVec2(980, 0), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(700, 64), ImGuiCond_FirstUseEver);
    ImGui::Begin("Controls");
    bool stopped = mode == STOPPED;
    ImGui::BeginDisabled(!stopped || cpu.halted);
    if (ImGui::Button("Run (F5)"))
        mode = RUNNING, run_cycles(1);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(stopped);
    if (ImGui::Button("Stop"))
        mode = STOPPED;
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!stopped || cpu.halted);
    if (ImGui::Button("Step (F11)"))
        step_one();
    ImGui::SameLine();
    if (ImGui::Button("Over (F10)"))
        start_step_over();
    ImGui::SameLine();
    if (ImGui::Button("Out"))
        start_step_out();
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Reset"))
        reset_machine();
    ImGui::SameLine();
    ImGui::SetNextItemWidth(110);
    int sp = max_speed ? 1 : 0;
    if (ImGui::Combo("##speed", &sp, "50 MHz\0Max\0"))
        max_speed = sp == 1;
    ImGui::SameLine();
    if (cpu.halted)
        ImGui::TextColored(ImVec4(1, 0.5f, 0.3f, 1), "halted: %s", cpu.halt_reason);
    else if (mode == STOPPED)
        ImGui::Text("stopped at %06x", cpu.pc);
    else
        ImGui::Text("running %.1f MHz", achieved_mhz);
    ImGui::End();
}

static void win_registers()
{
    ImGui::SetNextWindowPos(ImVec2(980, 64), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(320, 430), ImGuiCond_FirstUseEver);
    ImGui::Begin("Registers");
    bool edit = mode == STOPPED;
    static const char *const pname[8] = { "p0", "p1", "p2", "p3", "p4", "p5", "lr", "sp" };
    if (ImGui::BeginTable("regs", 4, ImGuiTableFlags_SizingFixedFit)) {
        for (int p = 0; p < 8; p++) {
            ImGui::TableNextRow();
            for (int h = 0; h < 2; h++) {
                int r = 2 * p + h;
                ImGui::TableNextColumn();
                ImGui::Text("r%-2d", r);
                ImGui::TableNextColumn();
                ImGui::PushID(r);
                ImGui::SetNextItemWidth(52);
                if (edit)
                    ImGui::InputScalar("##r", ImGuiDataType_U16, &cpu.r[r], nullptr, nullptr, "%04X",
                                       ImGuiInputTextFlags_CharsHexadecimal);
                else
                    ImGui::Text("%04X", cpu.r[r]);
                ImGui::PopID();
            }
        }
        ImGui::EndTable();
    }
    ImGui::Separator();
    for (int p = 0; p < 8; p++) {
        uint32_t v = cpu.r[2 * p] | (uint32_t)cpu.r[2 * p + 1] << 16;
        const char *sname = nullptr;
        uint32_t off = 0;
        if (nsyms && (v & 0xFFFFFF) < SRAM_SIZE)
            sname = sym_near(v & 0xFFFFFF, &off);
        if (sname && off < 0x10000)
            ImGui::Text("%-2s %08X  %s+%u", pname[p], v, sname, off);
        else
            ImGui::Text("%-2s %08X", pname[p], v);
    }
    ImGui::Separator();
    uint32_t pc = cpu.pc;
    ImGui::Text("PC");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(70);
    if (edit) {
        if (ImGui::InputScalar("##pc", ImGuiDataType_U32, &pc, nullptr, nullptr, "%06X", ImGuiInputTextFlags_CharsHexadecimal))
            cpu.pc = pc & 0xFFFFFE;
    } else {
        ImGui::Text("%06X", pc);
    }
    ImGui::SameLine();
    bool f[4] = { cpu.n != 0, cpu.z != 0, cpu.cf != 0, cpu.v != 0 };
    const char *fn[4] = { "N", "Z", "C", "V" };
    ImGui::BeginDisabled(!edit);
    for (int i = 0; i < 4; i++) {
        ImGui::SameLine();
        ImGui::Checkbox(fn[i], &f[i]);
    }
    ImGui::EndDisabled();
    if (edit)
        cpu.n = f[0], cpu.z = f[1], cpu.cf = f[2], cpu.v = f[3];
    ImGui::Text("cycles %llu", (unsigned long long)cpu.st.cycles);
    ImGui::Text("insts  %llu", (unsigned long long)cpu.st.insts);
    ImGui::Text("frame  %u  line %u", dev_frame(cpu.st.cycles), dev_line(cpu.st.cycles));
    ImGui::End();
}

static bool parse_addr(const char *s, uint32_t *a)
{
    while (*s == ' ')
        s++;
    if (!*s)
        return false;
    if (sym_find(s, a))
        return true;
    char *e;
    unsigned long v = strtoul(s[0] == '$' ? s + 1 : s, &e, 16);
    if (*e)
        return false;
    *a = (uint32_t)v & 0xFFFFFF;
    return true;
}

static void win_disasm()
{
    static uint32_t top;
    static bool follow = true;
    static char go[64];
    ImGui::SetNextWindowPos(ImVec2(1300, 64), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(380, 630), ImGuiCond_FirstUseEver);
    ImGui::Begin("Disassembly");
    ImGui::SetNextItemWidth(160);
    if (ImGui::InputTextWithHint("##go", "address or symbol", go, sizeof go, ImGuiInputTextFlags_EnterReturnsTrue)) {
        uint32_t a;
        if (parse_addr(go, &a))
            top = a & ~1u, follow = false;
    }
    ImGui::SameLine();
    ImGui::Checkbox("follow PC", &follow);
    ImGui::Separator();
    float lh = ImGui::GetTextLineHeightWithSpacing();
    int rows = (int)(ImGui::GetContentRegionAvail().y / lh);
    if (rows < 4)
        rows = 4;
    if (follow) {
        // keep the PC inside the window, a few lines from the top
        uint32_t a = top;
        bool seen = false;
        for (int i = 0; i < rows - 3; i++) {
            if (a == cpu.pc) {
                seen = true;
                break;
            }
            dinst_t d;
            a = (a + (decode_at(a, &d) ? d.len : 2)) & 0xFFFFFF;
        }
        if (!seen) {
            uint32_t off;
            const char *s = sym_near(cpu.pc, &off);
            top = (s && off < 24) ? cpu.pc - off : cpu.pc;
        }
    }
    ImGui::BeginChild("lines");
    uint32_t a = top;
    for (int i = 0; i < rows; i++) {
        const char *lab = nsyms ? sym_at(a) : nullptr;
        if (lab) {
            ImGui::TextColored(ImVec4(0.6f, 0.8f, 1, 1), "%s:", lab);
            if (++i >= rows)
                break;
        }
        dinst_t d;
        char text[96] = "??";
        int len = 2;
        if (decode_at(a, &d)) {
            disasm(&d, a, text, sizeof text);
            len = d.len;
            // annotate branch targets with symbols
            if (op_info[d.op].cls == CLS_BR || op_info[d.op].cls == CLS_JAL || op_info[d.op].cls == CLS_LEAPC) {
                uint32_t t = (a + (uint32_t)d.imm) & 0xFFFFFF, off;
                const char *s = nsyms ? sym_near(t, &off) : nullptr;
                if (s) {
                    size_t n = strlen(text);
                    if (off)
                        snprintf(text + n, sizeof text - n, " <%s+%u>", s, off);
                    else
                        snprintf(text + n, sizeof text - n, " <%s>", s);
                }
            }
        }
        char hex[16];
        if (len == 4)
            snprintf(hex, sizeof hex, "%04X %04X", cpu_peek16(&cpu, a), cpu_peek16(&cpu, a + 2));
        else
            snprintf(hex, sizeof hex, "%04X     ", cpu_peek16(&cpu, a));
        char line[160];
        snprintf(line, sizeof line, "%c %06X  %s  %s", bp[a >> 1] ? '*' : ' ', a, hex, text);
        bool at_pc = a == cpu.pc;
        if (at_pc)
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 0.3f, 1));
        else if (bp[a >> 1])
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0.4f, 0.4f, 1));
        ImGui::PushID((int)a);
        if (ImGui::Selectable(line, at_pc))
            bp[a >> 1] ^= 1;
        ImGui::PopID();
        if (at_pc || bp[a >> 1])
            ImGui::PopStyleColor();
        a = (a + (uint32_t)len) & 0xFFFFFF;
    }
    if (ImGui::IsWindowHovered() && ImGui::GetIO().MouseWheel != 0) {
        follow = false;
        top = (top - (uint32_t)(int)(ImGui::GetIO().MouseWheel * 6)) & 0xFFFFFE;
    }
    ImGui::EndChild();
    ImGui::End();
}

static void win_memory()
{
    static char go[64];
    static int scroll_to = -1;
    ImGui::SetNextWindowPos(ImVec2(0, 590), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(600, 410), ImGuiCond_FirstUseEver);
    ImGui::Begin("Memory");
    ImGui::SetNextItemWidth(160);
    if (ImGui::InputTextWithHint("##go", "address or symbol", go, sizeof go, ImGuiInputTextFlags_EnterReturnsTrue)) {
        uint32_t a;
        if (parse_addr(go, &a))
            scroll_to = (int)(a / 16);
    }
    ImGui::SameLine();
    ImGui::TextDisabled("SRAM 000000-0FFFFF, ROM FF0000-FFFFFF");
    ImGui::BeginChild("hex");
    float lh = ImGui::GetTextLineHeightWithSpacing();
    const int nrows = 0x1000000 / 16;
    if (scroll_to >= 0) {
        ImGui::SetScrollY(scroll_to * lh);
        scroll_to = -1;
    }
    ImGuiListClipper clip;
    clip.Begin(nrows, lh);
    while (clip.Step())
        for (int row = clip.DisplayStart; row < clip.DisplayEnd; row++) {
            uint32_t a = (uint32_t)row * 16;
            char line[100];
            int n = snprintf(line, sizeof line, "%06X ", a);
            bool mapped = a < SRAM_SIZE || a >= ROM_BASE;
            char asc[17];
            for (int i = 0; i < 16; i++) {
                uint32_t x = a + (uint32_t)i;
                if (!mapped) {
                    n += snprintf(line + n, sizeof line - (size_t)n, i == 8 ? "  --" : " --");
                    asc[i] = ' ';
                    continue;
                }
                uint8_t b = x < SRAM_SIZE ? cpu.sram[x] : cpu.rom[x - ROM_BASE];
                n += snprintf(line + n, sizeof line - (size_t)n, i == 8 ? "  %02X" : " %02X", b);
                asc[i] = b >= 32 && b < 127 ? (char)b : '.';
            }
            asc[16] = 0;
            ImGui::Text("%s  %s", line, asc);
        }
    ImGui::EndChild();
    ImGui::End();
}

static void win_serial()
{
    static char input[256];
    static size_t last_len;
    ImGui::SetNextWindowPos(ImVec2(600, 590), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(380, 410), ImGuiCond_FirstUseEver);
    ImGui::Begin("Serial console");
    float h = ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing();
    ImGui::BeginChild("out", ImVec2(0, h), ImGuiChildFlags_Borders);
    ImGui::TextUnformatted(serial.data(), serial.data() + serial.size());
    if (serial.size() != last_len && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 40)
        ImGui::SetScrollHereY(1.0f);
    last_len = serial.size();
    ImGui::EndChild();
    ImGui::SetNextItemWidth(-60);
    if (ImGui::InputText("##in", input, sizeof input, ImGuiInputTextFlags_EnterReturnsTrue)) {
        for (const char *p = input; *p; p++)
            dev_con_in(&cpu.dev, (uint8_t)*p);
        dev_con_in(&cpu.dev, '\n');
        input[0] = 0;
        ImGui::SetKeyboardFocusHere(-1);
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear"))
        serial.clear();
    ImGui::End();
}

// segments a..g for hex digits 0-F
static const uint8_t seg_hex[16] = { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07,
                                     0x7F, 0x6F, 0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71 };

static void draw_digit(ImDrawList *dl, ImVec2 o, int segs, bool dp)
{
    const float w = 18, h = 32, t = 3;
    ImU32 on = IM_COL32(255, 40, 30, 255), off = IM_COL32(60, 20, 20, 255);
    struct { float x0, y0, x1, y1; } s[7] = {
        { t, 0, w - t, t }, { w - t, t, w, h / 2 - t / 2 }, { w - t, h / 2 + t / 2, w, h - t },
        { t, h - t, w - t, h }, { 0, h / 2 + t / 2, t, h - t }, { 0, t, t, h / 2 - t / 2 }, { t, h / 2 - t / 2, w - t, h / 2 + t / 2 },
    };
    for (int i = 0; i < 7; i++)
        dl->AddRectFilled(ImVec2(o.x + s[i].x0, o.y + s[i].y0), ImVec2(o.x + s[i].x1, o.y + s[i].y1), (segs >> i) & 1 ? on : off);
    dl->AddCircleFilled(ImVec2(o.x + w + 4, o.y + h - 2), 2, dp ? on : off);
}

static void win_board()
{
    iodev_t *d = &cpu.dev;
    ImGui::SetNextWindowPos(ImVec2(980, 494), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(320, 200), ImGuiCond_FirstUseEver);
    ImGui::Begin("Board");
    ImDrawList *dl = ImGui::GetWindowDrawList();
    // 7-segment display: digit 3 is leftmost
    ImVec2 o = ImGui::GetCursorScreenPos();
    for (int i = 0; i < 4; i++) {
        int dig = 3 - i;
        bool blank = (d->seg7_ctrl >> (4 + dig)) & 1;
        int segs = blank ? 0 : seg_hex[(d->seg7 >> (4 * dig)) & 15];
        draw_digit(dl, ImVec2(o.x + 4 + i * 30.0f, o.y + 2), segs, (d->seg7_ctrl >> dig) & 1);
    }
    ImGui::Dummy(ImVec2(130, 38));
    // LEDs: LD7 on the left, as on the board
    ImGui::Text("LD");
    ImGui::SameLine();
    o = ImGui::GetCursorScreenPos();
    for (int i = 0; i < 8; i++) {
        bool on = (d->leds >> (7 - i)) & 1;
        dl->AddCircleFilled(ImVec2(o.x + 8 + i * 20.0f, o.y + 8), 6, on ? IM_COL32(60, 255, 60, 255) : IM_COL32(20, 60, 20, 255));
    }
    ImGui::Dummy(ImVec2(170, 16));
    ImGui::Text("SW");
    for (int i = 7; i >= 0; i--) {
        ImGui::SameLine();
        bool on = (d->switches >> i) & 1;
        ImGui::PushID(i);
        if (ImGui::Checkbox("##sw", &on))
            d->switches = (uint16_t)((d->switches & ~(1u << i)) | (on ? 1u << i : 0));
        ImGui::PopID();
    }
    ImGui::Text("BTN");
    uint16_t btn = 0;
    for (int i = 3; i >= 0; i--) {
        ImGui::SameLine();
        char lab[8];
        snprintf(lab, sizeof lab, "%d", i);
        ImGui::Button(lab, ImVec2(28, 0));
        if (ImGui::IsItemActive())
            btn |= (uint16_t)(1u << i);
    }
    d->buttons = btn;
    ImGui::TextDisabled("sound: %u/%u/%u Hz", d->snd[0][0], d->snd[1][0], d->snd[2][0]);
    ImGui::End();
}

static double pct(uint64_t a, uint64_t b) { return b ? 100.0 * (double)a / (double)b : 0.0; }

static void win_stats()
{
    const stats_t *s = &cpu.st;
    ImGui::SetNextWindowPos(ImVec2(980, 694), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(700, 306), ImGuiCond_FirstUseEver);
    ImGui::Begin("Statistics");
    ImGui::Text("IPC %.3f  (CPI %.3f)   16-bit instructions %.1f%%", s->cycles ? (double)s->insts / (double)s->cycles : 0.0,
                s->insts ? (double)s->cycles / (double)s->insts : 0.0, pct(s->len16, s->insts));
    ImGui::Text("I-cache hit %.2f%%  misses %llu  stream-buffer hits %llu (+%llu partial)", pct(s->ic_hit, s->ic_access),
                (unsigned long long)s->ic_miss, (unsigned long long)s->sb_hit, (unsigned long long)s->sb_partial);
    ImGui::Text("branches %llu, %.1f%% taken", (unsigned long long)s->branches, pct(s->taken, s->branches));
    ImGui::Separator();
    ImGui::Text("Stall cycles (%% of all cycles)");
    struct { const char *n; uint64_t v; } st[] = {
        { "ifetch", s->st_ifetch }, { "taken branch", s->st_branch }, { "load-use", s->st_loaduse },
        { "video", s->st_video }, { "I/O", s->st_io }, { "pair split", s->st_pair },
    };
    for (auto &x : st) {
        float f = s->cycles ? (float)x.v / (float)s->cycles : 0.0f;
        char lab[48];
        snprintf(lab, sizeof lab, "%llu (%.1f%%)", (unsigned long long)x.v, 100.0f * f);
        ImGui::ProgressBar(f, ImVec2(220, 0), lab);
        ImGui::SameLine();
        ImGui::Text("%s", x.n);
    }
    ImGui::Separator();
    ImGui::Text("fusion candidates:");
    for (int i = 0; i < FU__COUNT; i++) {
        ImGui::SameLine();
        ImGui::Text(" %s %llu", fuse_names[i], (unsigned long long)s->fuse_cand[i]);
    }
    ImGui::End();
}

// ---- audio ----

static void audio_cb(void *, Uint8 *stream, int len)
{
    sound_fill(&cpu.dev, (float *)stream, len / (int)sizeof(float), 48000);
}

// ---- main ----

static void save_window(SDL_Renderer *ren, const char *path)
{
    int w, h;
    SDL_GetRendererOutputSize(ren, &w, &h);
    SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
    SDL_RenderReadPixels(ren, nullptr, SDL_PIXELFORMAT_ARGB8888, s->pixels, s->pitch);
    SDL_Surface *rgb = SDL_ConvertSurfaceFormat(s, SDL_PIXELFORMAT_BGR24, 0); // 24-bit BMP: readable everywhere
    if (!rgb || SDL_SaveBMP(rgb, path))
        fprintf(stderr, "%s: %s\n", path, SDL_GetError());
    SDL_FreeSurface(rgb);
    SDL_FreeSurface(s);
}

int main(int argc, char **argv)
{
    if (opts_parse(&opt, argc, argv, 1))
        opts_usage(1);
    if (setup_cpu(&cpu, &opt))
        return 2;
    cpu.dev.con_stdin = 0;
    cpu.dev.con_hook = serial_hook;
    max_speed = opt.max_speed;
    mode = opt.paused ? STOPPED : RUNNING;
    if (getenv("FSIM_GUI_SELFTEST"))
        return selftest();

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
        fprintf(stderr, "SDL: %s\n", SDL_GetError());
        return 1;
    }
    SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
    char title[300];
    snprintf(title, sizeof title, "fsim-gui: %s (variant %s)", opt.image, opt.vname);
    SDL_Window *win = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1680, 1000,
                                       SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!win || !ren) {
        fprintf(stderr, "SDL: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Texture *tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_STREAMING, SCREEN_W, SCREEN_H);
    SDL_SetTextureScaleMode(tex, SDL_ScaleModeLinear);

    SDL_AudioSpec want = {}, have;
    want.freq = 48000;
    want.format = AUDIO_F32SYS;
    want.channels = 1;
    want.samples = 1024;
    want.callback = audio_cb;
    SDL_AudioDeviceID audio = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (audio)
        SDL_PauseAudioDevice(audio, 0);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    static std::string ini;
    if (opt.frames || !getenv("HOME")) {
        io.IniFilename = nullptr; // scripted runs get the default layout
    } else {
        ini = std::string(getenv("HOME")) + "/.fsim_gui.ini";
        io.IniFilename = ini.c_str();
    }
    ImGui::StyleColorsDark();
    // HiDPI: render the font at the framebuffer scale
    int ww, wh, rw, rh;
    SDL_GetWindowSize(win, &ww, &wh);
    SDL_GetRendererOutputSize(ren, &rw, &rh);
    float scale = ww ? (float)rw / (float)ww : 1.0f;
    SDL_RenderSetScale(ren, scale, scale);
    ImFontConfig fc;
    fc.OversampleH = 1;
    ImFont *font = io.Fonts->AddFontFromFileTTF("/System/Library/Fonts/Menlo.ttc", 13.0f * scale, &fc);
    if (!font) {
        fc.SizePixels = 13.0f * scale;
        io.Fonts->AddFontDefault(&fc);
    }
    io.FontGlobalScale = 1.0f / scale;
    ImGui_ImplSDL2_InitForSDLRenderer(win, ren);
    ImGui_ImplSDLRenderer2_Init(ren);

    Uint64 freq = SDL_GetPerformanceFrequency(), last = SDL_GetPerformanceCounter();
    double mhz_cycles = 0, mhz_time = 0;
    int frame = 0;
    bool quit = false;
    while (!quit) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            ImGui_ImplSDL2_ProcessEvent(&e);
            if (e.type == SDL_QUIT)
                quit = true;
            if (e.type == SDL_KEYDOWN && !display_focused && mode == STOPPED && !cpu.halted && !io.WantTextInput) {
                if (e.key.keysym.sym == SDLK_F11) step_one();
                if (e.key.keysym.sym == SDLK_F10) start_step_over();
            }
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_F5 && mode == STOPPED && !cpu.halted)
                mode = RUNNING, run_cycles(1);
            else
                key_event(e);
        }
        SDL_Keymod km = SDL_GetModState();
        cpu.dev.kbd_mod = (uint16_t)(((km & KMOD_SHIFT) ? 1 : 0) | ((km & KMOD_CTRL) ? 2 : 0) | ((km & KMOD_ALT) ? 4 : 0));

        // run the CPU for the time since the last frame (real time) or ~14 ms (max)
        Uint64 now = SDL_GetPerformanceCounter();
        double dt = (double)(now - last) / (double)freq;
        last = now;
        if (dt > 0.1)
            dt = 0.1;
        if (opt.frames)
            dt = 1.0 / 60; // scripted runs: exactly one display frame per loop
        if (mode != STOPPED) {
            uint64_t c0 = cpu.st.cycles;
            if (max_speed && !opt.frames) {
                Uint64 stop = now + freq * 14 / 1000;
                while (mode != STOPPED && SDL_GetPerformanceCounter() < stop)
                    run_cycles(200000);
            } else {
                run_cycles((uint64_t)(dt * CPU_HZ));
            }
            mhz_cycles += (double)(cpu.st.cycles - c0);
            if (opt.max_cycles && cpu.st.cycles >= opt.max_cycles)
                mode = STOPPED;
        }
        mhz_time += dt;
        if (mhz_time >= 0.5) {
            achieved_mhz = mhz_cycles / mhz_time / 1e6;
            mhz_cycles = mhz_time = 0;
        }

        // display: blink the cursor at about 2 Hz of emulated time
        video_render(&cpu, frame_px, opt.true_color, (dev_frame(cpu.st.cycles) / 16) % 2 == 0);
        SDL_UpdateTexture(tex, nullptr, frame_px, SCREEN_W * 4);

        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        win_display(tex);
        win_controls();
        win_registers();
        win_disasm();
        win_memory();
        win_serial();
        win_board();
        win_stats();
        ImGui::Render();
        SDL_SetRenderDrawColor(ren, 30, 30, 34, 255);
        SDL_RenderClear(ren);
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), ren);
        frame++;
        if (opt.frames && frame >= opt.frames) {
            if (opt.shot_window)
                save_window(ren, opt.shot_window);
            quit = true;
        }
        SDL_RenderPresent(ren);
    }
    if (opt.screenshot) {
        video_render(&cpu, frame_px, opt.true_color, 1);
        write_ppm(opt.screenshot, frame_px, SCREEN_W, SCREEN_H);
    }
    if (audio)
        SDL_CloseAudioDevice(audio);
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    int rc = cpu.halted ? cpu.exit_code : 0;
    cpu_free(&cpu);
    return rc;
}
