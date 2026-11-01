# KIsland 🏝️

> **Dynamic Island & Live Activities al estilo Apple para el panel de KDE Plasma 6.8 sobre Wayland en CachyOS Linux.**

KIsland transforma el centro del panel superior de KDE Plasma 6 en una **Píldora Dinámica interactiva** con estética **Liquid Glass**:
- **MPRIS2 Media & Live Waveform (PipeWire)**: Detecta automáticamente cualquier reproductor multimedia en reproducción (Spotify, Elisa, Firefox, VLC), muestra título/artista y renderiza un visualizador de onda reactivo en tiempo real capturando el monitor del sumidero por defecto de PipeWire (`stream.capture.sink=true`, respetando EasyEffects).
- **Live Activities de Kiri & Temporizadores**: Sincroniza recordatorios/temporizadores activos de Kiri (`~/.local/share/kiri/reminders.json`) y permite crear temporizadores rápidos desde la isla.
- **Telemetría de Sistema y Batería (UPower / CachyOS)**: Muestra estado de carga/batería o actividad de CPU/RAM al expandir la isla.

---

## 🚀 Compilación e Instalación

```bash
cmake -B build -S . -DCMAKE_INSTALL_PREFIX=$HOME/.local -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
cmake --install build
```
