// Motor del Predictive Input Buffer (por AlexMera, base v4.6) integrado en Predictive Buffer HUD.
// Cambios respecto al original: sin consola, ON/OFF y limites de ticks controlados por el HUD via pbuf::g,
// horizonte ampliado para limites > 16 ticks (version "extended").
#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>
#include <Geode/modify/GameObject.hpp>
#include <Geode/modify/EffectGameObject.hpp>
#include <Geode/modify/HardStreak.hpp>
#include <Geode/modify/RingObject.hpp>
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "PredictiveBuffer.hpp"

using namespace geode::prelude;

// ============================================================================
//  USER SETTINGS
// ============================================================================

// True Wave Spam Guard: only bypasses the buffer if you do at least
// SPAM_STREAK_REQUIRED consecutive inputs faster than SPAM_INTERVAL_TICKS apart.
// This keeps isolated micro-clicks and spike/sawblade dodges 100% protected!
constexpr int SPAM_INTERVAL_TICKS = 8;
constexpr int SPAM_STREAK_REQUIRED = 3;

// ============================================================================


// ============================================================================
//  ORBS / PADS + MOVE TRIGGERS SUPPORT
// ============================================================================

static bool isOrbOrPad(GameObject* o) {
    if (!o) return false;
    switch (o->m_objectType) {
        case GameObjectType::YellowJumpPad:
        case GameObjectType::PinkJumpPad:
        case GameObjectType::RedJumpPad:
        case GameObjectType::GravityPad:
        case GameObjectType::YellowJumpRing:
        case GameObjectType::PinkJumpRing:
        case GameObjectType::RedJumpRing:
        case GameObjectType::GravityRing:
        case GameObjectType::GreenRing:
        case GameObjectType::DropRing:
        case GameObjectType::CustomRing:
        case GameObjectType::DashRing:
        case GameObjectType::GravityDashRing:
            return true;
        default:
            return false;
    }
}

static bool isSimCollidable(GameObject* o) {
    if (!o) return false;
    if (isOrbOrPad(o)) return true;
    switch (o->m_objectType) {
        case GameObjectType::Solid:
        case GameObjectType::Hazard:
        case GameObjectType::AnimatedHazard:
        case GameObjectType::CollisionObject:
        case GameObjectType::Slope:
            return true;
        default:
            return false;
    }
}

// Easing de GD (mismas formulas que cocos2d CCActionEase). type = valor de EasingType (0..18)
static float gdEase(int type, float t, float rate) {
    t = std::clamp(t, 0.f, 1.f);
    if (rate <= 0.f) rate = 2.f;
    const float PI = 3.14159265f;
    auto bounceOut = [](float x) {
        if (x < 1.f / 2.75f) return 7.5625f * x * x;
        if (x < 2.f / 2.75f) { x -= 1.5f / 2.75f; return 7.5625f * x * x + 0.75f; }
        if (x < 2.5f / 2.75f) { x -= 2.25f / 2.75f; return 7.5625f * x * x + 0.9375f; }
        x -= 2.625f / 2.75f; return 7.5625f * x * x + 0.984375f;
    };
    auto elasticOut = [&](float x, float per) {
        if (x == 0.f || x == 1.f) return x;
        float s = per / 4.f;
        return std::pow(2.f, -10.f * x) * std::sin((x - s) * (PI * 2.f) / per) + 1.f;
    };
    auto elasticIn = [&](float x, float per) {
        if (x == 0.f || x == 1.f) return x;
        float s = per / 4.f;
        x -= 1.f;
        return -std::pow(2.f, 10.f * x) * std::sin((x - s) * (PI * 2.f) / per);
    };
    const float c = 1.70158f;
    switch (type) {
        case 1: { float x = t * 2; return x < 1 ? 0.5f * std::pow(x, rate) : 1.f - 0.5f * std::pow(2 - x, rate); }
        case 2: return std::pow(t, rate);
        case 3: return std::pow(t, 1.f / rate);
        case 4: { float p = rate * 1.5f; if (t < 0.5f) return 0.5f * elasticIn(t * 2, p); return 0.5f * elasticOut(t * 2 - 1, p) + 0.5f; }
        case 5: return elasticIn(t, rate);
        case 6: return elasticOut(t, rate);
        case 7: return t < 0.5f ? 0.5f * (1.f - bounceOut(1.f - t * 2)) : 0.5f * bounceOut(t * 2 - 1) + 0.5f;
        case 8: return 1.f - bounceOut(1.f - t);
        case 9: return bounceOut(t);
        case 10: { if (t == 0 || t == 1) return t; float x = t * 2; return x < 1 ? 0.5f * std::pow(2.f, 10 * (x - 1)) : 0.5f * (-std::pow(2.f, -10 * (x - 1)) + 2); }
        case 11: return t == 0 ? 0.f : std::pow(2.f, 10 * (t - 1));
        case 12: return t == 1 ? 1.f : 1.f - std::pow(2.f, -10 * t);
        case 13: return -0.5f * (std::cos(PI * t) - 1);
        case 14: return -std::cos(t * PI / 2) + 1;
        case 15: return std::sin(t * PI / 2);
        case 16: { float x = t * 2, cc = c * 1.525f; return x < 1 ? 0.5f * (x * x * ((cc + 1) * x - cc)) : 0.5f * ((x - 2) * (x - 2) * ((cc + 1) * (x - 2) + cc) + 2); }
        case 17: return t * t * ((c + 1) * t - c);
        case 18: { float x = t - 1; return x * x * ((c + 1) * x + c) + 1; }
        default: return t; // 0 = None (lineal)
    }
}

// Lee los Move Triggers (ID 901) del nivel, recuerda los que ya se han disparado de verdad
// y, durante la simulacion, mueve los objetos afectados tick a tick (y restaura todo al terminar).
class MoveTracker {
public:
    struct Info {
        EffectGameObject* trig = nullptr;
        int group = 0;
        CCPoint off{0, 0};
        float dur = 0.f;
        int easing = 0;
        float rate = 2.f;
        float x = 0.f;
    };

private:
    struct Active { const Info* info; double start; bool real; };
    struct Base { GameObject* obj; CCPoint pos; CCPoint last; std::vector<const Info*> affect; };

    bool m_scanned = false;
    std::vector<Info> m_infos;
    std::unordered_map<EffectGameObject*, size_t> m_index;
    std::unordered_map<int, std::vector<GameObject*>> m_groupCache;

    double m_clock = 0.0;                   // tiempo de nivel acumulado (s) segun los ticks reales de P1
    std::vector<Active> m_realActive;       // moves disparados de verdad y aun en curso

    // estado de una simulacion
    std::vector<Active> m_simActive;
    std::vector<const Info*> m_pending;
    std::vector<Base> m_bases;
    double m_simStartClock = 0.0;
    double m_stepSec = 1.0 / 240.0;
    bool m_inSim = false;

    // flags de activacion de orbes/pads modificados por la simulacion (hay que dejarlos como estaban)
    struct Act { GameObject* o; bool a; bool a2; };
    std::vector<Act> m_acts;
    std::unordered_set<GameObject*> m_actSet;

    std::vector<GameObject*> const& groupObjects(PlayLayer* pl, int g) {
        auto it = m_groupCache.find(g);
        if (it != m_groupCache.end()) return it->second;
        std::vector<GameObject*> v;
        if (auto* arr = pl->getGroup(g)) {
            for (auto* o : CCArrayExt<GameObject*>(arr)) if (o) v.push_back(o);
        }
        return m_groupCache.emplace(g, std::move(v)).first->second;
    }

    void scan(PlayLayer* pl) {
        m_scanned = true;
        m_infos.clear(); m_index.clear(); m_groupCache.clear();
        if (!pl || !pl->m_objects) return;
        for (auto* o : CCArrayExt<GameObject*>(pl->m_objects)) {
            if (!o || o->m_objectID != 901) continue;
            auto* t = static_cast<EffectGameObject*>(o);
            Info i;
            i.trig = t;
            i.group = t->m_targetGroupID;
            i.off = t->m_moveOffset;
            i.dur = t->m_duration;
            i.easing = static_cast<int>(t->m_easingType);
            i.rate = t->m_easingRate;
            i.x = t->getPositionX();
            // Limitaciones: "move target" y "lock to player" dependen de posiciones dinamicas -> se ignoran
            if (t->m_useMoveTarget || t->m_lockToPlayerX || t->m_lockToPlayerY) i.group = 0;
            m_index[t] = m_infos.size();
            m_infos.push_back(i);
        }
    }

    static CCPoint deltaOf(const Active& a, double now, double baseTime) {
        float d = a.info->dur;
        auto prog = [&](double t) -> float {
            if (d <= 0.0001f) return t >= a.start ? 1.f : 0.f;
            return std::clamp(static_cast<float>((t - a.start) / d), 0.f, 1.f);
        };
        float e1 = gdEase(a.info->easing, prog(now), a.info->rate);
        float e0 = a.real ? gdEase(a.info->easing, prog(baseTime), a.info->rate) : 0.f;
        return a.info->off * (e1 - e0);
    }

    void setObjPos(GameObject* o, CCPoint p) {
        o->setPosition(p);
        o->m_isObjectRectDirty = true;
        o->m_isOrientedRectDirty = true;
    }

public:
    void reset() {
        m_clock = 0.0;
        m_realActive.clear();
        m_scanned = false;      // se vuelve a leer en la siguiente simulacion
    }
    void tickReal(float dt) { m_clock += dt / 60.0; }

    void onRealFired(EffectGameObject* t) {
        if (!t || t->m_objectID != 901) return;
        if (!m_scanned) scan(PlayLayer::get());
        auto it = m_index.find(t);
        if (it == m_index.end() || m_infos[it->second].group == 0) return;
        m_realActive.push_back({&m_infos[it->second], m_clock, true});
    }

    bool any() const { return !m_bases.empty(); }
    std::vector<Base> const& bases() const { return m_bases; }

    void rememberActivation(GameObject* o) {
        if (!m_inSim || !o || !m_actSet.insert(o).second) return;
        m_acts.push_back({o, o->m_hasBeenActivated, o->m_hasBeenActivatedP2});
    }

    void begin(PlayLayer* pl, float simX, float lookAheadX, float stepSec) {
        m_inSim = true;
        if (!m_scanned) scan(pl);
        m_stepSec = stepSec;
        m_simStartClock = m_clock;
        m_simActive.clear(); m_pending.clear(); m_bases.clear();

        // limpiar moves reales ya terminados
        m_realActive.erase(std::remove_if(m_realActive.begin(), m_realActive.end(), [&](Active const& a) {
            return m_clock >= a.start + a.info->dur;
        }), m_realActive.end());
        for (auto const& a : m_realActive) m_simActive.push_back(a);

        for (auto const& i : m_infos) {
            if (i.group == 0 || !i.trig) continue;
            if (i.trig->m_isSpawnTriggered || i.trig->m_isTouchTriggered) continue;   // las cadenas de spawn no se predicen (v1)
            if (i.x > simX && i.x < lookAheadX && m_realActive.end() ==
                std::find_if(m_realActive.begin(), m_realActive.end(), [&](Active const& a) { return a.info == &i; })) {
                m_pending.push_back(&i);
            }
        }

        std::unordered_map<GameObject*, size_t> baseOf;
        auto addInfo = [&](const Info* inf) {
            for (auto* o : groupObjects(pl, inf->group)) {
                if (!isSimCollidable(o)) continue;
                auto it = baseOf.find(o);
                if (it == baseOf.end()) {
                    it = baseOf.emplace(o, m_bases.size()).first;
                    m_bases.push_back({o, o->getPosition(), o->getPosition(), {}});
                }
                auto& v = m_bases[it->second].affect;
                if (std::find(v.begin(), v.end(), inf) == v.end()) v.push_back(inf);
            }
        };
        for (auto const& a : m_simActive) addInfo(a.info);
        for (auto* i : m_pending) addInfo(i);
    }

    // Avanza el mundo hasta el tick k de la simulacion (k = 1 -> un tick despues del estado real)
    void stepTo(int k, float simPlayerX) {
        if (m_bases.empty()) return;
        double now = m_simStartClock + k * m_stepSec;

        for (size_t n = 0; n < m_pending.size();) {
            if (m_pending[n]->x <= simPlayerX) {
                m_simActive.push_back({m_pending[n], now, false});
                m_pending.erase(m_pending.begin() + n);
            } else n++;
        }
        if (m_simActive.empty()) return;

        for (auto& b : m_bases) {
            CCPoint d{0, 0};
            for (auto const& a : m_simActive) {
                if (std::find(b.affect.begin(), b.affect.end(), a.info) == b.affect.end()) continue;
                d = d + deltaOf(a, now, m_simStartClock);
            }
            CCPoint np = b.pos + d;
            if (np.x != b.last.x || np.y != b.last.y) {
                setObjPos(b.obj, np);
                b.last = np;
            }
        }
    }

    void end() {
        for (auto& b : m_bases) {
            setObjPos(b.obj, b.pos);
        }
        m_bases.clear();
        m_simActive.clear();
        m_pending.clear();
        for (auto& a : m_acts) {
            a.o->m_hasBeenActivated = a.a;
            a.o->m_hasBeenActivatedP2 = a.a2;
        }
        m_acts.clear();
        m_actSet.clear();
        m_inSim = false;
    }
};

static MoveTracker s_moves;

class TrajectoryBufferSim {
private:
    PlayerObject* m_simPlayer1 = nullptr;
    PlayerObject* m_simPlayer2 = nullptr;

    bool m_simulating = false;
    bool m_firingBuffered = false;
    bool m_simulationDead = false;

    // Tracks actual physical mouse state vs what PlayLayer currently has active
    bool m_physicalHolding = false;
    bool m_gameHolding = false;

    // Estado ON/OFF y limites de ticks: viven en pbuf::g (los escribe el HUD cada frame, segun el vehiculo)

    // Buffer state
    int  m_waitTicks = -1;          // -1 = inactive, >= 0 = countdown ticks, -2 = queued opposite tick
    bool m_bufferedPush = true;     // true = buffered PRESS, false = buffered RELEASE
    bool m_queuedOpposite = false;  // true if user quickly released/re-pressed during wait
    float m_frameDt = 0.25f;
    float m_rawDt = 0.25f;          // dt real (sin dividir por timewarp)        // GD 240 TPS normalized dt is 0.25f

    // Streak-based Spam Guard state
    int  m_ticksSinceLastInput = 999;
    int  m_rapidInputStreak = 0;

    PlayerObject* createTrajectoryPlayer() {
        PlayLayer* pl = PlayLayer::get();
        if (!pl) return nullptr;

        PlayerObject* player = PlayerObject::create(1, 1, pl, pl, true);
        player->retain();
        player->setPosition({0, 105});
        player->setVisible(false);
        pl->m_objectLayer->addChild(player);
        return player;
    }

    void resetCollisionLog(PlayerObject* self) {
        self->m_collisionLogTop->removeAllObjects();
        self->m_collisionLogBottom->removeAllObjects();
        self->m_collisionLogLeft->removeAllObjects();
        self->m_collisionLogRight->removeAllObjects();
    }

    // Matches Show Trajectory's exact attribute + hitbox size synchronization
    void syncSimPlayer(PlayerObject* sim, PlayerObject* real) {
        sim->setVisible(false);
        sim->copyAttributes(real);
        sim->setScale(real->getScale());
        sim->setContentSize(real->getContentSize());
        sim->m_width = real->m_width;
        sim->m_height = real->m_height;
        sim->m_gravityMod = real->m_gravityMod;
        sim->m_isOnGround = real->m_isOnGround;
        sim->m_isUpsideDown = real->m_isUpsideDown;
        sim->m_vehicleSize = real->m_vehicleSize;
        sim->m_playerSpeed = real->m_playerSpeed;
    }

    float getValidDt() const {
        if (m_frameDt > 0.001f && m_frameDt < 2.0f) {
            return m_frameDt;
        }
        return 0.25f;
    }

public:
    void init() {
        PlayLayer* pl = PlayLayer::get();
        if (!pl) return;

        m_simPlayer1 = createTrajectoryPlayer();
        m_simPlayer2 = createTrajectoryPlayer();
        m_waitTicks = -1;
        m_queuedOpposite = false;
        m_physicalHolding = false;
        m_gameHolding = false;
        m_simulating = false;
        m_firingBuffered = false;
        m_ticksSinceLastInput = 999;
        m_rapidInputStreak = 0;
    }

    void quit() {
        if (m_simPlayer1) {
            m_simPlayer1->release();
            m_simPlayer1 = nullptr;
        }
        if (m_simPlayer2) {
            m_simPlayer2->release();
            m_simPlayer2 = nullptr;
        }
        m_simulating = false;
        m_firingBuffered = false;
        m_waitTicks = -1;
        m_queuedOpposite = false;
        m_rapidInputStreak = 0;
    }

    bool isSimulating() const { return m_simulating; }
    float rawDt() const { return m_rawDt; }
    float validDt() const { return getValidDt(); }
    bool isFiringBuffered() const { return m_firingBuffered; }

    bool checkSimulationDeath(PlayerObject* player) {
        if (m_simulating && (player == m_simPlayer1 || player == m_simPlayer2)) {
            m_simulationDead = true;
            return true;
        }
        // Real player died -> clear any pending buffer so respawn is 100% clean
        m_waitTicks = -1;
        m_queuedOpposite = false;
        m_gameHolding = false;
        m_rapidInputStreak = 0;
        return false;
    }

    void setFrameDelta(float dt) {
        PlayLayer* pl = PlayLayer::get();
        m_rawDt = dt;
        if (pl && pl->m_gameState.m_timeWarp > 0.f) {
            m_frameDt = dt / pl->m_gameState.m_timeWarp;
        } else {
            m_frameDt = dt;
        }
    }

    int getSurvivalTicks(PlayLayer* pl, PlayerObject* realP1, PlayerObject* realP2, bool isDual, int delayFrames, bool actionPush, int maxTicks) {
        if (!pl || !m_simPlayer1 || !realP1) return maxTicks;

        m_simulating = true;
        m_simulationDead = false;

        syncSimPlayer(m_simPlayer1, realP1);
        if (isDual && m_simPlayer2 && realP2) {
            syncSimPlayer(m_simPlayer2, realP2);
        }

        // Mundo dinamico: move triggers (activos + futuros) y orbes/pads
        {
            float vx = std::max(1.f, std::abs(realP1->m_playerSpeed)) * 6.f;   // px por tick (cota alta)
            s_moves.begin(pl, realP1->getPositionX(), realP1->getPositionX() + vx * (maxTicks + delayFrames) + 60.f,
                          getValidDt() / 60.f);
        }

        // Before delayFrames, maintain the pre-input button state
        if (delayFrames > 0) {
            if (!actionPush) {
                m_simPlayer1->pushButton(PlayerButton::Jump);
                if (isDual && m_simPlayer2) {
                    m_simPlayer2->pushButton(PlayerButton::Jump);
                }
            } else {
                m_simPlayer1->releaseButton(PlayerButton::Jump);
                if (isDual && m_simPlayer2) {
                    m_simPlayer2->releaseButton(PlayerButton::Jump);
                }
            }
        }

        float dt = getValidDt();
        int survived = 0;
        bool isContinuousMode = realP1->m_isDart || realP1->m_isBird || realP1->m_isSwing ||
                                (isDual && realP2 && (realP2->m_isDart || realP2->m_isBird || realP2->m_isSwing));
        int airborneTicks = 0;
        int safeLandedTicks = 0;

        for (int i = 0; i < delayFrames + maxTicks; i++) {
            if (i == delayFrames) {
                if (actionPush) {
                    m_simPlayer1->pushButton(PlayerButton::Jump);
                    if (isDual && m_simPlayer2) {
                        m_simPlayer2->pushButton(PlayerButton::Jump);
                    }
                } else {
                    m_simPlayer1->releaseButton(PlayerButton::Jump);
                    if (isDual && m_simPlayer2) {
                        m_simPlayer2->releaseButton(PlayerButton::Jump);
                    }
                }
            }

            m_simPlayer1->update(dt);
            m_simPlayer1->updateRotation(dt);
            if (isDual && m_simPlayer2) {
                m_simPlayer2->update(dt);
                m_simPlayer2->updateRotation(dt);
            }
            s_moves.stepTo(i + 1, m_simPlayer1->getPositionX());
            if (m_simulationDead) break;

            resetCollisionLog(m_simPlayer1);
            pl->checkCollisions(m_simPlayer1, dt, false);
            if (m_simulationDead) break;

            if (isDual && m_simPlayer2 && realP2) {
                resetCollisionLog(m_simPlayer2);
                pl->checkCollisions(m_simPlayer2, dt, false);
                if (m_simulationDead) break;
            }

            if (i >= delayFrames) {
                survived++;
                if (!isContinuousMode && !s_moves.any()) {   // con objetos en movimiento no se da por "seguro" al aterrizar
                    bool p1Grounded = m_simPlayer1->m_isOnGround;
                    bool p2Grounded = (!isDual || !m_simPlayer2) ? true : m_simPlayer2->m_isOnGround;
                    if (!p1Grounded || !p2Grounded) {
                        airborneTicks++;
                        safeLandedTicks = 0;
                    } else if (airborneTicks >= 15) {
                        safeLandedTicks++;
                        if (safeLandedTicks >= 10) {
                            survived = maxTicks;
                            break;
                        }
                    }
                }
            }
        }

        s_moves.end();
        m_simulating = false;
        return survived;
    }

    bool interceptInput(GJBaseGameLayer* layer, bool push, bool isPlayer1) {
        PlayLayer* pl = PlayLayer::get();
        if (!pl) return true;

        bool isDual = pl->m_gameState.m_isDualMode && pl->m_player2 != nullptr;
        bool isTwoPlayerControl = pl->m_level && pl->m_level->m_twoPlayerMode;

        // Keep Player 2 synced with Player 1 in 1-player Dual mode
        if (!isPlayer1) {
            if (isDual && !isTwoPlayerControl && (m_waitTicks >= 0 || m_waitTicks == -2)) {
                return false;
            }
            return true;
        }

        // Always record the user's true physical mouse/key state first!
        m_physicalHolding = push;

        if (!pbuf::g.flag) {
            m_gameHolding = push;
            return true;
        }

        PlayerObject* p1 = layer->m_player1;
        PlayerObject* p2 = isDual ? pl->m_player2 : nullptr;
        if (!p1 || p1->m_isDead) {
            m_waitTicks = -1;
            m_queuedOpposite = false;
            m_gameHolding = push;
            return true;
        }

        // 1. NEVER touch Ship mode (or Dual with Ship)
        if (p1->m_isShip || (isDual && p2 && p2->m_isShip)) {
            m_waitTicks = -1;
            m_queuedOpposite = false;
            m_gameHolding = push;
            return true;
        }

        // 2. If an input is currently waiting in the buffer and the user quickly releases
        // (or re-presses) 1-2 ticks later, queue it for the next tick so fast taps are never eaten
        if (m_waitTicks >= 0) {
            if (push != m_bufferedPush) {
                m_queuedOpposite = true;
            } else {
                m_queuedOpposite = false;
            }
            m_ticksSinceLastInput = 0;
            return false;
        }

        // If a queued opposite action is due to fire on the next tick (m_waitTicks == -2)
        // and the user clicks again, fire the pending opposite action first
        if (m_waitTicks == -2) {
            m_waitTicks = -1;
            m_queuedOpposite = false;
            if (m_gameHolding != !m_bufferedPush) {
                m_SimFireButton(pl, !m_bufferedPush);
            }
        }

        bool isWave = p1->m_isDart || (isDual && p2 && p2->m_isDart);
        bool isMiniWave = (p1->m_isDart && p1->m_vehicleSize < 1.0f) ||
                          (isDual && p2 && p2->m_isDart && p2->m_vehicleSize < 1.0f);

        // 3. STREAK-BASED WAVE SPAM GUARD:
        // Only bypasses if you perform 3+ rapid inputs in a row (< 8 ticks apart).
        // Single micro-clicks and normal Wave turns around spikes/sawblades stay 100% active!
        if (m_ticksSinceLastInput < SPAM_INTERVAL_TICKS) {
            m_rapidInputStreak++;
        } else {
            m_rapidInputStreak = 0;
        }
        m_ticksSinceLastInput = 0;

        if (isWave && m_rapidInputStreak >= SPAM_STREAK_REQUIRED) {
            m_gameHolding = push;
            return true;
        }

        // Only Wave (m_isDart) and Robot (m_isRobot) change trajectory on RELEASE
        bool releaseAffectsPhysics = p1->m_isDart || p1->m_isRobot ||
                                     (isDual && p2 && (p2->m_isDart || p2->m_isRobot));
        if (!push && !releaseAffectsPhysics) {
            m_gameHolding = false;
            return true;
        }

        m_queuedOpposite = false;

        // Full Show-Trajectory lookahead for Wave (65 ticks / 50 ticks Mini Wave)
        // so it sees spikes and sawblades 1-3 blocks ahead along the diagonal path!
        int targetHorizon = isMiniWave ? 50 : (isWave ? 65 : 160);
        int maxDelayFrames = static_cast<int>(isWave ? pbuf::g.wave : pbuf::g.all);
        // Extension de la version "extended": con limites mayores de 16 ticks se mira mas lejos en la trayectoria
        if (maxDelayFrames > 16) targetHorizon += maxDelayFrames - 16;

        // 4. Check Frame 0 (immediate input) across the full trajectory horizon
        int ticksFrame0 = getSurvivalTicks(pl, p1, p2, isDual, 0, push, targetHorizon);
        if (ticksFrame0 >= targetHorizon) {
            m_gameHolding = push;
            return true;
        }

        // 5. Frame 0 hits a spike, sawblade, or slope within the horizon!
        // Scan +1..maxDelayFrames for the earliest delay that genuinely clears the obstacle.
        int bestDelay = 0;
        int bestTicks = ticksFrame0;

        for (int delay = 1; delay <= maxDelayFrames; delay++) {
            int ticks = getSurvivalTicks(pl, p1, p2, isDual, delay, push, targetHorizon);

            if (isWave) {
                // In Wave mode, delaying a turn inside a flat corridor naturally adds ~(2 * delay)
                // ticks against the opposite wall. Require at least (2 * delay + 10) extra ticks
                // so we ONLY buffer when the delayed trajectory actually clears past a spike,
                // sawblade, or corner!
                int minClearedGain = (delay * 2) + 10;
                if (ticks >= targetHorizon && (ticks - ticksFrame0) >= (delay * 2) + 6) {
                    bestDelay = delay;
                    bestTicks = ticks;
                    break;
                }
                if (ticks >= ticksFrame0 + minClearedGain && ticks > bestTicks + 2) {
                    bestDelay = delay;
                    bestTicks = ticks;
                    if (ticks >= targetHorizon) break;
                }
            } else {
                if (ticks >= targetHorizon) {
                    bestDelay = delay;
                    bestTicks = ticks;
                    break;
                }
                if (ticks > bestTicks + 12) {
                    bestDelay = delay;
                    bestTicks = ticks;
                }
            }
        }

        if (bestDelay > 0) {
            // se avisa al HUD del click bufferizado
            pbuf::g.last = bestDelay;
            pbuf::g.press = push ? 1 : 0;
            pbuf::g.seq++;
            m_bufferedPush = push;
            m_waitTicks = bestDelay;
            return false;
        }

        m_gameHolding = push;
        return true;
    }

    void onPlayerUpdate(PlayerObject* player) {
        PlayLayer* pl = PlayLayer::get();
        if (!pl || player != pl->m_player1) return;

        // ON/OFF (F6 y la tecla del HUD) lo gestiona el HUD; aqui solo se mira el flag
        if (!pbuf::g.flag) return;

        if (m_ticksSinceLastInput < 999) {
            m_ticksSinceLastInput++;
        }

        if (player->m_isShip || (pl->m_gameState.m_isDualMode && pl->m_player2 && pl->m_player2->m_isShip)) {
            m_waitTicks = -1;
            m_queuedOpposite = false;
            if (m_gameHolding != m_physicalHolding) {
                m_SimFireButton(pl, m_physicalHolding);
            }
            return;
        }

        if (m_waitTicks == 0) {
            m_SimFireButton(pl, m_bufferedPush);
            if (m_queuedOpposite) {
                m_waitTicks = -2;
                m_queuedOpposite = false;
            } else {
                m_waitTicks = -1;
            }
        } else if (m_waitTicks > 0) {
            m_waitTicks--;
        } else if (m_waitTicks == -2) {
            m_SimFireButton(pl, !m_bufferedPush);
            m_queuedOpposite = false;
            m_waitTicks = -1;
        } else if (m_waitTicks == -1) {
            // FAILSAFE: When no buffer is active, guarantee the game's hold state
            // matches your actual physical mouse button so Wave/Mini Wave can NEVER stay stuck holding!
            if (m_gameHolding != m_physicalHolding) {
                m_SimFireButton(pl, m_physicalHolding);
            }
        }
    }

    void m_SimFireButton(PlayLayer* pl, bool push) {
        m_firingBuffered = true;
        m_gameHolding = push;
        pl->handleButton(push, 1, true);
        if (pl->m_gameState.m_isDualMode && (!pl->m_level || !pl->m_level->m_twoPlayerMode)) {
            pl->handleButton(push, 1, false);
        }
        m_firingBuffered = false;
    }
};

static TrajectoryBufferSim s_sim;

class $modify(BufPlayLayer, PlayLayer) {
    static void onModify(auto& self) {
        (void)self.setHookPriority("PlayLayer::destroyPlayer", -1000000);
        (void)self.setHookPriority("PlayLayer::playEndAnimationToPos", -1000000);
        (void)self.setHookPriority("PlayLayer::flipGravity", -1000000);
    }

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
        s_sim.init();
        return true;
    }

    void destroyPlayer(PlayerObject* player, GameObject* gameObject) {
        if (s_sim.checkSimulationDeath(player)) return;
        PlayLayer::destroyPlayer(player, gameObject);
    }

    void playEndAnimationToPos(cocos2d::CCPoint p0) {
        if (s_sim.isSimulating()) return;
        PlayLayer::playEndAnimationToPos(p0);
    }

    void flipGravity(PlayerObject* p0, bool p1, bool p2) {
        if (s_sim.isSimulating()) {
            // La simulacion SI necesita voltear la gravedad (orbes/pads de gravedad), pero sin efectos visuales
            if (p0) p0->flipGravity(p1, p2);
            return;
        }
        PlayLayer::flipGravity(p0, p1, p2);
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        s_moves.reset();
    }

    void onQuit() {
        PlayLayer::onQuit();
        s_moves.reset();
        s_sim.quit();
    }
};

class $modify(BufEditorLayer, LevelEditorLayer) {
    bool init(GJGameLevel* level, bool unk) {
        bool res = LevelEditorLayer::init(level, unk);
        s_sim.quit();
        return res;
    }
};

class $modify(BufHardStreak, HardStreak) {
    static void onModify(auto& self) {
        (void)self.setHookPriority("HardStreak::addPoint", -1000000);
    }

    void addPoint(cocos2d::CCPoint p0) {
        if (s_sim.isSimulating()) return;
        HardStreak::addPoint(p0);
    }
};

class $modify(BufEffectGameObject, EffectGameObject) {
    static void onModify(auto& self) {
        (void)self.setHookPriority("EffectGameObject::triggerObject", -1000000);
    }

    void triggerObject(GJBaseGameLayer* p0, int p1, const gd::vector<int>* p2) {
        if (s_sim.isSimulating()) return;
        EffectGameObject::triggerObject(p0, p1, p2);
        s_moves.onRealFired(this);   // si es un Move Trigger real, lo recordamos para predecir lo que le queda por mover
    }
};

class $modify(BufGameObject, GameObject) {
    static void onModify(auto& self) {
        (void)self.setHookPriority("GameObject::playShineEffect", -1000000);
    }

    void playShineEffect() {
        if (s_sim.isSimulating()) return;
        GameObject::playShineEffect();
    }
};

class $modify(BufPlayerObject, PlayerObject) {
    static void onModify(auto& self) {
        (void)self.setHookPriority("PlayerObject::playSpiderDashEffect", -1000000);
        (void)self.setHookPriority("PlayerObject::incrementJumps", -1000000);
        (void)self.setHookPriority("PlayerObject::ringJump", -1000000);
    }

    void playSpiderDashEffect(cocos2d::CCPoint from, cocos2d::CCPoint to) {
        if (s_sim.isSimulating()) return;
        PlayerObject::playSpiderDashEffect(from, to);
    }

    void incrementJumps() {
        if (s_sim.isSimulating()) return;
        PlayerObject::incrementJumps();
    }

    void update(float dt) {
        if (!s_sim.isSimulating()) {
            s_sim.onPlayerUpdate(this);
        }
        PlayerObject::update(dt);
        if (PlayLayer* pl = PlayLayer::get(); pl && !s_sim.isSimulating()) {
            s_sim.setFrameDelta(dt);
            if (this == pl->m_player1) s_moves.tickReal(s_sim.validDt());
        }
    }

    void ringJump(RingObject* p0, bool p1) {
        // Orbes: se ejecutan tambien en la simulacion (si no, la prediccion atraviesa los orbes)
        PlayerObject::ringJump(p0, p1);
    }
};

class $modify(BufBaseGameLayer, GJBaseGameLayer) {
    static void onModify(auto& self) {
        (void)self.setHookPriority("GJBaseGameLayer::canBeActivatedByPlayer", -1000000);
        (void)self.setHookPriority("GJBaseGameLayer::handleButton", -1000000);
        (void)self.setHookPriority("GJBaseGameLayer::collisionCheckObjects", -1000000);
        (void)self.setHookPriority("GJBaseGameLayer::playerTouchedRing", -1000000);
    }

    bool canBeActivatedByPlayer(PlayerObject* p0, EffectGameObject* p1) {
        if (s_sim.isSimulating()) {
            // solo orbes/pads pueden activarse dentro de la simulacion; triggers y demas, no
            if (!isOrbOrPad(p1)) return false;
            s_moves.rememberActivation(p1);
        }
        return GJBaseGameLayer::canBeActivatedByPlayer(p0, p1);
    }

    void handleButton(bool down, int button, bool isPlayer1) {
        if (!s_sim.isSimulating() && !s_sim.isFiringBuffered() && button == 1 && PlayLayer::get()) {
            if (!s_sim.interceptInput(this, down, isPlayer1)) {
                return;
            }
        }
        GJBaseGameLayer::handleButton(down, button, isPlayer1);
    }

    void collisionCheckObjects(PlayerObject* player, gd::vector<GameObject*>* vec, int objectsCount, float dt) {
        if (s_sim.isSimulating()) {
            gd::vector<GameObject*> extra;
            extra.reserve(objectsCount + 16);
            std::unordered_set<GameObject*> have;
            for (int i = 0; i < objectsCount; i++) {
                GameObject* obj = vec->at(i);
                if (!obj || !isSimCollidable(obj)) continue;
                if (isOrbOrPad(obj)) s_moves.rememberActivation(obj);
                extra.push_back(obj);
                have.insert(obj);
            }
            // objetos movidos por triggers: su "seccion" puede estar desfasada -> se anaden a mano si estan cerca
            if (s_moves.any()) {
                float px = player->getPositionX();
                for (auto const& b : s_moves.bases()) {
                    if (std::abs(b.obj->getPositionX() - px) > 240.f) continue;
                    if (have.insert(b.obj).second) extra.push_back(b.obj);
                }
            }
            GJBaseGameLayer::collisionCheckObjects(player, &extra, extra.size(), dt);
            return;
        }
        GJBaseGameLayer::collisionCheckObjects(player, vec, objectsCount, dt);
    }

    void playerTouchedRing(PlayerObject* player, RingObject* ring) {
        if (s_sim.isSimulating()) s_moves.rememberActivation(ring);
        GJBaseGameLayer::playerTouchedRing(player, ring);
    }
};
