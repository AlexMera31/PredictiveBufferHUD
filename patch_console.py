#!/usr/bin/env python3
"""
Quita la ventana de consola (AllocConsole) del mod ai.predictivebuffer.
Uso:  python patch_console.py PredictiveBuffer_extended.geode PredictiveBuffer_noconsole.geode

Parche: el primer byte de la funcion de init de la consola (VA 0x180003a10,
offset de archivo 0x2e10, byte original 0x56 = push rsi) pasa a 0xC3 (ret).
La logica del buffer NO se toca; solo deja de abrirse la consola y de
imprimirse el banner. El HUD dentro del juego lee el estado directamente.
"""
import sys, zipfile, json, re

src, dst = sys.argv[1], sys.argv[2]
OFF = 0x2E10
EXPECT = bytes.fromhex("564883ec30ff15d5750100".replace(" ", ""))  # push rsi; sub rsp,0x30; call [AllocConsole]

zin = zipfile.ZipFile(src)
with zipfile.ZipFile(dst, "w", zipfile.ZIP_DEFLATED) as zout:
    for item in zin.infolist():
        data = zin.read(item.filename)
        if item.filename == "ai.predictivebuffer.dll":
            assert data[OFF:OFF + len(EXPECT)] == EXPECT, "La DLL no es la version esperada; no se parchea."
            b = bytearray(data); b[OFF] = 0xC3; data = bytes(b)
        elif item.filename == "mod.json":
            t = data.decode("utf-8")
            t = t.replace('"v1.0.0"', '"v1.0.1"')
            json.loads(t)
            data = t.encode("utf-8")
        zout.writestr(item.filename, data)
print("OK ->", dst)
