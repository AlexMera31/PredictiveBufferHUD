# Predictive Buffer HUD v1.3 (menu estilo Mega Hack)

## Que cambia en la v1.2
- Menu con ventanas como las de Mega Hack: barra de titulo de color, cuerpo gris, texto blanco.
- **Ticks por vehiculo desde el menu**: numero con dos flechas a la izquierda (arriba / abajo).
  Escalon igual que el F6 original: 0 - 2 - 4 - 8 - 16 - 24 - 32 - 48 - 54 - 60.
- **Buffer activo**: se activa y desactiva con un click (la fila se pone del color principal). F6 sigue funcionando.
- **Colores** (ventana "Interfaz"): color principal (azul por defecto) y color gris (gris por defecto), con rueda de colores propia (matiz + saturacion + brillo). Boton para restablecer.
- Click en la barra de titulo = plegar / desplegar. Arrastrar la barra = mover la ventana. Se guarda posicion, colores y ticks.
- **L-Ctrl** oculta / muestra todo el menu. Escala del menu ajustable (0.75x a 2x).

## Diseno (v1.3)
- Ventanas con esquinas redondeadas, sombra suave y borde del color principal; barra de titulo con brillo.
- Filas activas con borde y brillo del color principal; muestras de color y separadores finos.
- Animacion muy corta (0.14 s) al plegar / desplegar ventanas, al abrir la rueda de color y al mostrar el menu con L-Ctrl.
- Todo se redibuja con los colores que elijas (principal y gris).

## No hace falta tocar el .geode original
El HUD escribe cada frame los limites "All" y "Wave" en la memoria de `ai.predictivebuffer.dll`,
asi que la DLL original queda igual. Solo hay que recompilar este mod.

## Compilar (el codigo NO esta compilado ni probado en el juego)
- **Local:** Geode CLI + SDK instalados, y en esta carpeta: `geode build`
- **GitHub:** sube la carpeta; `.github/workflows/build.yml` genera el .geode como artefacto.

## Instalar
En `geode/mods` pon los dos .geode:
1. `base_mod/PredictiveBuffer_noconsole.geode` (mod original sin consola)
2. `lilwc.predictivebufferhud.geode` (este menu)

## Notas
- Los ticks que tenias en los ajustes de Geode (v1.1) ya no se usan: ahora se guardan desde el menu (por defecto 4 en todos).
- Si cambia la DLL base, el menu muestra "SIN DATOS" en vez de leer memoria equivocada.
- La rueda de colores es propia (dibujada con nodos basicos de cocos), no depende del ColorPickPopup del SDK.
