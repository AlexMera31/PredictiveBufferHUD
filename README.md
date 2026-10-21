# Predictive Buffer With HUD

**English** | [Español](#español)

An in-game, Mega Hack–style menu for the **Predictive Input Buffer** Geode mod (Geometry Dash, Windows). It lets you set how many ticks of buffer each vehicle gets, turn the buffer on and off with a click, watch the buffered clicks live, and customize the menu colors with a color wheel.

- **Mod ID:** `lilwc.predictivebufferhud`
- **Geometry Dash:** 2.2081 (Windows)
- **Geode:** 5.11.2
- **Requires:** the base mod `ai.predictivebuffer` (a ready-to-use build is included in `base_mod/`)

---

## English

### Features

- **Mega Hack–style windows:** title bar, gray body, white text, rounded corners, soft shadow, and borders that follow your color theme.
- **Ticks per vehicle:** Cube, Ship, UFO, Ball, Wave, Robot, Spider and Swing each have their own value. Use the two arrows on the left of the number: ▲ goes up, ▼ goes down.
  - Steps: `0 → 2 → 4 → 8 → 16 → 24 → 32 → 48 → 54 → 60` (0 = no buffer for that vehicle).
  - The row of the vehicle you are currently playing is highlighted.
- **Click to toggle:** the *Buffer active* row turns the buffer on and off with a single click. The row lights up in the main color when it is on. `F6` (from the base mod) also works.
- **Buffered click info:** last click (PRESS / RELEASE), ticks saved, frames and milliseconds, total / average / maximum, and a short history.
- **Custom colors:** the main color (blue by default) and the gray color (gray by default) can be changed with a built-in color wheel plus a brightness slider. A button restores the defaults.
- **Short animations:** windows fold and unfold in a fraction of a second.
- **Everything is remembered:** window positions, folded state, ticks, colors and interface scale are saved between sessions.

### How it works

The base mod keeps two limits in memory: one used while you are a wave and one for every other vehicle. This HUD detects your current vehicle every frame and writes the right value into those two limits. The base mod itself is not modified, which is why you only need to rebuild the HUD when you change something here.

> 4 ticks = 1 frame at 60 FPS (the game runs at 240 ticks per second).

### Installation

1. Install [Geode](https://geode-sdk.org/) for Geometry Dash 2.2081.
2. Copy both files into your `geode/mods` folder:
   - `base_mod/PredictiveBuffer_noconsole.geode` (base mod, without the console window)
   - `lilwc.predictivebufferhud.geode` (this menu)
3. Start the game. The menu appears on the top left of the screen, on every screen and inside levels.

### Usage

| Action | How |
| --- | --- |
| Show / hide the whole menu | `Left Ctrl` |
| Turn the buffer on / off | Click *Buffer active* (or press `F6`) |
| Change ticks for a vehicle | Click ▲ / ▼ next to the number in the *Ticks* window |
| Move a window | Drag its title bar |
| Fold / unfold a window | Click its title bar (without dragging) |
| Change colors | *Interface* window → click *Main color* or *Gray color* → use the wheel and the slider. Click the same row again, or *Close*, to close the wheel |
| Reset colors | *Interface* window → *Reset colors* |
| Change menu size | *Interface* window → arrows next to *Scale* (0.75x to 2x) |

Windows:

- **Buffer:** on/off, current vehicle, active ticks, level name and progress.
- **Ticks:** value for each vehicle.
- **Buffered click:** last click, statistics and history.
- **Interface:** colors, reset and scale.

### Building from source

**With GitHub Actions (no local setup):**

1. Push this repository to GitHub (branch `main`).
2. Open the **Actions** tab and run **Build Predictive Buffer HUD** (it also runs on every push).
3. When it finishes, download the `lilwc.predictivebufferhud` artifact. Inside is the `.geode` file.

**Locally:**

1. Install the [Geode CLI and SDK](https://docs.geode-sdk.org/getting-started/).
2. In the project folder run:

```
geode build
```

### Repository layout

```
src/main.cpp                      HUD source code
mod.json                          Geode metadata
CMakeLists.txt                    build configuration
base_mod/                         base mod without the console window
patch_console.py                  script that removes the console from the base mod
.github/workflows/build.yml       automatic build on GitHub
```

### Troubleshooting

- **The menu says "NO DATA":** the base mod is not the expected build. Use the `.geode` from `base_mod/`; the HUD refuses to write to memory of an unknown version on purpose.
- **I don't see the menu:** press `Left Ctrl` once; it may be hidden. Also check that both mods are enabled in Geode.
- **A window ended up off-screen or in a bad place:** set the scale to a smaller value, or drag it back by its title bar. Windows are kept inside the screen when you release them.
- **The ticks I set in an older version are gone:** older versions stored them in Geode's settings; now they are stored by the menu itself (default value 4).
- **The build fails:** open the failed step in Actions and check the compiler error. SDK versions change; if the error mentions a Geode function, the SDK may have changed.

### Notes

- Windows only: the mod reads and writes the base mod's memory.
- Use at your own risk. Check the rules of any leaderboard or list before using a buffer mod on a record run.

### Credits

- Base mod **Predictive Input Buffer** (`ai.predictivebuffer`), included in `base_mod/`.
- Built with the [Geode SDK](https://geode-sdk.org/).
- Visual style inspired by the Mega Hack v9 menu.

---

## Español

[English](#english) | **Español**

Menú dentro del juego, al estilo Mega Hack, para el mod de Geode **Predictive Input Buffer** (Geometry Dash, Windows). Permite elegir cuántos ticks de buffer tiene cada vehículo, activar y desactivar el buffer con un click, ver en directo los clicks bufferizados y cambiar los colores del menú con una rueda de colores.

- **ID del mod:** `lilwc.predictivebufferhud`
- **Geometry Dash:** 2.2081 (Windows)
- **Geode:** 5.11.2
- **Requiere:** el mod base `ai.predictivebuffer` (hay una versión lista para usar en `base_mod/`)

### Características

- **Ventanas estilo Mega Hack:** barra de título, cuerpo gris, texto blanco, esquinas redondeadas, sombra suave y bordes que siguen el color elegido.
- **Ticks por vehículo:** Cubo, Nave, UFO, Bola, Wave, Robot, Araña y Swing tienen cada uno su propio valor. Usa las dos flechas a la izquierda del número: ▲ sube y ▼ baja.
  - Escalones: `0 → 2 → 4 → 8 → 16 → 24 → 32 → 48 → 54 → 60` (0 = sin buffer en ese vehículo).
  - La fila del vehículo que estás jugando se resalta.
- **Activar con un click:** la fila *Buffer activo* enciende y apaga el buffer con un solo click. Cuando está encendido, la fila se ilumina con el color principal. `F6` (del mod base) también funciona.
- **Información de clicks bufferizados:** último click (PRESS / RELEASE), ticks ahorrados, frames y milisegundos, total / media / máximo y un pequeño historial.
- **Colores personalizados:** el color principal (azul por defecto) y el color gris (gris por defecto) se cambian con una rueda de colores propia y un deslizador de brillo. Un botón restablece los valores por defecto.
- **Animaciones cortas:** las ventanas se pliegan y se despliegan en una fracción de segundo.
- **Todo se recuerda:** posición de las ventanas, si están plegadas, ticks, colores y escala se guardan entre sesiones.

### Cómo funciona

El mod base guarda dos límites en memoria: uno que se usa cuando eres wave y otro para el resto de vehículos. Este HUD detecta tu vehículo actual en cada frame y escribe el valor correcto en esos dos límites. El mod base no se modifica, por eso solo tienes que recompilar el HUD cuando cambies algo aquí.

> 4 ticks = 1 frame a 60 FPS (el juego va a 240 ticks por segundo).

### Instalación

1. Instala [Geode](https://geode-sdk.org/) para Geometry Dash 2.2081.
2. Copia los dos archivos en tu carpeta `geode/mods`:
   - `base_mod/PredictiveBuffer_noconsole.geode` (mod base, sin ventana de consola)
   - `lilwc.predictivebufferhud.geode` (este menú)
3. Abre el juego. El menú aparece arriba a la izquierda, en todas las pantallas y dentro de los niveles.

### Uso

| Acción | Cómo |
| --- | --- |
| Mostrar / ocultar todo el menú | `Ctrl izquierdo` |
| Activar / desactivar el buffer | Click en *Buffer activo* (o pulsa `F6`) |
| Cambiar los ticks de un vehículo | Click en ▲ / ▼ junto al número en la ventana *Ticks* |
| Mover una ventana | Arrastra su barra de título |
| Plegar / desplegar una ventana | Click en su barra de título (sin arrastrar) |
| Cambiar colores | Ventana *Interfaz* → click en *Color principal* o *Color gris* → usa la rueda y el deslizador. Vuelve a pulsar la misma fila, o *Cerrar*, para cerrar la rueda |
| Restablecer colores | Ventana *Interfaz* → *Restablecer colores* |
| Cambiar el tamaño del menú | Ventana *Interfaz* → flechas junto a *Escala* (de 0.75x a 2x) |

Ventanas:

- **Buffer:** activar/desactivar, vehículo actual, ticks activos, nombre del nivel y progreso.
- **Ticks:** valor de cada vehículo.
- **Click bufferizado:** último click, estadísticas e historial.
- **Interfaz:** colores, restablecer y escala.

### Compilar desde el código

**Con GitHub Actions (sin instalar nada):**

1. Sube este repositorio a GitHub (rama `main`).
2. Abre la pestaña **Actions** y ejecuta **Build Predictive Buffer HUD** (también se ejecuta solo en cada push).
3. Cuando termine, descarga el artefacto `lilwc.predictivebufferhud`. Dentro está el archivo `.geode`.

**En local:**

1. Instala el [Geode CLI y el SDK](https://docs.geode-sdk.org/getting-started/).
2. En la carpeta del proyecto ejecuta:

```
geode build
```

### Estructura del repositorio

```
src/main.cpp                      código fuente del HUD
mod.json                          datos del mod para Geode
CMakeLists.txt                    configuración de compilación
base_mod/                         mod base sin ventana de consola
patch_console.py                  script que quita la consola del mod base
.github/workflows/build.yml       compilación automática en GitHub
```

### Solución de problemas

- **El menú dice "SIN DATOS":** el mod base no es la versión esperada. Usa el `.geode` de `base_mod/`; el HUD se niega a escribir en la memoria de una versión desconocida a propósito.
- **No veo el menú:** pulsa `Ctrl izquierdo` una vez, puede estar oculto. Comprueba también que los dos mods estén activados en Geode.
- **Una ventana quedó fuera de la pantalla o en mal sitio:** baja la escala o arrástrala de vuelta por su barra de título. Las ventanas se mantienen dentro de la pantalla al soltarlas.
- **Los ticks que puse en una versión anterior han desaparecido:** las versiones antiguas los guardaban en los ajustes de Geode; ahora los guarda el propio menú (valor por defecto 4).
- **La compilación falla:** abre el paso fallido en Actions y mira el error del compilador. Las versiones del SDK cambian; si el error menciona una función de Geode, puede que el SDK haya cambiado.

### Notas

- Solo Windows: el mod lee y escribe la memoria del mod base.
- Úsalo bajo tu responsabilidad. Revisa las reglas de cualquier lista o ranking antes de usar un mod de buffer en un récord.

### Créditos

- Mod base **Predictive Input Buffer** (`ai.predictivebuffer`), incluido en `base_mod/`.
- Hecho con el [Geode SDK](https://geode-sdk.org/).
- Estilo visual inspirado en el menú de Mega Hack v9.
