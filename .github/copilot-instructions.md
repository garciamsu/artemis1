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
  - HC-05 en pines 2/3 con divisor resistivo hacia `RXD` del módulo
  - FC-51 derecho en pin 12
  - FC-51 izquierdo en pin 13
  - HC-SR04 frontal en pines 7/8
  - DHT22 en `A0`
  - TSL2591 por bus I2C (`SDA`/`SCL`)
  - L298N con motor derecho en `ENA=5`, `IN1=11`, `IN2=10`
  - L298N con motor izquierdo en `ENB=6`, `IN3=9`, `IN4=4`
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
- Cuando cambie el cableado físico, actualizar primero este archivo y luego el firmware para evitar mezclar mapas de pines viejos con el inventario activo del robot.
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
- `Bluetooth HC-05`: propósito actual validar comunicación serial bidireccional y servir como canal de comandos de prueba para el robot 2WD. Entradas: texto recibido por Bluetooth y por monitor serial USB. Salidas: respuestas por Bluetooth y trazas por serial USB. Dependencias de hardware: HC-05 en modo esclavo. Pinout usado en la prueba: `TXD HC-05 -> pin 2 Arduino`, `pin 3 Arduino -> divisor resistivo 2k/1k -> RXD HC-05`, `VCC -> 5V`, `GND -> GND`, `STATE` y `KEY` desconectados. Estado: validado en enlace básico. Hallazgos relevantes: módulo nuevo de fábrica, PIN de emparejamiento validado con `1234`, comunicación de texto confirmada con Android usando `Serial Bluetooth Terminal`, velocidad de trabajo actual `9600`. Procedimiento mínimo: revisar cableado y divisor resistivo, abrir monitor serial a `9600`, enlazar Android, configurar envío con fin de línea `LF` o `CR+LF`, enviar `hola`, `ayuda`, `ping` y `estado`, verificar eco y respuestas en ambos sentidos.
- `FC-51 derecho`: propósito actual detectar línea o contraste en el lado derecho del robot. Entradas: `OUT=12`. Salidas: nivel digital. Dependencias de hardware: sensor infrarrojo FC-51, `5V`, `GND` común. Pinout usado en la prueba: sensor derecho montado hacia el piso. Estado: identificado, calibración pendiente. Hallazgos relevantes: altura declarada `25 mm`, separación entre centros `45 mm`, objetivo final seguimiento de línea negra de `17 mm` sobre cartulina blanca mate. El potenciómetro de al menos un sensor fue tocado previamente y la polaridad negro/blanco aún no está validada. Procedimiento mínimo: consultar lectura digital por Bluetooth o serial, probar negro y blanco manualmente, registrar si el sensor entrega `HIGH` o `LOW` sobre cada superficie antes de integrar control.
- `FC-51 izquierdo`: propósito actual detectar línea o contraste en el lado izquierdo del robot. Entradas: `OUT=13`. Salidas: nivel digital. Dependencias de hardware: sensor infrarrojo FC-51, `5V`, `GND` común. Pinout usado en la prueba: sensor izquierdo montado hacia el piso. Estado: identificado, calibración pendiente. Hallazgos relevantes: comparte la misma geometría y limitaciones del sensor derecho; la altura actual de `25 mm` probablemente sea exigente para seguimiento fiable y debe validarse con pruebas reales antes de asumir funcionamiento correcto. Procedimiento mínimo: consultar lectura digital por Bluetooth o serial, probar negro y blanco manualmente y comparar con el sensor derecho.
- `HC-SR04 frontal`: propósito actual medir distancia al frente del robot. Entradas: `TRIG=7`, `ECHO=8`. Salidas: distancia en cm o lectura inválida si no hay eco. Dependencias de hardware: `5V`, `GND` común. Pinout usado en la prueba: sensor montado fijo mirando hacia adelante. Estado: identificado; prueba específica pendiente.
- `DHT22`: propósito actual medir temperatura y humedad ambiente. Entradas: `DATA=A0`. Salidas: temperatura y humedad. Dependencias de hardware: módulo con pull-up integrada, `5V`, `GND` común. Pinout usado en la prueba: `DATA` al pin analógico `A0`. Estado: identificado; lectura pendiente.
- `Adafruit TSL2591`: propósito actual medir luz ambiente por I2C. Entradas: `SDA=SDA`, `SCL=SCL`. Salidas: lectura de luminosidad y canales internos según librería. Dependencias de hardware: `5V`, `GND` común, `3Vo` e `INT` desconectados. Pinout usado en la prueba: conectado directamente al bus I2C del Arduino Uno sin otros módulos I2C activos por ahora. Estado: identificado; lectura pendiente.
- `L298N motor derecho`: propósito actual controlar el motor derecho del robot 2WD. Entradas: `ENA=5`, `IN1=11`, `IN2=10`. Salidas: giro adelante, atrás y velocidad PWM. Dependencias de hardware: driver L298N, batería externa, `GND` común. Estado: identificado; prueba de sentido y velocidad pendiente.
- `L298N motor izquierdo`: propósito actual controlar el motor izquierdo del robot 2WD. Entradas: `ENB=6`, `IN3=9`, `IN4=4`. Salidas: giro adelante, atrás y velocidad PWM. Dependencias de hardware: driver L298N, batería externa, `GND` común. Estado: identificado; prueba de sentido y velocidad pendiente.