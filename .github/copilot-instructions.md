# Project Guidelines

## Code Style
- Mantener los cambios de firmware pequeños, claros y consistentes con el estilo actual de `src/main.cpp`.
- Preservar comentarios, nombres descriptivos y mensajes seriales en español, salvo que la tarea pida cambiarlos.
- Declarar pines, umbrales y tiempos al inicio del archivo para que el hardware y la calibración se puedan revisar rápido.
- Preferir funciones cortas y de responsabilidad única. Si una función mezcla lectura, decisión y actuación, dividirla.
- Evitar abstracciones prematuras, herencia, clases complejas o nuevas librerías si una función simple resuelve el problema.
- Cuando un bloque empiece a reutilizarse o deba probarse por separado, extraerlo como módulo pequeño y fácil de validar.
- Codificar con una estructura que en el futuro sea fácil de migrar a un estilo tipo Arduino Blocks: funciones simples, flujo lineal, nombres explícitos, entradas y salidas claras, sin metaprogramación ni patrones avanzados difíciles de representar visualmente.
- Favorecer bloques de lógica pequeños y autocontenidos que puedan traducirse después a secuencias visuales de inicialización, lectura, decisión y acción.

## Architecture
- El proyecto usa PlatformIO con Arduino Uno. `platformio.ini` es la fuente de verdad para board, framework y puerto serial.
- La lógica principal vive en `src/main.cpp` mientras el proyecto siga siendo un sketch único.
- El hardware actual conocido es:
  - buzzer en pin 7
  - motor vibrador PWM en pin 6
  - sensor frontal HC-SR04 en pines 8/9
  - sensor de suelo HC-SR04 en pines 10/11
- Aún no existe una lógica final cerrada. El desarrollo debe avanzar por módulos independientes y pruebas incrementales antes de consolidar el comportamiento global.
- Si el sketch crece, separar primero por responsabilidad y no por complejidad artificial:
  - lectura de sensores
  - procesamiento o decisión
  - actuadores
  - diagnóstico serial

## Build And Test
- Usar `pio run` para compilar el firmware del entorno `uno`.
- Usar `pio run --target upload` solo cuando se quiera cargar a hardware real y la placa esté conectada.
- Usar `pio device monitor --baud 9600` para observar telemetría y resultados de pruebas manuales.
- Usar `pio test` solo cuando existan pruebas en `test/`; por ahora el flujo principal será validación incremental de módulos sobre hardware o sketches de prueba controlados.
- Tratar `.pio/` y casi todo `.vscode/` como artefactos generados.
- Cuando se pruebe un módulo, registrar en este archivo qué se probó, con qué entradas, qué resultado dio y si queda validado o pendiente.

## Conventions
- Preferir cambios en `platformio.ini` antes que editar archivos generados dentro de `.vscode/`.
- Mantener manejo defensivo de lecturas inválidas. En el código actual, `pulseIn()` con timeout devuelve `-1` cuando no hay eco; no eliminar esa protección sin reemplazo equivalente.
- Diseñar cada módulo para que sus entradas y salidas sean explícitas. Evitar dependencias ocultas y estados globales innecesarios.
- Antes de integrar lógica nueva en `loop()`, validar el módulo de forma aislada con una rutina simple, salida serial y criterios de aceptación concretos.
- Favorecer simplicidad sobre sofisticación: primero que funcione y sea medible, luego refinar.
- Tener presente las limitaciones del Arduino Uno: RAM reducida, CPU limitada, temporización bloqueante con `delay()` y PWM del motor en pin 6.
- Mientras no exista una estrategia final de tiempo real, `delay()` es aceptable en pruebas aisladas. Si más adelante se combinan varios módulos simultáneos, reevaluar migración parcial a lógica no bloqueante con `millis()`.
- Si se solicita probar o integrar un módulo y no se especifica su pinout, detener la implementación y pedir al usuario la configuración exacta de conexiones antes de asumir pines.
- Cuando falte el pinout o la forma de conexión de un módulo, orientar al usuario paso a paso: alimentación, pines de señal, tipo de entrada o salida, precauciones eléctricas y secuencia de prueba esperada.

## Desarrollo Modular
- Todo cambio nuevo debe encajar, idealmente, en uno de estos tipos de módulo:
  - entrada: lectura o captura de sensores
  - procesamiento: filtrado, normalización o cálculo de estado
  - decisión: reglas de riesgo, prioridad o activación
  - salida: buzzer, vibración, LEDs u otros actuadores
  - diagnóstico: mensajes seriales y trazas para pruebas
- Al crear un módulo nuevo, documentar en este archivo:
  - nombre del módulo
  - propósito
  - entradas y salidas
  - dependencias de hardware
  - pinout usado en la prueba
  - estado: pendiente, en prueba o validado
  - hallazgos relevantes de la prueba
- Si una prueba obliga a usar lógica temporal o umbrales temporales, dejarlo anotado aquí para distinguir prototipo de comportamiento definitivo.
- En cada prueba modular, indicar también el procedimiento mínimo que debe seguir el usuario para verificar funcionamiento correcto en hardware real.

## Interacción Para Pruebas
- Si el usuario pide trabajar con un módulo nuevo, primero confirmar nombre del módulo, objetivo de la prueba y pinout completo antes de escribir la lógica.
- Si el pinout no fue indicado, hacer la pregunta explícita antes de continuar, por ejemplo: alimentación, GND, pines de control, pines analógicos o digitales y si requiere resistencias, driver o fuente externa.
- Después de tener el pinout, proponer una prueba incremental y guiada paso a paso:
  - revisar cableado
  - confirmar alimentación correcta
  - cargar sketch mínimo de prueba
  - observar salida serial o respuesta física
  - comparar resultado esperado contra resultado observado
  - registrar hallazgos en este archivo
- No asumir que un módulo funciona igual que otro similar sin confirmación del usuario o evidencia de prueba.

## Registro De Módulos
- `Lectura HC-SR04`: propósito actual medir distancia en cm con timeout defensivo. Entradas: `trigPin`, `echoPin`. Salida: distancia o `-1`. Estado: en prueba.
- `Sensor frontal`: propósito actual detectar obstáculos al frente. Hardware: HC-SR04 en 8/9. Estado: en prueba.
- `Sensor de suelo`: propósito actual detectar cambios de distancia al piso o posibles desniveles. Hardware: HC-SR04 en 10/11. Estado: en prueba.
- `Buzzer`: propósito actual emitir alerta auditiva. Hardware: pin 7. Estado: en prueba.
- `Motor vibrador`: propósito actual emitir alerta háptica por PWM. Hardware: pin 6. Estado: en prueba.
- `Alerta fuerte`: propósito actual patrón de emergencia para alto riesgo. Dependencias: buzzer y motor. Estado: prototipo actual, no asumir definitivo.
- `Alerta media`: propósito actual patrón moderado para obstáculo frontal. Dependencias: buzzer y motor. Estado: prototipo actual, no asumir definitivo.
- `Diagnóstico serial`: propósito actual mostrar mediciones en monitor serial a 9600 baudios. Estado: en prueba.
- `Lógica de prioridad`: existe un prototipo donde el desnivel tiene prioridad sobre obstáculo frontal, pero debe tratarse como referencia temporal hasta validar el comportamiento final.