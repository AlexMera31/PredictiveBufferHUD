// Prediccion de objetos moviles (triggers de movimiento) y utilidades de tipos para el simulador del buffer.
//
// Idea: al cargar el nivel se leen todos los Move Trigger (objeto 901) "normales" (activados por posicion,
// sin "move to target", sin lock a jugador/camara) y los objetos que pertenecen a los grupos que mueven.
// Durante el juego se cuenta en que tick real se activo cada trigger. Antes de simular se construye un "plan":
// los triggers que estan en curso o que el jugador alcanzara dentro del horizonte, y los objetos de sus grupos
// que estan cerca. En cada tick simulado el plan coloca esos objetos donde estarian (con la misma curva de
// easing del trigger) y al terminar la simulacion los devuelve EXACTAMENTE a su sitio real.
#pragma once
#include <Geode/Geode.hpp>
#include <algorithm>
#include <climits>
#include <cmath>
#include <unordered_set>
#include <vector>
#include "PredictiveBuffer.hpp"

namespace motion {
    using namespace geode::prelude;

    // ---- tipos de objeto ----
    inline bool isOrbOrPad(GameObjectType t) {
        switch (t) {
            case GameObjectType::YellowJumpPad:
            case GameObjectType::PinkJumpPad:
            case GameObjectType::RedJumpPad:
            case GameObjectType::GravityPad:
            case GameObjectType::SpiderPad:
            case GameObjectType::YellowJumpRing:
            case GameObjectType::PinkJumpRing:
            case GameObjectType::RedJumpRing:
            case GameObjectType::GravityRing:
            case GameObjectType::GreenRing:
            case GameObjectType::DropRing:
            case GameObjectType::DashRing:
            case GameObjectType::GravityDashRing:
            case GameObjectType::SpiderOrb:
                return true;
            default:
                return false;
        }
    }

    inline bool isCollidable(GameObjectType t) {
        return t == GameObjectType::Solid || t == GameObjectType::Hazard || t == GameObjectType::AnimatedHazard ||
               t == GameObjectType::CollisionObject || t == GameObjectType::Slope;
    }

    // ---- funciones de easing (mismas formulas que cocos2d-x CCActionEase) ----
    inline double bounceTime(double t) {
        if (t < 1.0 / 2.75) return 7.5625 * t * t;
        if (t < 2.0 / 2.75) { t -= 1.5 / 2.75; return 7.5625 * t * t + 0.75; }
        if (t < 2.5 / 2.75) { t -= 2.25 / 2.75; return 7.5625 * t * t + 0.9375; }
        t -= 2.625 / 2.75;
        return 7.5625 * t * t + 0.984375;
    }

    inline double ease(int type, double t, double rate) {
        constexpr double PI = 3.14159265358979323846;
        if (t <= 0.0) return 0.0;
        if (t >= 1.0) return 1.0;
        double r = rate > 0.01 ? rate : 2.0;          // exponente de ease in/out
        double per = (rate > 0.01 && rate < 5.0) ? rate : 0.3;   // periodo de elastic
        switch (type) {
            case 1: { double k = t * 2.0; return k < 1.0 ? 0.5 * std::pow(k, r) : 1.0 - 0.5 * std::pow(2.0 - k, r); }
            case 2: return std::pow(t, r);
            case 3: return std::pow(t, 1.0 / r);
            case 4: {
                double p = per * 1.5, s = p / 4.0, k = t * 2.0 - 1.0;
                return k < 0.0 ? -0.5 * std::pow(2.0, 10.0 * k) * std::sin((k - s) * PI * 2.0 / p)
                               : std::pow(2.0, -10.0 * k) * std::sin((k - s) * PI * 2.0 / p) * 0.5 + 1.0;
            }
            case 5: { double s = per / 4.0, k = t - 1.0; return -std::pow(2.0, 10.0 * k) * std::sin((k - s) * PI * 2.0 / per); }
            case 6: { double s = per / 4.0; return std::pow(2.0, -10.0 * t) * std::sin((t - s) * PI * 2.0 / per) + 1.0; }
            case 7: return t < 0.5 ? (1.0 - bounceTime(1.0 - t * 2.0)) * 0.5 : bounceTime(t * 2.0 - 1.0) * 0.5 + 0.5;
            case 8: return 1.0 - bounceTime(1.0 - t);
            case 9: return bounceTime(t);
            case 10: { double k = t / 0.5; return k < 1.0 ? 0.5 * std::pow(2.0, 10.0 * (k - 1.0)) : 0.5 * (-std::pow(2.0, -10.0 * (k - 1.0)) + 2.0); }
            case 11: return std::pow(2.0, 10.0 * (t - 1.0)) - 0.001;
            case 12: return -std::pow(2.0, -10.0 * t) + 1.0;
            case 13: return -0.5 * (std::cos(PI * t) - 1.0);
            case 14: return -std::cos(t * PI / 2.0) + 1.0;
            case 15: return std::sin(t * PI / 2.0);
            case 16: {
                double o = 1.70158 * 1.525, k = t * 2.0;
                if (k < 1.0) return (k * k * ((o + 1.0) * k - o)) / 2.0;
                k -= 2.0;
                return (k * k * ((o + 1.0) * k + o)) / 2.0 + 1.0;
            }
            case 17: { double o = 1.70158; return t * t * ((o + 1.0) * t - o); }
            case 18: { double o = 1.70158, k = t - 1.0; return k * k * ((o + 1.0) * k + o) + 1.0; }
            default: return t;
        }
    }

    // ---- datos del nivel ----
    constexpr int kNone = INT_MAX;       // trigger todavia sin activar
    constexpr int kDone = -1000000;      // trigger anterior al punto de aparicion (ya terminado)

    struct Trig {
        double x = 0;
        int group = 0;
        double dx = 0, dy = 0;           // desplazamiento total (unidades del juego)
        double durTicks = 0;             // duracion en ticks (240 por segundo)
        int easing = 0;
        double rate = 0;
        int actTick = kNone;             // tick real en el que se activo
    };

    struct Dyn {
        GameObject* obj = nullptr;
        std::vector<int> groups;         // grupos del objeto que mueve algun trigger
    };

    struct LevelState {
        std::vector<Trig> trigs;         // ordenados por x
        std::vector<Dyn> dyn;
        size_t next = 0;                 // siguiente trigger que activara el jugador
        int tick = 0;                    // ticks reales de P1 desde que se cargo el nivel
        float lastX = -1e9f;
        bool scanned = false;
    };

    inline LevelState st;

    inline void clear() { st = LevelState{}; }

    // coloca los indices de activacion segun la X del jugador (aparicion, checkpoint, practica...)
    inline void resetProgress(float x) {
        auto it = std::upper_bound(st.trigs.begin(), st.trigs.end(), static_cast<double>(x),
                                   [](double v, Trig const& t) { return v < t.x; });
        st.next = static_cast<size_t>(it - st.trigs.begin());
        for (size_t i = 0; i < st.trigs.size(); i++) st.trigs[i].actTick = i < st.next ? kDone : kNone;
        st.lastX = x;
    }

    inline void scan(PlayLayer* pl) {
        clear();
        st.scanned = true;
        if (!pl || !pl->m_objects) return;

        std::unordered_set<int> targets;
        for (auto o : geode::cocos::CCArrayExt<GameObject, false>(pl->m_objects)) {
            if (!o || o->m_objectID != 901) continue;            // 901 = Move Trigger
            auto e = typeinfo_cast<EffectGameObject*>(o);
            if (!e) continue;
            if (e->m_isTouchTriggered || e->m_isSpawnTriggered || e->m_useMoveTarget || e->m_isDynamicMode ||
                e->m_lockToPlayerX || e->m_lockToPlayerY || e->m_lockToCameraX || e->m_lockToCameraY) continue;
            if (e->m_targetGroupID <= 0) continue;

            double mx = e->m_moveModX != 0.f ? e->m_moveModX : 1.0;
            double my = e->m_moveModY != 0.f ? e->m_moveModY : 1.0;
            Trig t;
            t.x = o->m_positionX;
            t.group = e->m_targetGroupID;
            t.dx = e->m_moveOffset.x * mx;
            t.dy = e->m_moveOffset.y * my;
            if (t.dx == 0.0 && t.dy == 0.0) continue;
            t.durTicks = std::max(0.0, static_cast<double>(e->m_duration)) * 240.0;
            t.easing = static_cast<int>(e->m_easingType);
            t.rate = e->m_easingRate;
            st.trigs.push_back(t);
            targets.insert(t.group);
        }
        if (st.trigs.empty()) return;
        std::stable_sort(st.trigs.begin(), st.trigs.end(), [](Trig const& a, Trig const& b) { return a.x < b.x; });

        for (auto o : geode::cocos::CCArrayExt<GameObject, false>(pl->m_objects)) {
            if (!o || !o->m_groups || o->m_groupCount <= 0) continue;
            if (!isCollidable(o->m_objectType) && !isOrbOrPad(o->m_objectType)) continue;
            short const* arr = o->m_groups->data();
            Dyn d;
            for (int i = 0; i < o->m_groupCount; i++) {
                if (targets.count(arr[i])) d.groups.push_back(arr[i]);
            }
            if (d.groups.empty()) continue;
            d.obj = o;
            st.dyn.push_back(std::move(d));
        }
        log::info("[PredictiveBuffer] move triggers: {}, moving objects: {}", st.trigs.size(), st.dyn.size());
    }

    // una vez por tick real de P1 (fuera de simulaciones)
    inline void trackTick(PlayerObject* p1) {
        if (!st.scanned || st.trigs.empty() || !p1) return;
        st.tick++;
        float x = p1->getPositionX();
        if (st.lastX < -1e8f || std::abs(x - st.lastX) > 30.f) resetProgress(x);   // reaparicion / checkpoint / teletransporte
        st.lastX = x;
        while (st.next < st.trigs.size() && st.trigs[st.next].x <= x) {
            st.trigs[st.next].actTick = st.tick;
            st.next++;
        }
    }

    // ---- plan de movimiento para una tanda de simulaciones ----
    struct Item {
        GameObject* obj = nullptr;
        double bx = 0, by = 0;           // posicion real al empezar
        double lx = 0, ly = 0;           // m_lastPosition real
        double curX = 0, curY = 0;       // posicion actual (simulada)
        double d0x = 0, d0y = 0;         // desplazamiento modelado en el instante actual
        std::vector<int> tr;             // indices en Plan::ti
    };

    class Plan {
    public:
        bool on = false;

        void build(PlayerObject* p1, int horizonTicks) {
            on = false;
            ti.clear(); act0.clear(); act.clear(); items.clear();
            if (!pbuf::g.moving || !st.scanned || st.trigs.empty() || !p1) return;

            double px = p1->getPositionX();
            double maxDist = horizonTicks * 2.7 + 80.0;           // ~2.4 unidades/tick a la maxima velocidad + margen
            for (size_t j = 0; j < st.trigs.size(); j++) {
                auto const& t = st.trigs[j];
                if (t.actTick == kNone) {
                    if (t.x > px && t.x <= px + maxDist) { ti.push_back(static_cast<int>(j)); act0.push_back(kNone); }
                } else if (t.actTick != kDone) {
                    int rel = t.actTick - st.tick;
                    if (rel + t.durTicks > 0.0) { ti.push_back(static_cast<int>(j)); act0.push_back(rel); }
                }
            }
            if (ti.empty()) return;

            double xlo = px - 500.0, xhi = px + maxDist + 500.0;
            for (auto const& d : st.dyn) {
                if (!d.obj) continue;
                double ox = d.obj->m_positionX;
                if (ox < xlo || ox > xhi) continue;
                Item it;
                for (size_t k = 0; k < ti.size(); k++) {
                    int g = st.trigs[ti[k]].group;
                    if (std::find(d.groups.begin(), d.groups.end(), g) != d.groups.end()) it.tr.push_back(static_cast<int>(k));
                }
                if (it.tr.empty()) continue;
                it.obj = d.obj;
                it.bx = it.curX = ox;
                it.by = it.curY = d.obj->m_positionY;
                it.lx = d.obj->m_lastPosition.x;
                it.ly = d.obj->m_lastPosition.y;
                items.push_back(std::move(it));
                if (items.size() >= 400) break;                    // limite de seguridad
            }
            on = !items.empty();
        }

        // antes de cada simulacion
        void begin() {
            act = act0;
            for (auto& it : items) {
                displacement(it, 0, act0, it.d0x, it.d0y);
                it.curX = it.bx; it.curY = it.by;
            }
        }

        // al principio del tick simulado T (1, 2, 3...), con la X del jugador simulado
        void apply(PlayLayer* pl, int T, double simX) {
            for (size_t k = 0; k < ti.size(); k++) {
                if (act[k] == kNone && st.trigs[ti[k]].x <= simX) act[k] = T;
            }
            for (auto& it : items) {
                double dx, dy;
                displacement(it, T, act, dx, dy);
                double nx = it.bx + (dx - it.d0x);
                double ny = it.by + (dy - it.d0y);
                if (it.obj->m_tempOffsetXRelated) nx = it.bx;
                if (std::abs(nx - it.curX) < 1e-6 && std::abs(ny - it.curY) < 1e-6) continue;
                it.obj->m_lastPosition.x = it.curX;
                it.obj->m_lastPosition.y = it.curY;
                place(pl, it.obj, nx, ny);
                it.curX = nx; it.curY = ny;
            }
        }

        // despues de cada simulacion: todo vuelve a su sitio real
        void restore(PlayLayer* pl) {
            if (!pl) return;
            for (auto& it : items) {
                if (it.curX == it.bx && it.curY == it.by) continue;
                place(pl, it.obj, it.bx, it.by);
                it.obj->m_lastPosition.x = it.lx;
                it.obj->m_lastPosition.y = it.ly;
                it.curX = it.bx; it.curY = it.by;
            }
        }

        ~Plan() { if (on) restore(PlayLayer::get()); }

    private:
        std::vector<int> ti;      // indices en st.trigs
        std::vector<int> act0;    // tick de activacion relativo a "ahora" (<=0 pasado, kNone = futuro)
        std::vector<int> act;     // igual, pero actualizado durante la simulacion
        std::vector<Item> items;

        void displacement(Item const& it, int T, std::vector<int> const& a, double& dx, double& dy) const {
            dx = 0; dy = 0;
            for (int k : it.tr) {
                int ak = a[k];
                if (ak == kNone || ak > T) continue;
                auto const& t = st.trigs[ti[k]];
                double p = t.durTicks <= 0.0 ? 1.0 : std::clamp((T - ak) / t.durTicks, 0.0, 1.0);
                double e = ease(t.easing, p, t.rate);
                dx += t.dx * e;
                dy += t.dy * e;
            }
        }

        static void place(PlayLayer* pl, GameObject* o, double x, double y) {
            o->m_positionX = x;
            o->m_positionY = y;
            o->dirtifyObjectRect();
            o->dirtifyObjectPos();
            pl->updateObjectSection(o);
        }
    };
}
