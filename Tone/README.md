# Proyecto: Tone Mapping y Procesamiento HDR (Fase 2D)


## 1. Objetivos
El objetivo principal de este proyecto fue comprender y replicar el pipeline de **Tone Mapping** visto en el material del curso (Imaging - Tone):
*   Entender el formato de **Rango Dinámico (HDR a LDR)** en imágenes y escenas generadas.
*   Implementar un pipeline de post-procesado *pointwise* (píxel por píxel) mediante un *Fullscreen Quad* en OpenGL.
*   Implementar y comparar los tres operadores de tone mapping vistos en clase: **Clip**, **Reinhard** y **ACES**.
*   Extender la aplicación para cargar imágenes reales en formato **HDR** (`.hdr`) y **LDR** (`.jpg`, `.png`), además de mantener la escena sintética del material original, con el fin de entender las diferencias técnicas entre ambos formatos.
*   Desarrollar los controles de usuario necesarios para alterar la exposición, el operador, el contraste y la temperatura en tiempo real.

## 2. Decisiones de Diseño
*   **Dominio y Capítulos:** El proyecto se enfoca en el dominio de **Imaging** (Procesamiento de imagen), específicamente en el capítulo de **Tone** (Tone Mapping). Se descartó la fase 3D para enfocar todos los esfuerzos en entender la teoría del color y los formatos de datos.
*   **Adaptación del Host:** El código original del curso estaba diseñado para OpenGL ES 3.0 (usando la librería `vc.hpp` y shaders en GLSL ES). Para asegurar la compatibilidad en Ubuntu (con drivers de escritorio estándar), se adaptó el host a **OpenGL Core 3.3**. Se construyó una nueva capa `vc.hpp` (matrices y wrappers de OpenGL) y se adaptaron los shaders a `#version 330 core`, eliminando el uso de `precision highp float`.
*   **Fuentes de Datos (El Selector):** Para hacer el programa versátil, el *Fragment Shader* (`tone.frag`) incluye un selector (`uUseTexture`). Esto nos permite alternar en tiempo real entre tres fuentes:
    1.  **Escena Sintética:** Generada proceduralmente en el shader (sol con intensidad `40.0`, destellos de `16.0`).
    2.  **Imagen HDR:** Cargada con `stbi_loadf()` (flotantes de 32 bits) y subida a la GPU como `GL_RGB16F`.
    3.  **Imagen LDR:** Cargada con `stbi_load()` (bytes de 8 bits) y subida como `GL_RGB8`.
*   **Manejo de Formatos (HDR vs LDR):** Entendimos que un monitor convencional solo muestra 8 bits (rango `[0.0, 1.0]`). Para que el HDR funcione, la GPU debe mantener los datos en un *Framebuffer* flotante (`GL_RGB16F`) o en una textura con este formato, de lo contrario, OpenGL clampea el color a `1.0` antes de que el Tone Mapping pueda actuar. Para las imágenes LDR, se aplicó una multiplicación artificial (`L *= 10.0`) en el shader para simular un alto rango dinámico.
*   **Pipeline Pointwise:** Se implementó la cadena exacta del curso: `HDR Scene -> Exposure -> Tone Operator -> Grade -> Display (Gamma)`.

### LDR (Low Dynamic Range) - `.jpg`, `.png`
*   **Representación:** Utilizan 8 bits por canal (0 a 255). Su rango dinámico está limitado matemáticamente al intervalo `[0.0, 1.0]`.
*   **Naturaleza:** Son imágenes *display-referred* (diseñadas para ser mostradas directamente en pantalla). Suelen tener aplicada una corrección de gamma (sRGB) para compensar la no linealidad del ojo humano.
*   **Carga en C++:** Se utiliza `stbi_load()`, que devuelve un arreglo de `unsigned char*` (bytes).
*   **Uso en el Proyecto:** Como no contienen información de alta luminosidad, en el shader (`tone.frag`) se aplicó una multiplicación artificial (`L *= 10.0`) para "simular" un HDR y forzar a los operadores a comprimir los brillos.

### HDR (High Dynamic Range) - `.hdr`, `.exr`
*   **Representación:** Utilizan 16 o 32 bits flotantes por canal (puntos flotantes IEEE 754). Su rango dinámico es inmenso (teóricamente infinito, pero limitado en la práctica a valores muy altos, ej. `1000.0`).
*   **Naturaleza:** Son imágenes *scene-referred* (capturan la radiancia real de la escena). Almacenan el valor de la luz de forma **lineal**, sin corrección de gamma.
*   **Carga en C++:** Se utiliza `stbi_loadf()` (la `f` significa *float*), que devuelve un arreglo de `float*`. Esto permite leer valores enteros como `45.0` o `120.5`.
*   **Uso en el Proyecto:** Se cargan las imágenes descargadas de Poly Haven. En el shader, el valor leído (`texture(uScene, uv).rgb`) se pasa directamente al *Exposure*, que multiplicará esta radiancia por potencias de 2.

## 3. Resultados y Conceptos Clave
El programa final permite visualizar claramente por qué necesitamos el Tone Mapping:
*   **El Operador (La Curva):** 
    *   *Clip* (`clamp`): Demostró ser el más rápido pero destruye la información. El sol sintético se vuelve un disco blanco plano.
    *   *Reinhard* (`L / (1 + L)`): Una curva asintótica que suaviza el brillo, pero oscurece un poco los tonos medios.
    *   *ACES* (Polinomio de Narkowicz): La curva que mejor se comporta. Aplana las zonas extremadamente brillantes y mantiene el contraste en las zonas medias.
*   **Exposure y Grade:** El uso de `exp2(uExposure)` simula los "stops" de una cámara fotográfica.
*   **Corrección Gamma:** Fundamental para que los valores de luz lineal se vean correctos en el monitor.
*   **El Flip Vertical:** Se requirió usar `stbi_set_flip_vertically_on_load(true)` porque los archivos de imagen (JPG, HDR) guardan la fila superior primero, mientras que OpenGL espera la fila inferior primero en los buffers de textura.

## 4. Trabajo Futuro
*   **Bloom:** El paso natural siguiente es implementar un desenfoque de los píxeles más brillantes (Bloom).
*   **Interfaz Gráfica (ImGui):** Reemplazar el control por teclado por un panel de *Sliders* reales en pantalla, tal como se ve en la página del curso.


## 5. Manual de Uso y Compilación

### Requisitos e Instalación
Dependencias necesarias en Ubuntu:

sudo apt update
sudo apt install build-essential libglfw3-dev libglew-dev libgl1-mesa-dev

Clonar el repositorio y compilar main_tone.cpp:

g++ -std=c++17 main_tone.cpp -lglfw -lGLEW -lGL -o tone

Ejecutar:

* Opción 1: Iniciar con la escena sintética
XDG_SESSION_TYPE=x11 ./tone

* Opción 2: Iniciar con una imagen HDR
XDG_SESSION_TYPE=x11 ./tone imagen.hdr

* Opción 3: Iniciar con una imagen LDR
XDG_SESSION_TYPE=x11 ./tone foto.jpg


## 6. Controles de Aplicación

| Tecla | Acción |
| :--- | :--- |
| **↑ / ↓** | Subir / Bajar la exposición (stops) |
| **1 / 2 / 3** | Cambiar operador: 1=Clip, 2=Reinhard, 3=ACES |
| **→ / ←** | Subir / Bajar el contraste (Grade) |
| **W / S** | Ajustar la temperatura de color (Cálido / Frío) |
| **TAB** | Cambiar entre la escena/imagen y el gráfico de las curvas |
| **T** | Alternar entre la imagen cargada y la escena sintética (si hay imagen) |
| **R** | Resetear todos los valores |
| **ESC** | Cerrar la ventana |

## 7. Uso de Inteligencia Artificial en el Desarrollo

Durante la construcción de este proyecto, se uso un asistente de Inteligencia Artificial Deepseek como una herramienta de apoyo para la traducción de conceptos, la depuración de errores y la comprensión profunda de la API de OpenGL en C++. Es fundamental destacar que la IA no reemplazó el entendimiento del alumno, la teoría necesaria (Tone Mapping, operadores ACES/Reinhard, y la escena sintética) provino directamente del material del curso y fue estudiada previamente.

## 8. Referencias

1. Charalambos, J. P. Visual Computing Notebook. https://visualcomputing.github.io/docs/
2. **Poly Haven** (https://polyhaven.com/): Fuente de las imágenes HDRI (`.hdr`) utilizadas para probar el alto rango dinámico.
3. **Reinhard, E., Stark, M., Shirley, P. & Ferwerda, J.** (2002). *Photographic Tone Reproduction for Digital Images*. ACM Transactions on Graphics. (Base matemática del operador Reinhard).
4. **Narkowicz, K.** (2016). *ACES Filmic Tone Mapping Curve*. (Implementación del ajuste polinomial para ACES).
5. **stb_image.h:** Librería header-only de Sean Barrett para carga de imágenes HDR y LDR.
6. **OpenGL / GLEW / GLFW Documentation:** Para la configuración de los contextos OpenGL Core 3.3 y el manejo de ventanas.
