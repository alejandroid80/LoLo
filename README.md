# LoLo — Asistente IA para TrimUI Brick (NextUI)

Tool Pak con cara de robot emoji (solo ojos y boca, colores pastel) que consulta
modelos gratuitos (OpenRouter free por defecto; fallback a Pollinations sin API key).

## Compilar

Requiere Docker con la imagen `ghcr.io/loveretro/tg5040-toolchain:latest`.

```sh
make tg5040
```

El binario queda en `LoLo.pak/bin/lolo`.

## Instalar

Copia la carpeta `LoLo.pak` a `Tools/tg5040/` en la SD card.

## Compilar en la nube (GitHub Actions)

1. Sube este proyecto a un repositorio de GitHub.
2. Ve a **Actions** → **Build LoLo (tg5040)** → **Run workflow**.
3. Descarga el artefacto `LoLo.pak` (incluye `LoLo.pak.zip`).
4. Extrae `LoLo.pak` y cópialo a `Tools/tg5040/` en la SD card.

## Uso

- A: escribir pregunta (teclado en pantalla)
- B: volver / salir
- MENU: ajustes (proveedor, modelo, API key, prompt, historial)

Configura tu API key de OpenRouter en Ajustes. Sin key, usa Pollinations (limitado por IP).