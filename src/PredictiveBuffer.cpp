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
#include <Geode/modify/EnhancedGameObject.hpp>
#include <Geode/modify/RingObject.hpp>
#include <Geode/modify/FMODAudioEngine.hpp>
#include <algorithm>
#include <unordered_set>
#include <vector>
#include "PredictiveBuffer.hpp"
#include "Motion.hpp"

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
    float m_frameDt = 0.25f;        // GD 240 TPS normalized dt is 0.25f

    // Streak-based Spam Guard state
    int  m_ticksSinceLastInput = 999;
    int  m_rapidInputStreak = 0;

    // ---- orbes / pads en la simulacion ----
    // Los orbes/pads son los MISMOS objetos que ve el jugador real: se guarda su estado de activacion antes de simular,
    // se restaura al terminar, y se anota aparte que objetos ha activado el jugador simulado (no repetir en la misma simulacion).
    struct Guard { EnhancedGameObject* obj; bool activated, by1, by2; int uid; };
    std::vector<Guard> m_guards;
    std::unordered_set<int> m_guardIds;
    std::unordered_set<int> m_simActivated;

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

        // estado de orbes / pads del jugador real
        sim->m_touchedRings = real->m_touchedRings;
        sim->m_ringRelatedSet = real->m_ringRelatedSet;
        if (sim->m_touchingRings) {
            sim->m_touchingRings->removeAllObjects();
            if (real->m_touchingRings) {
                for (auto ring : geode::cocos::CCArrayExt<RingObject, false>(real->m_touchingRings)) {
                    sim->m_touchingRings->addObject(ring);
                }
            }
        }
        sim->m_touchedRing = real->m_touchedRing;
        sim->m_stateRingJump = real->m_stateRingJump;
        sim->m_stateRingJump2 = real->m_stateRingJump2;
        sim->m_ringJumpRelated = real->m_ringJumpRelated;
        sim->m_hasEverHitRing = real->m_hasEverHitRing;
        sim->m_padRingRelated = real->m_padRingRelated;
        sim->m_touchedPad = real->m_touchedPad;
        sim->m_dashRing = real->m_dashRing;
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
    bool isSimPlayer(PlayerObject* p) const { return p && (p == m_simPlayer1 || p == m_simPlayer2); }

    void guardObject(GameObject* o) {
        if (!o || !m_guardIds.insert(o->m_uniqueID).second) return;
        auto e = typeinfo_cast<EnhancedGameObject*>(o);
        if (!e) return;
        m_guards.push_back({e, e->m_activated, e->m_activatedByPlayer1, e->m_activatedByPlayer2, e->m_uniqueID});
    }

    void noteActivated(GameObject* o) {
        if (!o) return;
        guardObject(o);
        m_simActivated.insert(o->m_uniqueID);
    }

    bool simMayActivate(PlayerObject* p, EffectGameObject* o) const {
        if (!pbuf::g.orbs || !o || !isSimPlayer(p)) return false;
        if (!motion::isOrbOrPad(o->m_objectType)) return false;
        return m_simActivated.find(o->m_uniqueID) == m_simActivated.end();
    }

    void releaseGuards(PlayLayer* pl) {
        for (auto& g : m_guards) {
            g.obj->m_activated = g.activated;
            g.obj->m_activatedByPlayer1 = g.by1;
            g.obj->m_activatedByPlayer2 = g.by2;
            if (pl && pl->m_effectManager) {
                if (m_simPlayer1) pl->m_effectManager->removeTriggeredID(g.uid, m_simPlayer1->m_uniqueID);
                if (m_simPlayer2) pl->m_effectManager->removeTriggeredID(g.uid, m_simPlayer2->m_uniqueID);
            }
        }
        m_guards.clear();
        m_guardIds.clear();
        m_simActivated.clear();
    }
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
        if (pl && pl->m_gameState.m_timeWarp > 0.f) {
            m_frameDt = dt / pl->m_gameState.m_timeWarp;
        } else {
            m_frameDt = dt;
        }
    }

    int getSurvivalTicks(PlayLayer* pl, PlayerObject* realP1, PlayerObject* realP2, bool isDual, int delayFrames, bool actionPush, int maxTicks,
                         motion::Plan* plan = nullptr) {
        if (!pl || !m_simPlayer1 || !realP1) return maxTicks;

        releaseGuards(pl);
        if (plan && plan->on) plan->begin();

        m_simulating = true;
        m_simulationDead = false;

        syncSimPlayer(m_simPlayer1, realP1);
        if (isDual && m_simPlayer2 && realP2) {
            syncSimPlayer(m_simPlayer2, realP2);
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
            // objetos movidos por Move Triggers: se colocan donde estaran en este tick
            if (plan && plan->on) plan->apply(pl, i + 1, m_simPlayer1->getPositionX());

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
                if (!isContinuousMode) {
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

        m_simulating = false;
        if (plan && plan->on) plan->restore(pl);   // objetos movidos: de vuelta a su sitio real
        releaseGuards(pl);                          // orbes / pads: estado de activacion original
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

        // Plan de movimiento de objetos (Move Triggers) compartido por todas las simulaciones de este click
        if (pbuf::g.moving && !motion::st.scanned) motion::scan(pl);
        motion::Plan plan;
        plan.build(p1, targetHorizon + maxDelayFrames);

        // 4. Check Frame 0 (immediate input) across the full trajectory horizon
        int ticksFrame0 = getSurvivalTicks(pl, p1, p2, isDual, 0, push, targetHorizon, &plan);
        if (ticksFrame0 >= targetHorizon) {
            m_gameHolding = push;
            return true;
        }

        // 5. Frame 0 hits a spike, sawblade, or slope within the horizon!
        // Scan +1..maxDelayFrames for the earliest delay that genuinely clears the obstacle.
        int bestDelay = 0;
        int bestTicks = ticksFrame0;

        for (int delay = 1; delay <= maxDelayFrames; delay++) {
            int ticks = getSurvivalTicks(pl, p1, p2, isDual, delay, push, targetHorizon, &plan);

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

        // cuenta ticks y activaciones de Move Triggers (aunque el buffer este apagado)
        if (pbuf::g.moving) motion::trackTick(player);

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
        motion::clear();
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
        s_sim.init();
        return true;
    }

    // con el nivel montado se leen los Move Triggers y los objetos que mueven
    void setupHasCompleted() {
        PlayLayer::setupHasCompleted();
        motion::scan(this);
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
            // orbes / pads de gravedad: solo el jugador simulado cambia de gravedad, sin efectos
            if (pbuf::g.orbs && s_sim.isSimPlayer(p0)) p0->flipGravity(p1, true);
            return;
        }
        PlayLayer::flipGravity(p0, p1, p2);
    }

    void onQuit() {
        PlayLayer::onQuit();
        s_sim.quit();
        motion::clear();
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
        if (PlayLayer::get() && !s_sim.isSimulating()) {
            s_sim.setFrameDelta(dt);
        }
    }
};

class $modify(BufBaseGameLayer, GJBaseGameLayer) {
    static void onModify(auto& self) {
        (void)self.setHookPriority("GJBaseGameLayer::canBeActivatedByPlayer", -1000000);
        (void)self.setHookPriority("GJBaseGameLayer::handleButton", -1000000);
        (void)self.setHookPriority("GJBaseGameLayer::collisionCheckObjects", -1000000);
        (void)self.setHookPriority("GJBaseGameLayer::gameEventTriggered", -1000000);
    }

    // En la simulacion solo el jugador simulado activa orbes/pads (portales, triggers... siguen bloqueados)
    bool canBeActivatedByPlayer(PlayerObject* p0, EffectGameObject* p1) {
        if (s_sim.isSimulating() && !s_sim.simMayActivate(p0, p1)) return false;
        return GJBaseGameLayer::canBeActivatedByPlayer(p0, p1);
    }

    // eventos de juego (orbe tocado, pad...) pueden lanzar triggers: nunca durante una simulacion
    void gameEventTriggered(GJGameEvent event, int material, int playerID) {
        if (s_sim.isSimulating()) return;
        GJBaseGameLayer::gameEventTriggered(event, material, playerID);
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
            extra.reserve(objectsCount);
            for (int i = 0; i < objectsCount; i++) {
                GameObject* obj = vec->at(i);
                if (!obj) continue;
                if (motion::isCollidable(obj->m_objectType)) {
                    extra.push_back(obj);
                } else if (pbuf::g.orbs && motion::isOrbOrPad(obj->m_objectType)) {
                    s_sim.guardObject(obj);   // guarda su estado antes de que el simulado lo toque
                    extra.push_back(obj);
                }
            }
            GJBaseGameLayer::collisionCheckObjects(player, &extra, extra.size(), dt);
            return;
        }
        GJBaseGameLayer::collisionCheckObjects(player, vec, objectsCount, dt);
    }
};

// ---- orbes / pads: el jugador simulado no deja rastro en los objetos reales ni hace efectos ----
class $modify(BufEnhancedObject, EnhancedGameObject) {
    static void onModify(auto& self) {
        (void)self.setHookPriority("EnhancedGameObject::activatedByPlayer", -1000000);
    }

    void activatedByPlayer(PlayerObject* player) {
        if (s_sim.isSimulating()) {
            if (s_sim.isSimPlayer(player)) s_sim.noteActivated(this);
            return;
        }
        EnhancedGameObject::activatedByPlayer(player);
    }
};

class $modify(BufRingObject, RingObject) {
    static void onModify(auto& self) {
        (void)self.setHookPriority("RingObject::powerOnObject", -1000000);
        (void)self.setHookPriority("RingObject::spawnCircle", -1000000);
    }

    void powerOnObject(int state) {
        if (s_sim.isSimulating()) return;
        RingObject::powerOnObject(state);
    }

    void spawnCircle() {
        if (s_sim.isSimulating()) return;
        RingObject::spawnCircle();
    }
};

// sin sonidos mientras se simula
class $modify(BufAudio, FMODAudioEngine) {
    static void onModify(auto& self) {
        (void)self.setHookPriority("FMODAudioEngine::playEffect", -1000000);
    }

    int playEffect(gd::string path, float speed, float pan, float volume) {
        if (s_sim.isSimulating()) return 0;
        return FMODAudioEngine::playEffect(path, speed, pan, volume);
    }
};
