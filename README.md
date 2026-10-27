# Predictive Buffer With HUD

**English** | [Español](#español)

An in-game, Mega Hack–style menu for the **Predictive Input Buffer** Geode mod (Geometry Dash, Windows). It lets you set how many ticks of buffer each vehicle gets, turn the buffer on and off with a click, watch the buffered clicks live, and customize the menu colors with a color wheel. The menu is available in **English (default)** and **Spanish**.

- **Mod ID:** `alexmera.predictivebufferhud`
- **Geometry Dash:** 2.2081 (Windows)
- **Geode:** 5.11.2
- **Requires:** nothing else. The Predictive Input Buffer engine is now built into this mod. **Uninstall `ai.predictivebuffer` if you have it** (the two conflict).

---

## English

### Features

- **Mega Hack–style windows:** title bar, gray body, white text, rounded corners, soft shadow, and borders that follow your color theme.
- **Ticks per vehicle:** Cube, Ship, UFO, Ball, Wave, Robot, Spider and Swing each have their own value. Use the two arrows on the left of the number: ▲ goes up, ▼ goes down.
  - Steps: `0 → 2 → 4 → 8 → 16 → 24 → 32 → 48 → 54 → 60` (0 = no buffer for that vehicle).
  - The row of the vehicle you are currently playing is highlighted.
- **Click to toggle:** the *Buffer active* row turns the buffer on and off with a single click. The row lights up in the main color when it is on. `F6` also works, and you can set your own key (default `F7`).
- **Rebindable keys:** click the *L-Ctrl: hide menu* row in the *Interface* window, then press any key to change the menu key. The toggle key for the buffer can be changed the same way. Both are saved forever.
- **Buffered click info:** last click (PRESS / RELEASE), ticks saved, frames and milliseconds, total / average / maximum, and a short history.
- **Orbs, pads and moving objects (v2.0.2):** the prediction now simulates yellow / pink / red / gravity / green / dash / spider orbs and pads, and reads **Move Triggers** (position-activated, with their duration and easing) to predict where moving spikes and blocks will be. The buffer keeps working near orbs; it only skips a click when the click's own trajectory passes right through an unused orb (the click may be meant for it). Both can be turned off in the *Buffer* window (*Orbs & pads*, *Moving objs*) if something misbehaves in a level.
  - Not modeled: move-to-target / lock-to-player / lock-to-camera / dynamic moves, touch- and spawn-triggered moves, rotate / follow / scale triggers, portals and speed changes inside the prediction. In those cases the buffer behaves as before.
- **Custom colors:** the main color (blue by default) and the gray color (gray by default) can be changed with a built-in color wheel plus a brightness slider. A button restores the defaults.
- **Language selector:** switch the whole menu between **English** (default) and **Español** from the *Interface* window.
- **Short animations:** windows fold and unfold in a fraction of a second.
- **Everything is remembered:** window positions, folded state, ticks, colors, interface scale and language are saved between sessions.

### How it works

The mod contains the Predictive Input Buffer engine (by AlexMera) and the HUD in the same `.geode`. Two limits are used: one while you are a wave and one for every other vehicle. The HUD detects your current vehicle every frame and sets the right value for the engine.

> 4 ticks = 1 frame at 60 FPS (the game runs at 240 ticks per second).

### Installation

1. Install [Geode](https://geode-sdk.org/) for Geometry Dash 2.2081.
2. Copy `alexmera.predictivebufferhud.geode` (engine + menu, from the release or the GitHub Actions artifact) into your `geode/mods` folder. It is the only file you need. If you have the old `ai.predictivebuffer` mod, delete it.
3. Start the game. The menu appears on the top left of the screen, on every screen and inside levels. Language is English by default; change it in *Interface → Language*.

### Usage

| Action | How |
| --- | --- |
| Show / hide the whole menu | `Left Ctrl` (changeable) |
| Turn the buffer on / off | Click *Buffer active* (or press `F6` / your own key) |
| Turn orbs & pads / moving objects prediction on / off | *Buffer* window → click *Orbs & pads* or *Moving objs* |
| Change the menu key | *Interface* window → click the key row, then press the new key |
| Change ticks for a vehicle | Click ▲ / ▼ next to the number in the *Ticks* window |
| Move a window | Drag its title bar |
| Fold / unfold a window | Click its title bar (without dragging) |
| Change colors | *Interface* window → click *Main color* or *Gray color* → use the wheel and the slider. Click the same row again, or *Close*, to close the wheel |
| Reset colors | *Interface* window → *Reset colors* |
| Change menu size | *Interface* window → arrows next to *Scale* (0.75x to 2x) |
| Change language | *Interface* window → click *Language* to switch between English and Español (English by default) |

Windows:

- **Buffer:** on/off, *Orbs & pads* and *Moving objs* switches, current vehicle, active ticks, level name and progress.
- **Ticks:** value for each vehicle.
- **Buffered click:** last click, statistics and history.
- **Interface:** colors, reset, scale and language.

### Building from source

**With GitHub Actions (no local setup):**

1. Push this repository to GitHub (branch `main`).
2. Open the **Actions** tab and run **Build Predictive Buffer HUD** (it also runs on every push).
3. When it finishes, download the `alexmera.predictivebufferhud` artifact. Inside is the `.geode` file.

> The mod is built from `src/main.cpp` (HUD) `src/PredictiveBuffer.cpp` (buffer engine) and `src/Motion.hpp` (orbs / moving objects); CMake compiles every `src/*.cpp`.

**Locally:**

1. Install the [Geode CLI and SDK](https://docs.geode-sdk.org/getting-started/).
2. In the project folder run:

```
geode build
```

### Repository layout

```
src/main.cpp                      HUD source code (with English / Spanish texts)
src/PredictiveBuffer.cpp          buffer engine (hooks and trajectory simulation)
src/PredictiveBuffer.hpp          state shared between engine and HUD
src/Motion.hpp                    Move Trigger reading and moving-object prediction
mod.json                          Geode metadata
CMakeLists.txt                    build configuration
.github/workflows/build.yml       automatic build on GitHub
```

### Troubleshooting

- **The buffer does nothing / the game shows a conflict:** make sure the old `ai.predictivebuffer` mod is NOT installed next to this one; remove it and restart the game.
- **I don't see the menu:** press `Left Ctrl` once; it may be hidden. Also check the mod is enabled in Geode.
- **The buffer blocks or ignores a click near an orb / a moving object is predicted wrong:** turn off *Orbs & pads* or *Moving objs* in the *Buffer* window to compare. To tune how close a click's path must pass to an orb before the buffer skips it, change `ORB_PATH_RADIUS` in `src/PredictiveBuffer.cpp`.
- **The menu is in the wrong language:** *Interface* window → *Language*. The choice is saved.
- **A window ended up off-screen or in a bad place:** set the scale to a smaller value, or drag it back by its title bar. Windows are kept inside the screen when you release them.
- **The ticks I set in an older version are gone:** older versions stored them in Geode's settings; now they are stored by the menu itself (default value 4).
- **The build fails:** open the failed step in Actions and check the compiler error. SDK versions change; if the error mentions a Geode function, the SDK may have changed.

### Notes

- Windows only.
- The orb and moving-object prediction is new and still in testing; see the limits listed under *Features*.
- Use at your own risk. Check the rules of any leaderboard or list before using a buffer mod on a record run.

### Credits

- Predictive Input Buffer engine (formerly the separate `ai.predictivebuffer` mod) by **AlexMera**.
- Built with the [Geode SDK](https://geode-sdk.org/).
- Visual style inspired by the Mega Hack v9 menu.

---

## Español

[English](#english) | **Español**

Menú dentro del juego, al estilo Mega Hack, para el mod de Geode **Predictive Input Buffer** (Geometry Dash, Windows). Permite elegir cuántos ticks de buffer tiene cada vehículo, activar y desactivar el buffer con un click, ver en directo los clicks bufferizados y cambiar los colores del menú con una rueda de colores. El menú está disponible en **inglés (por defecto)** y **español**.

- **ID del mod:** `alexmera.predictivebufferhud`
- **Geometry Dash:** 2.2081 (Windows)
- **Geode:** 5.11.2
- **Requiere:** nada más. El motor del Predictive Input Buffer ya va dentro de este mod. **Desinstala `ai.predictivebuffer` si lo tienes** (los dos chocan).

### Características

- **Ventanas estilo Mega Hack:** barra de título, cuerpo gris, texto blanco, esquinas redondeadas, sombra suave y bordes que siguen el color elegido.
- **Ticks por vehículo:** Cubo, Nave, UFO, Bola, Wave, Robot, Araña y Swing tienen cada uno su propio valor. Usa las dos flechas a la izquierda del número: ▲ sube y ▼ baja.
  - Escalones: `0 → 2 → 4 → 8 → 16 → 24 → 32 → 48 → 54 → 60` (0 = sin buffer en ese vehículo).
  - La fila del vehículo que estás jugando se resalta.
- **Activar con un click:** la fila *Buffer activo* enciende y apaga el buffer con un solo click. Cuando está encendido, la fila se ilumina con el color principal. `F6` también funciona y puedes elegir tu propia tecla (por defecto `F7`).
- **Teclas configurables:** haz click en la fila *L-Ctrl: ocultar menu* de la ventana *Interfaz* y pulsa cualquier tecla para cambiar la tecla del menú. La tecla para activar/desactivar el buffer se cambia igual. Ambas se guardan para siempre.
- **Información de clicks bufferizados:** último click (PRESS / RELEASE), ticks ahorrados, frames y milisegundos, total / media / máximo y un pequeño historial.
- **Orbes, pads y objetos móviles (v2.0.2):** la predicción ahora simula orbes y pads (amarillo, rosa, rojo, gravedad, verde, dash, araña) y lee los **Move Trigger** (activados por posición, con su duración y easing) para saber dónde estarán los pinchos y bloques que se mueven. El buffer sigue funcionando cerca de orbes; solo se salta un click cuando su propia trayectoria pasa justo por un orbe sin usar (el click puede ser para el orbe). Se pueden desactivar en la ventana *Buffer* (*Orbes y pads*, *Obj. moviles*) si algo falla en un nivel.
  - No se modela: mover a objetivo / bloqueo a jugador o cámara / movimiento dinámico, triggers por toque o spawn, rotar / seguir / escalar, portales ni cambios de velocidad dentro de la predicción. En esos casos el buffer funciona como antes.
- **Colores personalizados:** el color principal (azul por defecto) y el color gris (gris por defecto) se cambian con una rueda de colores propia y un deslizador de brillo. Un botón restablece los valores por defecto.
- **Selector de idioma:** cambia todo el menú entre **inglés** (por defecto) y **español** desde la ventana *Interfaz*.
- **Animaciones cortas:** las ventanas se pliegan y se despliegan en una fracción de segundo.
- **Todo se recuerda:** posición de las ventanas, si están plegadas, ticks, colores, escala e idioma se guardan entre sesiones.

### Cómo funciona

El mod contiene el motor del Predictive Input Buffer (de AlexMera) y el HUD en el mismo `.geode`. Se usan dos límites: uno cuando eres wave y otro para el resto de vehículos. El HUD detecta tu vehículo actual en cada frame y fija el valor correcto para el motor.

> 4 ticks = 1 frame a 60 FPS (el juego va a 240 ticks por segundo).

### Instalación

1. Instala [Geode](https://geode-sdk.org/) para Geometry Dash 2.2081.
2. Copia `alexmera.predictivebufferhud.geode` (motor + menú, del release o del artefacto de GitHub Actions) en tu carpeta `geode/mods`. Es el único archivo que necesitas. Si tienes el mod antiguo `ai.predictivebuffer`, bórralo.
3. Abre el juego. El menú aparece arriba a la izquierda, en todas las pantallas y dentro de los niveles. El idioma por defecto es inglés; cámbialo en *Interfaz → Idioma*.

### Uso

| Acción | Cómo |
| --- | --- |
| Mostrar / ocultar todo el menú | `Ctrl izquierdo` (configurable) |
| Activar / desactivar el buffer | Click en *Buffer activo* (o pulsa `F6` / tu propia tecla) |
| Activar / desactivar la predicción de orbes y pads / objetos móviles | Ventana *Buffer* → click en *Orbes y pads* o *Obj. moviles* |
| Cambiar la tecla del menú | Ventana *Interfaz* → click en la fila de la tecla y pulsa la nueva |
| Cambiar los ticks de un vehículo | Click en ▲ / ▼ junto al número en la ventana *Ticks* |
| Mover una ventana | Arrastra su barra de título |
| Plegar / desplegar una ventana | Click en su barra de título (sin arrastrar) |
| Cambiar colores | Ventana *Interfaz* → click en *Color principal* o *Color gris* → usa la rueda y el deslizador. Vuelve a pulsar la misma fila, o *Cerrar*, para cerrar la rueda |
| Restablecer colores | Ventana *Interfaz* → *Restablecer colores* |
| Cambiar el tamaño del menú | Ventana *Interfaz* → flechas junto a *Escala* (de 0.75x a 2x) |
| Cambiar el idioma | Ventana *Interfaz* → click en *Idioma* para alternar entre inglés y español (inglés por defecto) |

Ventanas:

- **Buffer:** activar/desactivar, interruptores *Orbes y pads* y *Obj. moviles*, vehículo actual, ticks activos, nombre del nivel y progreso.
- **Ticks:** valor de cada vehículo.
- **Click bufferizado:** último click, estadísticas e historial.
- **Interfaz:** colores, restablecer, escala e idioma.

### Compilar desde el código

**Con GitHub Actions (sin instalar nada):**

1. Sube este repositorio a GitHub (rama `main`).
2. Abre la pestaña **Actions** y ejecuta **Build Predictive Buffer HUD** (también se ejecuta solo en cada push).
3. Cuando termine, descarga el artefacto `alexmera.predictivebufferhud`. Dentro está el archivo `.geode`.

> El mod se compila a partir de `src/main.cpp` (HUD) `src/PredictiveBuffer.cpp` (motor del buffer) y `src/Motion.hpp` (orbes / objetos móviles); CMake compila todos los `src/*.cpp`.

**En local:**

1. Instala el [Geode CLI y el SDK](https://docs.geode-sdk.org/getting-started/).
2. En la carpeta del proyecto ejecuta:

```
geode build
```

### Estructura del repositorio

```
src/main.cpp                      código fuente del HUD (con textos en inglés / español)
src/PredictiveBuffer.cpp          motor del buffer (hooks y simulación de trayectoria)
src/PredictiveBuffer.hpp          estado compartido entre motor y HUD
src/Motion.hpp                    lectura de Move Trigger y predicción de objetos móviles
mod.json                          datos del mod para Geode
CMakeLists.txt                    configuración de compilación
.github/workflows/build.yml       compilación automática en GitHub
```

### Solución de problemas

- **El buffer no hace nada / el juego avisa de un conflicto:** comprueba que el mod antiguo `ai.predictivebuffer` NO esté instalado junto a este; bórralo y reinicia el juego.
- **No veo el menú:** pulsa `Ctrl izquierdo` una vez, puede estar oculto. Comprueba también el mod esté activado en Geode.
- **El buffer bloquea o ignora un click cerca de un orbe / un objeto móvil se predice mal:** desactiva *Orbes y pads* u *Obj. moviles* en la ventana *Buffer* para comparar. Para ajustar a qué distancia debe pasar la trayectoria de un click de un orbe para que el buffer se lo salte, cambia `ORB_PATH_RADIUS` en `src/PredictiveBuffer.cpp`.
- **El menú está en otro idioma:** ventana *Interfaz* → *Idioma*. La elección se guarda.
- **Una ventana quedó fuera de la pantalla o en mal sitio:** baja la escala o arrástrala de vuelta por su barra de título. Las ventanas se mantienen dentro de la pantalla al soltarlas.
- **Los ticks que puse en una versión anterior han desaparecido:** las versiones antiguas los guardaban en los ajustes de Geode; ahora los guarda el propio menú (valor por defecto 4).
- **La compilación falla:** abre el paso fallido en Actions y mira el error del compilador. Las versiones del SDK cambian; si el error menciona una función de Geode, puede que el SDK haya cambiado.

### Notas

- Solo Windows.
- La predicción de orbes y objetos móviles es nueva y sigue en pruebas; mira los límites indicados en *Características*.
- Úsalo bajo tu responsabilidad. Revisa las reglas de cualquier lista o ranking antes de usar un mod de buffer en un récord.

### Créditos

- Motor del Predictive Input Buffer (antes el mod separado `ai.predictivebuffer`) de **AlexMera**.
- Hecho con el [Geode SDK](https://geode-sdk.org/).
- Estilo visual inspirado en el menú de Mega Hack v9.
