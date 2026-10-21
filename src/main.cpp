// Predictive Buffer HUD v1.3 - menu estilo Mega Hack dentro del juego.
// Lee/escribe el estado interno de ai.predictivebuffer (v4.7 "extended").
// La DLL usa el limite "Wave" cuando el jugador es wave y el limite "All" en el resto;
// este mod escribe esos dos valores cada frame segun el vehiculo actual.
#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/CCScheduler.hpp>
#include <deque>
#include <functional>
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstdint>

using namespace geode::prelude;

namespace lang {
    static inline bool es = false; // false = English (default), true = Espanol

    static char const* tr(char const* en, char const* sp) { return es ? sp : en; }
    static void load() { es = Mod::get()->getSavedValue<int>("ui-lang", 0) == 1; }
    static void toggle() {
        es = !es;
        Mod::get()->setSavedValue<int>("ui-lang", es ? 1 : 0);
    }
}

namespace buf {
    // RVAs dentro de ai.predictivebuffer.dll (VA - 0x180000000)
    constexpr size_t RVA_IDX   = 0x21908; // u32  stage actual (9 = OFF)
    constexpr size_t RVA_FLAG  = 0x2190c; // u8   buffer activo
    constexpr size_t RVA_ALL   = 0x21910; // u32  limite "All" en ticks (todo menos wave)
    constexpr size_t RVA_WAVE  = 0x21914; // u32  limite "Wave" en ticks
    constexpr size_t RVA_LAST  = 0x21918; // i32  ultimos ticks ahorrados (-1/-2 = nada)
    constexpr size_t RVA_PRESS = 0x2191c; // u8   1 = PRESS, 0 = RELEASE
    constexpr DWORD  IMAGE_SIZE = 0x28000; // guarda de version de la DLL
    constexpr int    OFF_IDX = 9;

    constexpr float TICKS_PER_FRAME = 4.f;           // 240 ticks/s a 60 fps
    constexpr float MS_PER_TICK     = 1000.f / 240.f;

    constexpr int VEH_COUNT = 8;
    constexpr int VEH_WAVE = 4;
    constexpr char const* VEH_NAMES[VEH_COUNT]    = {"Cube", "Ship", "UFO", "Ball", "Wave", "Robot", "Spider", "Swing"};
    constexpr char const* VEH_NAMES_ES[VEH_COUNT] = {"Cubo", "Nave", "UFO", "Bola", "Wave", "Robot", "Arana", "Swing"};
    constexpr char const* VEH_KEYS[VEH_COUNT] = {
        "tick-cube", "tick-ship", "tick-ufo", "tick-ball", "tick-wave", "tick-robot", "tick-spider", "tick-swing"};

    // Escalon de ticks: los mismos valores del ciclo F6 original (2 4 8 16 24 32 48 54 60), mas 0 = sin buffer.
    constexpr int STEPS[] = {0, 2, 4, 8, 16, 24, 32, 48, 54, 60};
    constexpr int STEP_COUNT = sizeof(STEPS) / sizeof(STEPS[0]);

    struct State {
        bool ok = false;
        int  idx = 0;
        bool active = false;
        int  all = 0, wave = 0;
        int  last = -1;
        bool press = true;
    };

    static uint8_t* base() {
        HMODULE h = GetModuleHandleA("ai.predictivebuffer.dll");
        if (!h) return nullptr;
        auto b = reinterpret_cast<uint8_t*>(h);
        auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(b);
        auto nt  = reinterpret_cast<IMAGE_NT_HEADERS64*>(b + dos->e_lfanew);
        if (nt->OptionalHeader.SizeOfImage != IMAGE_SIZE) return nullptr;
        return b;
    }

    static State read() {
        State s;
        auto b = base();
        if (!b) return s;
        s.idx    = *reinterpret_cast<volatile uint32_t*>(b + RVA_IDX);
        s.active = *reinterpret_cast<volatile uint8_t*>(b + RVA_FLAG) != 0;
        s.all    = *reinterpret_cast<volatile uint32_t*>(b + RVA_ALL);
        s.wave   = *reinterpret_cast<volatile uint32_t*>(b + RVA_WAVE);
        s.last   = *reinterpret_cast<volatile int32_t*>(b + RVA_LAST);
        s.press  = *reinterpret_cast<volatile uint8_t*>(b + RVA_PRESS) != 0;
        if (s.idx < 0 || s.idx > OFF_IDX) return s;
        s.ok = true;
        return s;
    }

    static void writeIdx(uint32_t idx) {
        if (auto b = base()) *reinterpret_cast<volatile uint32_t*>(b + RVA_IDX) = idx;
    }

    static void writeLimits(uint32_t all, uint32_t wave, bool flag) {
        auto b = base();
        if (!b) return;
        *reinterpret_cast<volatile uint32_t*>(b + RVA_ALL) = all;
        *reinterpret_cast<volatile uint32_t*>(b + RVA_WAVE) = wave;
        *reinterpret_cast<volatile uint8_t*>(b + RVA_FLAG) = flag ? 1 : 0;
    }

    // ---- ticks por vehiculo (se guardan en los datos guardados del mod) ----
    static inline int g_ticks[VEH_COUNT] = {4, 4, 4, 4, 4, 4, 4, 4};

    static int snap(int v) {          // al escalon valido mas cercano por debajo
        int r = 0;
        for (int i = 0; i < STEP_COUNT; i++) if (STEPS[i] <= v) r = STEPS[i];
        return r;
    }

    static void loadTicks() {
        for (int i = 0; i < VEH_COUNT; i++)
            g_ticks[i] = snap(Mod::get()->getSavedValue<int>(VEH_KEYS[i], 4));
    }

    static int ticksFor(int veh) { return g_ticks[std::clamp(veh, 0, VEH_COUNT - 1)]; }

    // dir = +1 sube (2 -> 4 -> 8 -> 16 ... 60), dir = -1 baja
    static void stepTicks(int veh, int dir) {
        int cur = ticksFor(veh), idx = 0;
        for (int i = 0; i < STEP_COUNT; i++) if (STEPS[i] <= cur) idx = i;
        idx = std::clamp(idx + dir, 0, STEP_COUNT - 1);
        g_ticks[veh] = STEPS[idx];
        Mod::get()->setSavedValue<int>(VEH_KEYS[veh], STEPS[idx]);
    }

    // vehiculo del jugador 1: 0 cubo, 1 nave, 2 ufo, 3 bola, 4 wave, 5 robot, 6 arana, 7 swing; -1 = fuera de nivel
    static int currentVehicle() {
        auto pl = PlayLayer::get();
        if (!pl || !pl->m_player1) return -1;
        auto p = pl->m_player1;
        if (p->m_isShip)   return 1;
        if (p->m_isBird)   return 2;
        if (p->m_isBall)   return 3;
        if (p->m_isDart)   return 4;
        if (p->m_isRobot)  return 5;
        if (p->m_isSpider) return 6;
        if (p->m_isSwing)  return 7;
        return 0;
    }
}

// ---------------------------------------------------------------------------
// Estilo Mega Hack: ventanas con barra de titulo de color, cuerpo gris, texto blanco.
// ---------------------------------------------------------------------------
namespace ui {
    constexpr float TITLE_H = 12.f;
    constexpr float ROW_H   = 11.f;
    constexpr float FONT_S  = 0.40f;
    constexpr char const* FONT = "chatFont.fnt";

    constexpr ccColor3B DEF_ACCENT = {36, 124, 255};   // azul por defecto (antes rojo en Mega Hack)
    constexpr ccColor3B DEF_GRAY   = {72, 72, 72};     // gris por defecto
    constexpr float SCALES[] = {0.75f, 1.f, 1.25f, 1.5f, 2.f};
    constexpr int SCALE_COUNT = 5;

    constexpr float PW = 104.f;      // ancho de la ventana de la rueda
    constexpr float PR = 38.f;       // radio de la rueda
    constexpr float PCX = 52.f;      // centro X de la rueda
    constexpr float PSX = 8.f;       // inicio X del deslizador de brillo
    constexpr float PSL = 88.f;      // largo del deslizador
    constexpr float PEXTRA = 4.f + 2.f * PR + 6.f + 8.f + 5.f; // alto extra bajo las filas

    static GLubyte toByte(float v) { return static_cast<GLubyte>(std::clamp(v, 0.f, 255.f) + 0.5f); }

    static ccColor3B hsv2rgb(float h, float s, float v) {
        h = std::fmod(std::fmod(h, 360.f) + 360.f, 360.f);
        float c = v * s, hp = h / 60.f;
        float x = c * (1.f - std::abs(std::fmod(hp, 2.f) - 1.f));
        float r = 0, g = 0, b = 0;
        if (hp < 1) { r = c; g = x; }
        else if (hp < 2) { r = x; g = c; }
        else if (hp < 3) { g = c; b = x; }
        else if (hp < 4) { g = x; b = c; }
        else if (hp < 5) { r = x; b = c; }
        else { r = c; b = x; }
        float m = v - c;
        return ccc3(toByte((r + m) * 255.f), toByte((g + m) * 255.f), toByte((b + m) * 255.f));
    }

    static void rgb2hsv(ccColor3B c, float& h, float& s, float& v) {
        float r = c.r / 255.f, g = c.g / 255.f, b = c.b / 255.f;
        float mx = std::max({r, g, b}), mn = std::min({r, g, b}), d = mx - mn;
        v = mx;
        s = mx <= 0.f ? 0.f : d / mx;
        if (d <= 0.f) h = 0.f;
        else if (mx == r) h = 60.f * std::fmod((g - b) / d, 6.f);
        else if (mx == g) h = 60.f * ((b - r) / d + 2.f);
        else h = 60.f * ((r - g) / d + 4.f);
        if (h < 0.f) h += 360.f;
    }

    constexpr float ANIM_T = 0.14f;  // duracion de la animacion de plegar / desplegar (segundos)

    static ccColor4F c4f(ccColor3B c, float a = 1.f) { return ccc4f(c.r / 255.f, c.g / 255.f, c.b / 255.f, a); }
    static ccColor3B shade(ccColor3B c, float f) { return ccc3(toByte(c.r * f), toByte(c.g * f), toByte(c.b * f)); }
    static ccColor3B mix(ccColor3B a, ccColor3B b, float t) {
        return ccc3(toByte(a.r + (b.r - a.r) * t), toByte(a.g + (b.g - a.g) * t), toByte(a.b + (b.b - a.b) * t));
    }
    static ccColor3B lighten(ccColor3B c, float t) {
        return ccc3(toByte(c.r + (255 - c.r) * t), toByte(c.g + (255 - c.g) * t), toByte(c.b + (255 - c.b) * t));
    }

    // Rectangulo con esquinas redondeadas. (x, yTop) = esquina superior izquierda; y crece hacia ARRIBA, asi que baja con -h.
    static void rrect(CCDrawNode* d, float x, float yTop, float w, float h, float r, ccColor4F col,
                      bool tl = true, bool tr = true, bool br = true, bool bl = true) {
        if (!d || w <= 0.f || h <= 0.f) return;
        r = std::min(r, std::min(w, h) / 2.f);
        const int N = 4;
        const float k = 3.14159265f / 180.f;
        std::vector<CCPoint> p;
        auto arc = [&](float cx, float cy, float a0, float a1, bool round, float px, float py) {
            if (!round || r <= 0.01f) { p.push_back(ccp(px, py)); return; }
            for (int i = 0; i <= N; i++) {
                float a = (a0 + (a1 - a0) * i / N) * k;
                p.push_back(ccp(cx + std::cos(a) * r, cy + std::sin(a) * r));
            }
        };
        arc(x + r, yTop - r, 180.f, 90.f, tl, x, yTop);
        arc(x + w - r, yTop - r, 90.f, 0.f, tr, x + w, yTop);
        arc(x + w - r, yTop - h + r, 0.f, -90.f, br, x + w, yTop - h);
        arc(x + r, yTop - h + r, -90.f, -180.f, bl, x, yTop - h);
        d->drawPolygon(p.data(), static_cast<unsigned int>(p.size()), col, 0.f, ccc4f(0.f, 0.f, 0.f, 0.f));
    }

    static int pack(ccColor3B c) { return (c.r << 16) | (c.g << 8) | c.b; }
    static ccColor3B unpack(int v) {
        return ccc3(static_cast<GLubyte>((v >> 16) & 255), static_cast<GLubyte>((v >> 8) & 255), static_cast<GLubyte>(v & 255));
    }
}

static std::string ticksInfo(int t) {
    return fmt::format("{} ticks = {:.2f} f ({:.1f} ms)", t, t / buf::TICKS_PER_FRAME, t * buf::MS_PER_TICK);
}

class BufferHud : public CCNode, public CCTouchDelegate {
protected:
    static inline BufferHud* s_instance = nullptr;
    static inline bool s_ready = false;

    enum class Kind { Toggle, Text, Stepper, Color, Button };

    struct Row {
        Kind kind = Kind::Text;
        std::string label;
        std::string labelEs;                 // etiqueta en espanol
        std::function<std::string()> text;   // Text: contenido dinamico
        std::function<std::string()> value;  // Stepper: numero mostrado
        std::function<bool()> on;            // Toggle: activo / Stepper: fila "actual"
        std::function<void()> click;         // Toggle, Button, Color
        std::function<void(int)> step;       // Stepper: +1 / -1
        std::function<ccColor3B()> color;    // Color: muestra
        CCLabelBMFont* lbl = nullptr;
        CCLabelBMFont* val = nullptr;
    };

    struct Win {
        std::string title;
        std::string titleEs;                 // titulo en espanol
        std::string key;
        float w = 110.f;
        float x = 6.f, y = 6.f;              // esquina superior izquierda (y hacia abajo)
        bool collapsed = false;
        bool hidden = false;                 // ventana de la rueda cuando esta cerrada
        bool picker = false;                 // ventana con la rueda de colores
        float extraH = 0.f;                  // alto extra bajo las filas (rueda)
        std::vector<Row> rows;
        CCNode* node = nullptr;
        CCNode* rowsNode = nullptr;
        CCDrawNode* frame = nullptr;         // sombra, borde, barra de titulo y cuerpo (esquinas redondeadas)
        CCDrawNode* rowGfx = nullptr;        // resaltados, separadores y muestras de color de las filas
        CCLabelBMFont* titleLbl = nullptr;
        float anim = 0.f;                    // 0 = plegada, 1 = desplegada (animacion)
        uint64_t sigRows = 0, sigFrame = 0;  // para redibujar solo cuando cambia algo
        CCDrawNode* slider = nullptr;
        CCDrawNode* marks = nullptr;
        float height() const { return ui::TITLE_H + (collapsed ? 0.f : rows.size() * ui::ROW_H + extraH); }
    };

    struct Ev { int ticks; bool press; float t; };

    std::vector<Win> m_wins;
    ccColor3B m_accent = ui::DEF_ACCENT;
    ccColor3B m_gray = ui::DEF_GRAY;
    int   m_scaleIdx = 1;
    int   m_editing = 0;                     // 0 = principal, 1 = gris
    int   m_pickIdx = -1;                    // indice de la ventana de la rueda
    int   m_pickWin = -1, m_pickMode = 0;    // arrastre en la rueda: 1 = rueda, 2 = brillo
    float m_pH = 0.f, m_pS = 0.f, m_pV = 1.f;
    int   m_topZ = 1;

    int   m_drag = -1;
    bool  m_dragMoved = false;
    CCPoint m_dragTouch0, m_dragWin0;

    bool  m_shown = true;
    bool  m_ctrlWasDown = false;
    float m_time = 0.f;
    float m_lastEventTime = -100.f;
    int   m_prevLast = -1;
    int   m_prevIdx = 0;
    std::deque<Ev> m_hist;
    int   m_total = 0, m_sum = 0, m_max = 0;
    buf::State m_s;

    static bool gdFocused() {
        HWND fg = GetForegroundWindow();
        if (!fg) return false;
        DWORD pid = 0;
        GetWindowThreadProcessId(fg, &pid);
        return pid == GetCurrentProcessId();
    }

    static void setText(CCLabelBMFont* l, std::string const& s) {
        if (l && s != l->getString()) l->setString(s.c_str());
    }

    static void tri(CCDrawNode* d, float cx, float cy, bool up) {
        float s = 3.2f;
        CCPoint v[3];
        if (up) { v[0] = ccp(cx - s, cy - s * 0.6f); v[1] = ccp(cx + s, cy - s * 0.6f); v[2] = ccp(cx, cy + s * 0.8f); }
        else    { v[0] = ccp(cx - s, cy + s * 0.6f); v[1] = ccp(cx + s, cy + s * 0.6f); v[2] = ccp(cx, cy - s * 0.8f); }
        d->drawPolygon(v, 3, ccc4f(1.f, 1.f, 1.f, 1.f), 0.f, ccc4f(0.f, 0.f, 0.f, 0.f));
    }

public:
    static BufferHud* create() {
        auto r = new BufferHud();
        if (r && r->init()) { r->autorelease(); return r; }
        CC_SAFE_DELETE(r);
        return nullptr;
    }

    static void markReady() { s_ready = true; }

    // Una vez por frame desde CCScheduler::update (funciona en cualquier pantalla).
    static void onFrame(float dt) {
        if (!s_ready) return;
        auto scene = CCDirector::get()->getRunningScene();
        if (!scene) return;
        if (!s_instance) {
            auto hud = create();
            if (!hud) return;
            hud->retain();
            hud->setID("buffer-hud");
            s_instance = hud;
            // prioridad -400: los popups (-500) reciben el click antes que el HUD
            CCDirector::get()->getTouchDispatcher()->addTargetedDelegate(hud, -400, true);
        }
        if (s_instance->getParent() != scene) {
            s_instance->removeFromParentAndCleanup(false);
            scene->addChild(s_instance, 100); // por debajo de los popups (z 105)
        }
        s_instance->tick(dt);
    }

    // ---------------- colores ----------------
    void setAccent(ccColor3B c) {
        m_accent = c;
        Mod::get()->setSavedValue<int>("ui-accent", ui::pack(c));
    }
    void setGray(ccColor3B c) {
        m_gray = c;
        Mod::get()->setSavedValue<int>("ui-gray", ui::pack(c));
    }
    void commitPick() {
        auto c = ui::hsv2rgb(m_pH, m_pS, m_pV);
        if (m_editing == 0) setAccent(c); else setGray(c);
        redrawPicker();
    }

    void openPicker(int which) {
        if (m_pickIdx < 0) return;
        auto& w = m_wins[m_pickIdx];
        if (!w.hidden && m_editing == which) {      // segundo click en la misma fila: cerrar
            closePicker();
            return;
        }
        m_editing = which;
        ui::rgb2hsv(which == 0 ? m_accent : m_gray, m_pH, m_pS, m_pV);
        w.hidden = false;
        w.node->setVisible(true);          // la animacion la va desplegando
        w.node->setZOrder(++m_topZ);
        redrawPicker();
    }

    void closePicker() {
        if (m_pickIdx < 0) return;
        auto& w = m_wins[m_pickIdx];
        w.hidden = true;                   // la animacion la pliega y luego se oculta
    }

    // dibuja el deslizador de brillo y las marcas (se llama al abrir y al cambiar el color)
    void redrawPicker() {
        if (m_pickIdx < 0) return;
        auto& w = m_wins[m_pickIdx];
        float y0 = ui::TITLE_H + w.rows.size() * ui::ROW_H;
        float cyN = -(y0 + 4.f + ui::PR);                         // centro de la rueda (coords de la ventana)
        float syN = -(y0 + 4.f + 2.f * ui::PR + 6.f + 8.f);       // borde inferior del deslizador

        w.slider->clear();
        ui::rrect(w.slider, -1.4f, 9.4f, ui::PSL + 2.8f, 10.8f, 3.f, ccc4f(0.92f, 0.92f, 0.92f, 1.f));
        const int N = 16;
        float seg = ui::PSL / N;
        for (int i = 0; i < N; i++) {
            auto c0 = ui::hsv2rgb(m_pH, m_pS, float(i) / N);
            CCPoint v[4] = {ccp(i * seg, 0.f), ccp((i + 1) * seg + 0.5f, 0.f), ccp((i + 1) * seg + 0.5f, 8.f), ccp(i * seg, 8.f)};
            w.slider->drawPolygon(v, 4, ccc4f(c0.r / 255.f, c0.g / 255.f, c0.b / 255.f, 1.f), 0.f, ccc4f(0.f, 0.f, 0.f, 0.f));
        }

        w.marks->clear();
        float rad = m_pH * 3.14159265f / 180.f;
        CCPoint mp = ccp(ui::PCX + std::cos(rad) * m_pS * ui::PR, cyN + std::sin(rad) * m_pS * ui::PR);
        w.marks->drawDot(mp, 3.6f, ccc4f(0.f, 0.f, 0.f, 1.f));
        w.marks->drawDot(mp, 2.4f, ccc4f(1.f, 1.f, 1.f, 1.f));
        float sx = ui::PSX + m_pV * ui::PSL;
        CCPoint b1[4] = {ccp(sx - 2.f, syN - 1.5f), ccp(sx + 2.f, syN - 1.5f), ccp(sx + 2.f, syN + 9.5f), ccp(sx - 2.f, syN + 9.5f)};
        CCPoint b2[4] = {ccp(sx - 1.f, syN - 0.5f), ccp(sx + 1.f, syN - 0.5f), ccp(sx + 1.f, syN + 8.5f), ccp(sx - 1.f, syN + 8.5f)};
        w.marks->drawPolygon(b1, 4, ccc4f(0.f, 0.f, 0.f, 1.f), 0.f, ccc4f(0.f, 0.f, 0.f, 0.f));
        w.marks->drawPolygon(b2, 4, ccc4f(1.f, 1.f, 1.f, 1.f), 0.f, ccc4f(0.f, 0.f, 0.f, 0.f));
    }

    // lx, d = posicion del click dentro de la ventana (d = distancia hacia abajo desde el borde superior)
    void applyPick(Win& w, float lx, float d) {
        float y0 = ui::TITLE_H + w.rows.size() * ui::ROW_H;
        if (m_pickMode == 1) {
            float dx = lx - ui::PCX;
            float dy = (y0 + 4.f + ui::PR) - d;
            float r = std::sqrt(dx * dx + dy * dy);
            m_pS = std::min(r / ui::PR, 1.f);
            if (r > 0.01f) {
                m_pH = std::atan2(dy, dx) * 180.f / 3.14159265f;
                if (m_pH < 0.f) m_pH += 360.f;
            }
        } else if (m_pickMode == 2) {
            m_pV = std::clamp((lx - ui::PSX) / ui::PSL, 0.f, 1.f);
        }
        commitPick();
    }

    // ---------------- construccion ----------------
    bool init() override {
        if (!CCNode::init()) return false;
        lang::load();
        m_shown = Mod::get()->getSavedValue<bool>("hud-visible", true);
        m_accent = ui::unpack(Mod::get()->getSavedValue<int>("ui-accent", ui::pack(ui::DEF_ACCENT)));
        m_gray = ui::unpack(Mod::get()->getSavedValue<int>("ui-gray", ui::pack(ui::DEF_GRAY)));
        m_scaleIdx = std::clamp(Mod::get()->getSavedValue<int>("ui-scale", 1), 0, ui::SCALE_COUNT - 1);
        buf::loadTicks();
        auto s = buf::read();
        m_prevLast = s.last;
        m_prevIdx = s.ok ? s.idx : 0;
        buildWindows();
        this->setVisible(m_shown);
        return true;
    }

    void buildWindows() {
        // ---- Window 1: Buffer ----
        {
            Win w; w.title = "Buffer"; w.titleEs = "Buffer"; w.key = "buffer"; w.w = 124.f; w.x = 6.f;
            Row t; t.kind = Kind::Toggle; t.label = "Buffer active"; t.labelEs = "Buffer activo";
            t.on = [this] { return m_s.ok && m_s.idx != buf::OFF_IDX; };
            t.click = [this] {
                if (!m_s.ok) return;
                int n = (m_s.idx == buf::OFF_IDX) ? 0 : buf::OFF_IDX;
                buf::writeIdx(n);
                m_s.idx = n;
                m_prevIdx = n;
            };
            w.rows.push_back(t);
            Row r1; r1.text = [this] {
                if (!m_s.ok) return std::string(lang::tr("NO DATA (different DLL)", "SIN DATOS (DLL distinta)"));
                int v = buf::currentVehicle();
                return std::string(lang::tr("Vehicle: ", "Vehiculo: ")) +
                       (v < 0 ? "-" : (lang::es ? buf::VEH_NAMES_ES[v] : buf::VEH_NAMES[v]));
            };
            w.rows.push_back(r1);
            Row r2; r2.text = [this] {
                int v = buf::currentVehicle();
                if (v < 0) return std::string(lang::tr("Enter a level", "Entra a un nivel"));
                bool on = m_s.ok && m_s.idx != buf::OFF_IDX;
                return on ? ticksInfo(buf::ticksFor(v)) : std::string(lang::tr("Buffer disabled", "Buffer desactivado"));
            };
            w.rows.push_back(r2);
            Row r3; r3.text = [] {
                auto pl = PlayLayer::get();
                if (!pl || !pl->m_level) return std::string(lang::tr("Level: -", "Nivel: -"));
                std::string n = pl->m_level->m_levelName;
                if (n.size() > 16) n = n.substr(0, 15) + ".";
                return std::string(lang::tr("Level: ", "Nivel: ")) + n;
            };
            w.rows.push_back(r3);
            Row r4; r4.text = [] {
                auto pl = PlayLayer::get();
                if (!pl || !pl->m_level) return std::string(lang::tr("Progress: -", "Progreso: -"));
                return fmt::format("{}%  {}", static_cast<int>(pl->getCurrentPercent()),
                                   pl->m_isPracticeMode ? lang::tr("Practice", "Practica") : "Normal");
            };
            w.rows.push_back(r4);
            m_wins.push_back(std::move(w));
        }
        // ---- Window 2: Ticks per vehicle ----
        {
            Win w; w.title = "Ticks"; w.titleEs = "Ticks"; w.key = "ticks"; w.w = 96.f; w.x = 136.f;
            for (int i = 0; i < buf::VEH_COUNT; i++) {
                Row r; r.kind = Kind::Stepper; r.label = buf::VEH_NAMES[i]; r.labelEs = buf::VEH_NAMES_ES[i];
                r.value = [i] { return std::to_string(buf::ticksFor(i)); };
                r.on = [i] { return buf::currentVehicle() == i; };
                r.step = [i](int d) { buf::stepTicks(i, d); };
                w.rows.push_back(r);
            }
            m_wins.push_back(std::move(w));
        }
        // ---- Window 3: Buffered click ----
        {
            Win w; w.title = "Buffered click"; w.titleEs = "Click bufferizado"; w.key = "clicks"; w.w = 138.f; w.x = 238.f;
            Row a; a.text = [this] {
                if (m_hist.empty()) return std::string(lang::tr("Waiting for click...", "Esperando click..."));
                auto& e = m_hist.front();
                return fmt::format("{}  +{} ticks", e.press ? "PRESS" : "RELEASE", e.ticks);
            };
            w.rows.push_back(a);
            Row b; b.text = [this] {
                if (m_hist.empty()) return std::string("-");
                auto& e = m_hist.front();
                return lang::es
                    ? fmt::format("{:.2f} f ({:.1f} ms) hace {:.1f}s", e.ticks / buf::TICKS_PER_FRAME,
                                  e.ticks * buf::MS_PER_TICK, m_time - m_lastEventTime)
                    : fmt::format("{:.2f} f ({:.1f} ms) {:.1f}s ago", e.ticks / buf::TICKS_PER_FRAME,
                                  e.ticks * buf::MS_PER_TICK, m_time - m_lastEventTime);
            };
            w.rows.push_back(b);
            Row c; c.text = [this] {
                if (m_total == 0) return std::string("Total: 0");
                return lang::es
                    ? fmt::format("Total {}  Media {:.1f}  Max {}", m_total, float(m_sum) / m_total, m_max)
                    : fmt::format("Total {}  Avg {:.1f}  Max {}", m_total, float(m_sum) / m_total, m_max);
            };
            w.rows.push_back(c);
            for (int i = 0; i < 4; i++) {
                Row h; h.text = [this, i] {
                    if (i >= static_cast<int>(m_hist.size())) return std::string("-");
                    auto& e = m_hist[i];
                    return lang::es
                        ? fmt::format("{:<7} +{:>2}t  hace {:.0f}s", e.press ? "PRESS" : "RELEASE", e.ticks, m_time - e.t)
                        : fmt::format("{:<7} +{:>2}t  {:.0f}s ago", e.press ? "PRESS" : "RELEASE", e.ticks, m_time - e.t);
                };
                w.rows.push_back(h);
            }
            m_wins.push_back(std::move(w));
        }
        // ---- Window 4: Interface (colors, scale, language) ----
        {
            Win w; w.title = "Interface"; w.titleEs = "Interfaz"; w.key = "ui"; w.w = 104.f; w.x = 382.f;
            Row c1; c1.kind = Kind::Color; c1.label = "Main color"; c1.labelEs = "Color principal";
            c1.color = [this] { return m_accent; };
            c1.click = [this] { openPicker(0); };
            w.rows.push_back(c1);
            Row c2; c2.kind = Kind::Color; c2.label = "Gray color"; c2.labelEs = "Color gris";
            c2.color = [this] { return m_gray; };
            c2.click = [this] { openPicker(1); };
            w.rows.push_back(c2);
            Row rs; rs.kind = Kind::Button; rs.label = "Reset colors"; rs.labelEs = "Restablecer colores";
            rs.click = [this] { setAccent(ui::DEF_ACCENT); setGray(ui::DEF_GRAY); };
            w.rows.push_back(rs);
            Row sc; sc.kind = Kind::Stepper; sc.label = "Scale"; sc.labelEs = "Escala";
            sc.value = [this] { return fmt::format("{:g}x", ui::SCALES[m_scaleIdx]); };
            sc.step = [this](int d) {
                m_scaleIdx = std::clamp(m_scaleIdx + d, 0, ui::SCALE_COUNT - 1);
                Mod::get()->setSavedValue<int>("ui-scale", m_scaleIdx);
            };
            w.rows.push_back(sc);
            Row lg; lg.kind = Kind::Button; lg.label = "Language: English"; lg.labelEs = "Idioma: Espanol";
            lg.click = [] { lang::toggle(); };
            w.rows.push_back(lg);
            Row hint; hint.text = [] { return std::string(lang::tr("L-Ctrl: hide menu", "L-Ctrl: ocultar menu")); };
            w.rows.push_back(hint);
            m_wins.push_back(std::move(w));
        }
        // ---- Window 5: color wheel (hidden until a color is clicked) ----
        {
            Win w; w.title = "Color wheel"; w.titleEs = "Rueda de color"; w.key = "picker"; w.w = ui::PW; w.x = 382.f; w.y = 80.f;
            w.picker = true; w.hidden = true; w.extraH = ui::PEXTRA;
            Row e; e.text = [this] {
                return std::string(lang::tr("Editing: ", "Editando: ")) +
                       (m_editing == 0 ? lang::tr("main", "principal") : lang::tr("gray", "gris"));
            };
            w.rows.push_back(e);
            Row c; c.kind = Kind::Button; c.label = "Close"; c.labelEs = "Cerrar";
            c.click = [this] { closePicker(); };
            w.rows.push_back(c);
            m_pickIdx = static_cast<int>(m_wins.size());
            m_wins.push_back(std::move(w));
        }

        for (int i = 0; i < static_cast<int>(m_wins.size()); i++) buildNodes(m_wins[i], i);
    }

    CCLabelBMFont* makeLabel(CCNode* parent, std::string const& txt, float x, float y, CCPoint anchor, float scale = ui::FONT_S) {
        auto l = CCLabelBMFont::create(txt.c_str(), ui::FONT);
        l->setScale(scale);
        l->setColor({255, 255, 255});
        l->setAnchorPoint(anchor);
        l->setPosition({x, y});
        parent->addChild(l, 3);
        return l;
    }

    void buildNodes(Win& w, int wi) {
        // posicion/estado guardados
        w.x = static_cast<float>(Mod::get()->getSavedValue<int>("wx-" + w.key, static_cast<int>(w.x)));
        w.y = static_cast<float>(Mod::get()->getSavedValue<int>("wy-" + w.key, static_cast<int>(w.y)));
        w.collapsed = Mod::get()->getSavedValue<bool>("wc-" + w.key, false);

        w.node = CCNode::create();
        w.node->setPosition(ccp(w.x, -w.y));
        this->addChild(w.node, wi);

        w.frame = CCDrawNode::create();
        w.node->addChild(w.frame, 0);

        w.titleLbl = makeLabel(w.node, w.title, 5.f, -ui::TITLE_H / 2, {0.f, 0.5f}, 0.42f);

        w.rowsNode = CCNode::create();
        w.node->addChild(w.rowsNode, 2);
        w.rowGfx = CCDrawNode::create();
        w.rowsNode->addChild(w.rowGfx, 0);

        for (size_t i = 0; i < w.rows.size(); i++) {
            auto& r = w.rows[i];
            float top = -(ui::TITLE_H + i * ui::ROW_H);
            float mid = top - ui::ROW_H / 2;
            switch (r.kind) {
                case Kind::Stepper: {
                    auto d = CCDrawNode::create();
                    tri(d, 7.f, mid, true);
                    tri(d, 18.f, mid, false);
                    w.rowsNode->addChild(d, 2);
                    r.val = makeLabel(w.rowsNode, "", 28.f, mid, {0.f, 0.5f});
                    r.lbl = makeLabel(w.rowsNode, r.label, 52.f, mid, {0.f, 0.5f});
                    break;
                }
                default:
                    r.lbl = makeLabel(w.rowsNode, r.label, 5.f, mid, {0.f, 0.5f});
                    break;
            }
        }
        if (w.picker) {
            float y0 = ui::TITLE_H + w.rows.size() * ui::ROW_H;
            // rueda de matiz (angulo) y saturacion (radio), a brillo maximo, con un aro claro de borde
            auto wheel = CCDrawNode::create();
            wheel->setPosition(ccp(ui::PCX, -(y0 + 4.f + ui::PR)));
            wheel->drawDot(ccp(0.f, 0.f), ui::PR + 1.8f, ccc4f(0.92f, 0.92f, 0.92f, 1.f));
            const int SEG = 48, RINGS = 8;
            const float kPi = 3.14159265f;
            for (int ring = 0; ring < RINGS; ring++) {
                float r0 = ui::PR * ring / RINGS, r1 = ui::PR * (ring + 1) / RINGS;
                float sat = (ring + 0.5f) / RINGS;
                for (int sg = 0; sg < SEG; sg++) {
                    float a0 = 360.f * sg / SEG, a1 = 360.f * (sg + 1) / SEG;
                    auto c = ui::hsv2rgb((a0 + a1) / 2.f, sat, 1.f);
                    float t0 = a0 * kPi / 180.f, t1 = (a1 + 0.6f) * kPi / 180.f;   // pequeno solape: sin rendijas
                    CCPoint v[4] = {ccp(std::cos(t0) * r0, std::sin(t0) * r0), ccp(std::cos(t0) * r1, std::sin(t0) * r1),
                                    ccp(std::cos(t1) * r1, std::sin(t1) * r1), ccp(std::cos(t1) * r0, std::sin(t1) * r0)};
                    wheel->drawPolygon(v, 4, ui::c4f(c, 1.f), 0.f, ccc4f(0.f, 0.f, 0.f, 0.f));
                }
            }
            w.rowsNode->addChild(wheel, 1);

            w.slider = CCDrawNode::create();
            w.slider->setPosition(ccp(ui::PSX, -(y0 + 4.f + 2.f * ui::PR + 6.f + 8.f)));
            w.rowsNode->addChild(w.slider, 1);

            w.marks = CCDrawNode::create();
            w.rowsNode->addChild(w.marks, 2);
        }
        w.anim = 0.f;                         // al arrancar todas las ventanas se despliegan con la animacion
        w.node->setVisible(!w.hidden);
    }

    // sombra suave, borde del color principal, barra de titulo con brillo y cuerpo gris, todo con esquinas redondeadas
    void drawFrame(Win& w, float H, bool open) {
        auto d = w.frame;
        d->clear();
        float W = w.w;
        for (int k = 3; k >= 1; k--) {
            float sp = k * 0.9f;
            ui::rrect(d, -sp, sp - 1.0f, W + 2.f * sp, H + 2.f * sp, 4.f + sp, ccc4f(0.f, 0.f, 0.f, 0.10f));
        }
        ui::rrect(d, 0.f, 0.f, W, H, 4.f, ui::c4f(ui::lighten(m_accent, 0.30f), 1.f));           // borde
        ui::rrect(d, 1.f, -1.f, W - 2.f, H - 2.f, 3.2f, ui::c4f(m_gray, 0.94f));                  // cuerpo
        bool closed = H <= ui::TITLE_H + 0.5f;
        ui::rrect(d, 1.f, -1.f, W - 2.f, ui::TITLE_H - 1.f, 3.2f, ui::c4f(m_accent, 1.f), true, true, closed, closed);
        ui::rrect(d, 1.f, -1.f, W - 2.f, (ui::TITLE_H - 1.f) * 0.5f, 3.2f, ui::c4f(ui::lighten(m_accent, 0.22f), 1.f), true, true, false, false);
        if (!closed) ui::rrect(d, 1.f, -(ui::TITLE_H - 0.6f), W - 2.f, 0.6f, 0.f, ui::c4f(ui::shade(m_accent, 0.55f), 1.f));
        // indicador de plegado: flecha abajo = desplegada, flecha izquierda = plegada
        float cx = W - 7.f, cy = -ui::TITLE_H / 2;
        CCPoint v[3];
        if (open) { v[0] = ccp(cx - 3.f, cy + 1.6f); v[1] = ccp(cx + 3.f, cy + 1.6f); v[2] = ccp(cx, cy - 2.f); }
        else      { v[0] = ccp(cx + 1.6f, cy + 3.f); v[1] = ccp(cx + 1.6f, cy - 3.f); v[2] = ccp(cx - 2.f, cy); }
        d->drawPolygon(v, 3, ccc4f(1.f, 1.f, 1.f, 1.f), 0.f, ccc4f(0.f, 0.f, 0.f, 0.f));
    }

    // resaltados redondeados de las filas activas, separadores finos y muestras de color
    void drawRows(Win& w) {
        auto d = w.rowGfx;
        d->clear();
        float W = w.w;
        for (size_t i = 0; i < w.rows.size(); i++) {
            auto& r = w.rows[i];
            float top = -(ui::TITLE_H + i * ui::ROW_H);
            float mid = top - ui::ROW_H / 2;
            if (i > 0) ui::rrect(d, 5.f, top + 0.3f, W - 10.f, 0.6f, 0.f, ui::c4f(ui::lighten(m_gray, 0.10f), 1.f));
            bool on = r.on && r.on();
            switch (r.kind) {
                case Kind::Toggle:
                    if (on) {
                        ui::rrect(d, 1.8f, top - 0.5f, W - 3.6f, ui::ROW_H - 1.f, 3.f, ui::c4f(ui::shade(m_accent, 0.7f), 1.f));
                        ui::rrect(d, 2.6f, top - 1.3f, W - 5.2f, ui::ROW_H - 2.6f, 2.4f, ui::c4f(m_accent, 1.f));
                        ui::rrect(d, 2.6f, top - 1.3f, W - 5.2f, (ui::ROW_H - 2.6f) * 0.5f, 2.4f, ui::c4f(ui::lighten(m_accent, 0.20f), 1.f), true, true, false, false);
                    }
                    break;
                case Kind::Stepper:
                    if (on) {
                        ui::rrect(d, 2.f, top - 1.f, W - 4.f, ui::ROW_H - 2.f, 2.6f, ui::c4f(ui::mix(m_gray, m_accent, 0.25f), 1.f));
                        ui::rrect(d, 2.4f, top - 2.4f, 2.2f, ui::ROW_H - 4.8f, 1.1f, ui::c4f(m_accent, 1.f));
                    }
                    break;
                case Kind::Color:
                    if (r.color) {
                        ui::rrect(d, W - 20.f, mid + 4.f, 14.f, 8.f, 2.4f, ccc4f(0.92f, 0.92f, 0.92f, 1.f));
                        ui::rrect(d, W - 19.f, mid + 3.f, 12.f, 6.f, 1.7f, ui::c4f(r.color(), 1.f));
                    }
                    break;
                case Kind::Button:
                    ui::rrect(d, 3.f, top - 1.2f, W - 6.f, ui::ROW_H - 2.4f, 2.6f, ui::c4f(ui::lighten(m_gray, 0.10f), 1.f));
                    break;
                default: break;
            }
        }
    }

    // animacion muy corta de plegar / desplegar: la altura va de la barra de titulo al total (suavizada)
    void animateWindows(float dt) {
        for (auto& w : m_wins) {
            bool open = !(w.collapsed || w.hidden);
            float target = open ? 1.f : 0.f;
            if (w.anim < target) w.anim = std::min(target, w.anim + dt / ui::ANIM_T);
            else if (w.anim > target) w.anim = std::max(target, w.anim - dt / ui::ANIM_T);
            float e = w.anim * w.anim * (3.f - 2.f * w.anim);
            float contentH = w.rows.size() * ui::ROW_H + w.extraH;
            float H = ui::TITLE_H + e * contentH;

            float sy = std::max(e, 0.003f);
            w.rowsNode->setVisible(e > 0.003f);
            w.rowsNode->setScaleY(sy);
            w.rowsNode->setPosition(ccp(0.f, -ui::TITLE_H * (1.f - sy)));   // escala anclada bajo la barra de titulo
            w.node->setVisible(!(w.hidden && w.anim <= 0.003f));

            uint64_t sig = 1469598103934665603ull;
            auto mixs = [&](uint64_t v) { sig = (sig ^ v) * 1099511628211ull; };
            mixs(ui::pack(m_accent)); mixs(ui::pack(m_gray)); mixs(static_cast<uint64_t>(H * 20.f)); mixs(open ? 1 : 0);
            if (sig != w.sigFrame) {
                w.sigFrame = sig;
                drawFrame(w, H, open);
            }
        }
    }

    // ---------------- eventos ----------------
    void pushEvent(int ticks, bool press) {
        m_hist.push_front({ticks, press, m_time});
        while (m_hist.size() > 4) m_hist.pop_back();
        m_total++;
        m_sum += ticks;
        m_max = std::max(m_max, ticks);
        m_lastEventTime = m_time;
    }

    // ---------------- raton ----------------
    bool ccTouchBegan(CCTouch* t, CCEvent*) override {
        if (!m_shown || !this->isVisible()) return false;
        m_pickMode = 0; m_pickWin = -1; m_drag = -1;
        auto p = this->convertToNodeSpace(t->getLocation());
        // de la ventana de mas arriba a la de mas abajo
        std::vector<int> order(m_wins.size());
        for (size_t i = 0; i < order.size(); i++) order[i] = static_cast<int>(i);
        std::sort(order.begin(), order.end(), [&](int a, int b) { return m_wins[a].node->getZOrder() > m_wins[b].node->getZOrder(); });
        for (int i : order) {
            auto& w = m_wins[i];
            if (w.hidden) continue;
            float lx = p.x - w.x;
            float d = -(p.y + w.y);          // distancia hacia abajo desde el borde superior
            if (lx < 0.f || lx > w.w || d < 0.f || d > w.height()) continue;

            w.node->setZOrder(++m_topZ);     // traer al frente
            if (d < ui::TITLE_H) {
                m_drag = i;
                m_dragMoved = false;
                m_dragTouch0 = p;
                m_dragWin0 = ccp(w.x, w.y);
                return true;
            }
            size_t ri = static_cast<size_t>((d - ui::TITLE_H) / ui::ROW_H);
            if (ri >= w.rows.size()) {
                if (w.picker) {                       // zona de la rueda / deslizador de brillo
                    float y0 = ui::TITLE_H + w.rows.size() * ui::ROW_H;
                    float cyD = y0 + 4.f + ui::PR;
                    float sy = y0 + 4.f + 2.f * ui::PR + 6.f;
                    float dx = lx - ui::PCX, dy = cyD - d;
                    if (std::sqrt(dx * dx + dy * dy) <= ui::PR + 3.f) m_pickMode = 1;
                    else if (d >= sy - 3.f && d <= sy + 11.f && lx >= ui::PSX - 4.f && lx <= ui::PSX + ui::PSL + 4.f) m_pickMode = 2;
                    if (m_pickMode != 0) {
                        m_pickWin = i;
                        applyPick(w, lx, d);
                    }
                }
                return true;
            }
            auto& r = w.rows[ri];
            switch (r.kind) {
                case Kind::Toggle:
                case Kind::Button:
                case Kind::Color:
                    if (r.click) r.click();
                    break;
                case Kind::Stepper:
                    if (r.step) {
                        if (lx < 12.f) r.step(+1);        // flecha arriba
                        else if (lx < 24.f) r.step(-1);   // flecha abajo
                    }
                    break;
                default: break;
            }
            return true;                     // el click no llega al juego
        }
        return false;
    }

    void ccTouchMoved(CCTouch* t, CCEvent*) override {
        if (m_pickMode != 0 && m_pickWin >= 0) {
            auto pp = this->convertToNodeSpace(t->getLocation());
            auto& pw = m_wins[m_pickWin];
            applyPick(pw, pp.x - pw.x, -(pp.y + pw.y));
            return;
        }
        if (m_drag < 0) return;
        auto p = this->convertToNodeSpace(t->getLocation());
        auto& w = m_wins[m_drag];
        float dx = p.x - m_dragTouch0.x, dy = p.y - m_dragTouch0.y;
        if (std::abs(dx) + std::abs(dy) > 2.f) m_dragMoved = true;
        if (!m_dragMoved) return;
        w.x = m_dragWin0.x + dx;
        w.y = m_dragWin0.y - dy;
        w.node->setPosition({w.x, -w.y});
    }

    void ccTouchEnded(CCTouch*, CCEvent*) override {
        if (m_pickMode != 0) { m_pickMode = 0; m_pickWin = -1; return; }
        if (m_drag < 0) return;
        auto& w = m_wins[m_drag];
        if (!m_dragMoved) {                  // click simple en la barra: plegar / desplegar
            w.collapsed = !w.collapsed;
            Mod::get()->setSavedValue<bool>("wc-" + w.key, w.collapsed);
        } else {
            auto win = CCDirector::get()->getWinSize();
            float sc = ui::SCALES[m_scaleIdx];
            w.x = std::clamp(w.x, 0.f, std::max(0.f, win.width / sc - w.w));
            w.y = std::clamp(w.y, 0.f, std::max(0.f, win.height / sc - ui::TITLE_H));
            w.node->setPosition({w.x, -w.y});
            Mod::get()->setSavedValue<int>("wx-" + w.key, static_cast<int>(w.x));
            Mod::get()->setSavedValue<int>("wy-" + w.key, static_cast<int>(w.y));
        }
        m_drag = -1;
    }

    void ccTouchCancelled(CCTouch*, CCEvent*) override { m_drag = -1; m_pickMode = 0; m_pickWin = -1; }

    // ---------------- actualizacion por frame ----------------
    void refresh() {
        auto win = CCDirector::get()->getWinSize();
        float sc = ui::SCALES[m_scaleIdx];
        this->setScale(sc);
        this->setPosition(ccp(0.f, win.height));

        for (auto& w : m_wins) {
            // idioma: actualiza titulo y etiquetas fijas
            setText(w.titleLbl, (lang::es && !w.titleEs.empty()) ? w.titleEs : w.title);
            for (auto& r : w.rows) {
                if (r.kind == Kind::Text || !r.lbl) continue;
                setText(r.lbl, (lang::es && !r.labelEs.empty()) ? r.labelEs : r.label);
            }
            uint64_t sig = 1469598103934665603ull;
            auto mixs = [&](uint64_t v) { sig = (sig ^ v) * 1099511628211ull; };
            mixs(ui::pack(m_accent)); mixs(ui::pack(m_gray));
            for (auto& r : w.rows) {
                switch (r.kind) {
                    case Kind::Toggle:
                        mixs(r.on && r.on() ? 1 : 0);
                        break;
                    case Kind::Text:
                        if (r.text) setText(r.lbl, r.text());
                        break;
                    case Kind::Stepper:
                        if (r.value) setText(r.val, r.value());
                        mixs(r.on && r.on() ? 1 : 0);
                        break;
                    case Kind::Color:
                        if (r.color) mixs(ui::pack(r.color()));
                        break;
                    default: break;
                }
            }
            if (sig != w.sigRows) {          // solo se redibuja cuando cambia el color o un estado
                w.sigRows = sig;
                drawRows(w);
            }
        }
    }

    void tick(float dt) {
        m_time += dt;

        // L-Ctrl: mostrar / ocultar todo el menu
        bool down = gdFocused() && (GetAsyncKeyState(VK_LCONTROL) & 0x8000);
        if (down && !m_ctrlWasDown) {
            m_shown = !m_shown;
            Mod::get()->setSavedValue("hud-visible", m_shown);
            this->setVisible(m_shown);
            if (m_shown) for (auto& w : m_wins) w.anim = 0.f;   // al mostrar el menu las ventanas se despliegan
        }
        m_ctrlWasDown = down;

        m_s = buf::read();
        if (m_s.ok) {
            // F6 (de la DLL) sigue funcionando: lo convertimos en ON/OFF (stage 1 <-> OFF)
            if (m_s.idx != m_prevIdx) {
                int n = (m_prevIdx == buf::OFF_IDX) ? 0 : buf::OFF_IDX;
                buf::writeIdx(n);
                m_s.idx = n;
                m_prevIdx = n;
            }

            // ticks segun el vehiculo actual
            bool on = m_s.idx != buf::OFF_IDX;
            int veh = buf::currentVehicle();
            int cur = veh < 0 ? 0 : veh;
            uint32_t waveT = on ? buf::ticksFor(buf::VEH_WAVE) : 0;
            uint32_t allT  = on ? buf::ticksFor(cur) : 0;
            uint32_t activeT = (cur == buf::VEH_WAVE) ? waveT : allT;
            buf::writeLimits(allT, waveT, on && activeT != 0);

            // detectar click bufferizado (mismo criterio que el log de la consola original)
            if (m_s.last != m_prevLast) {
                if (m_s.last > 0) pushEvent(m_s.last, m_s.press);
                m_prevLast = m_s.last;
            }
        }

        if (m_shown) { refresh(); animateWindows(dt); }
    }
};

// Cuando el menu principal ya cargo (fuentes y texturas listas), se activa el HUD.
class $modify(HudMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;
        BufferHud::markReady();
        return true;
    }
};

// Se ejecuta cada frame en cualquier pantalla: engancha el HUD a la escena activa.
class $modify(HudScheduler, CCScheduler) {
    void update(float dt) {
        CCScheduler::update(dt);
        BufferHud::onFrame(dt);
    }
};
