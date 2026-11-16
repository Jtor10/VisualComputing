# Proyecto Bloom y Simulación de Enfermedades Visuales (Fase 2D)

Tras dominar el Tone Mapping (operaciones pointwise), el objetivo aquí fue adentrarnos en las operaciones de vecindario (convolución y desenfoques) para implementar el efecto de Bloom, y utilizarlo como base para simular dos enfermedades ópticas reales el astigmatismo y las cataratas.

## 1. Objetivos
El objetivo principal fue comprender cómo los desenfoques y la dispersión de la luz afectan la percepción visual. Los objetivos específicos fueron:
   Comprender el Bloom: Implementar el Bright Pass y Multi-scale Blur en un pipeline HDR.
   Simulación Médica: Utilizar el Bloom y desenfoques direccionales para emular cómo el ojo humano percibe la luz cuando la córnea o el cristalino presentan anomalías.

## 2. Decisiones de Diseño
   ¿Por qué HDR? Todo el pipeline trabaja en formato de punto flotante (`RGBA16F`). El Bloom es físicamente imposible en un espacio LDR (0.0 a 1.0), ya que necesita que la lámpara de la escena tenga una intensidad mayor a `1.0` (HDR) para poder obtener los resultados buscados.
   Bloom vs. Enfermedades Visuales 
       El Bloom se genera con una pirámide de desenfoques. En lugar de aplicar un desenfoque gigante (debido a la limitación de recursos), extraemos el brillo y lo desenfocamos en 5 niveles de resolución (12, 14, 18, 116, 132). Al sumarlos, se crea un halo suave y de largo alcance.
       El Astigmatismo se simula con un desenfoque Gaussiano direccional (1D). Las líneas verticales se mantienen nítidas, pero las horizontales se difuminan. Además, simulamos la aberración cromática haciendo que el canal azul se desenfoque más que el rojo.
       Las Cataratas son una combinación de efectos. Se añade un desenfoque general, se baja el umbral del Bloom a casi `0.0` (creando un velo de luz que baja el contraste), se aplica un tinte amarillento y se generan opacidades procedurales (manchas y rayos radiales).

## 3. Pipeline
El render se compone de las siguientes etapas ejecutadas en Framebuffers
1.  `page.frag` (Escena Base) Dibuja una página de libro sobre un escritorio oscuro, con una lámpara muy brillante (valores HDR) en una esquina.
2.  `bright.frag` (Bright Pass) Filtra la imagen, dejando pasar solo los píxeles que superan un umbral de brillo (la lámpara).
3.  `blur.frag`  `astigmatism.frag` (Desenfoque) Se genera la pirámide de desenfoque. Si la enfermedad está activa, se aplica el desenfoque asimétrico y cromático.
4.  `composite.frag` (Composición Final) Mezcla la escena original con el Bloom. Luego, aplica los efectos de catarata, el Tone Mapping (ACES), la viñeta y la corrección de Gamma.

## 4. Trabajo Futuro
   Diplopía Monocular Simular la visión doble (común en cataratas avanzadas) renderizando la escena dos veces con un pequeño desplazamiento.
   Rayos de Estrella (Starbursts) Añadir patrones de difracción (cruces de luz) alrededor de la lámpara, típicos de lentes sucios o cataratas.
   Animación de Pupila Simular cómo la pupila se contrae (midriasis) al mirar directamente a la lámpara, lo que cambia el nivel de exposición global.

## 5. Manual de Uso y Compilación

### Requisitos e Instalación
Dependencias necesarias en Ubuntu
```bash
sudo apt update
sudo apt install build-essential cmake libglfw3-dev libglew-dev libstb-dev mesa-utils
```

### Compilación
```bash
cmake -B build && cmake --build build -j
```

### Ejecución
GLEW requiere el protocolo X11 en el entorno en que ha sido probado. Además, para forzar el renderizado por CPU en equipos sin GPU, se usa `LIBGL_ALWAYS_SOFTWARE=1`.

```bash
XDG_SESSION_TYPE=x11 LIBGL_ALWAYS_SOFTWARE=1 .buildbloomvision assetspage.png --scale 0.6
```

## 6. Controles de Aplicación

 Tecla  Acción 
 ---  --- 
 1  2  3  ActivarDesactivar Astigmatismo  Catarata  Bloom 
 O  Ver original (visión normal), para comparar 
 L  EncenderApagar la lámpara de la escena 
 ←  →  Girar el eje del astigmatismo (±5°) 
 ↑  ↓  Aumentar  Disminuir la fuerza del astigmatismo 
 Z  X  Disminuir  Aumentar la severidad de la catarata 
 V  B  Disminuir  Aumentar la intensidad del bloom 
 N  M  Bajar  Subir el umbral del bloom (qué tanto brilla) 
 D  E  Bajar  Subir la exposición (stops) 
 Rueda  Arrastrar  Zoom  Mover la página 
 P  R  Esc  Tomar captura (PPM)  Reiniciar  Salir 

## 7. Uso de Inteligencia Artificial en el Desarrollo

Al igual que en el proyecto anterior, se utilizó un asistente de Inteligencia Artificial (DeepSeek) para la depuración de errores y la comprensión técnica. Para este proyecto en específico, la IA fue fundamental para
   Estructurar el proyecto: Ayudar a refactorizar el código monolítico en una arquitectura modular con CMake.
   Optimización de CPU: Entender cómo funciona la pirámide de desenfoques para no colapsar la CPU y cómo implementar el `glfwWaitEvents()` para renderizar solo bajo demanda.
   Ayudar a desglosar los síntomas del astigmatismo (desenfoque direccional y aberración cromática) y las cataratas (velo de luz, opacidad) en fórmulas matemáticas aplicables en GLSL.

## 8. Referencias

1.  **Charalambos, J. P.** Visual Computing Notebook. https://visualcomputing.github.io/docs/
2.  **Narkowicz, K.** (2016). ACES Filmic Tone Mapping Curve.
3.  **Halle, M. & Benton, S.** (2003). *Understanding Astigmatism*. MIT OpenCourseWare, MAS.450 Holographic Imaging, Spring 2003. https://ocw.mit.edu/courses/mas-450-holographic-imaging-spring-2003/a839932f0316397f5afc48a1f0cd767a_understandingastigmatism.pdf
4. **Mayo Clinic Staff.** (2026, 13 de marzo). *Cataracts - Symptoms and causes*. Mayo Clinic. https://www.mayoclinic.org/diseases-conditions/cataracts/symptoms-causes/syc-20353790