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
- En la ronda actual de pruebas están cableados `HC-05`, `FC-51 derecho`, `FC-51 izquierdo`, `HC-SR04 frontal`, `DHT22`, `Adafruit TSL2591`, `L298N motor derecho` y `L298N motor izquierdo`.

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
- `Bluetooth HC-05`: propósito actual validar comunicación serial bidireccional y servir como canal de control de prueba para el robot 2WD. Entradas: texto recibido por Bluetooth cuando el modo fijo es `Serial Bluetooth Terminal`, tramas de RemoteXY cuando el modo fijo es `RemoteXY`, y comandos por monitor serial USB. Salidas: respuestas por Bluetooth solo en modo `Serial Bluetooth Terminal` y trazas por serial USB en ambos modos. Dependencias de hardware: HC-05 en modo esclavo. Pinout usado en la prueba: `TXD HC-05 -> pin 2 Arduino`, `pin 3 Arduino -> divisor resistivo 2k/1k -> RXD HC-05`, `VCC -> 5V`, `GND -> GND`, `STATE` y `KEY` desconectados. Estado: validado en enlace básico; integración con RemoteXY pendiente de validación en hardware. Hallazgos relevantes: módulo nuevo de fábrica, PIN de emparejamiento validado con `1234`, comunicación de texto confirmada con Android usando `Serial Bluetooth Terminal`, velocidad de trabajo actual `9600`; para ahorrar memoria en Arduino Uno se retiró el perfil `Arduino Bluetooth Controller` y se dejó una constante fija en firmware para elegir entre `RemoteXY` y `Serial Bluetooth Terminal`; en modo `RemoteXY` el HC-05 queda dedicado al protocolo de la app, se usa joystick con mezcla diferencial proporcional, zona muerta y auto-stop al perder enlace o centrar joystick, y se dejan trazas USB para diagnosticar desconexiones y rearme de reconexión. Procedimiento mínimo: revisar cableado y divisor resistivo, abrir monitor serial a `9600`, confirmar en el arranque el modo Bluetooth activo, y según el modo probar `hola`, `ayuda`, `ping` y `estado` por `Serial Bluetooth Terminal`, o conectar desde RemoteXY verificando trazas `RemoteXY conectado` y `RemoteXY desconectado` por USB.
- `RemoteXY HC-05`: propósito actual controlar el robot 2WD con la app gratuita RemoteXY usando un joystick, un slider de velocidad y salidas de sensores en tiempo real, manteniendo una vía de diagnóstico por USB. Entradas: `joystick_01_x`, `joystick_01_y`, `Velocidad` y `connect_flag` entregados por la librería RemoteXY. Salidas: velocidades diferenciales de ambos motores, variables `distancia`, `temperatura` y `radiacion` hacia la app, y trazas de conexión por serial USB. Dependencias de hardware: mismo HC-05 del robot en pines `2/3`, L298N, ambos motores, `HC-SR04`, `DHT22` y `TSL2591`. Pinout usado en la prueba: `TXD HC-05 -> pin 2 Arduino`, `pin 3 Arduino -> divisor resistivo -> RXD HC-05`, motor derecho `ENA=5 IN1=11 IN2=10`, motor izquierdo `ENB=6 IN3=9 IN4=4`, `HC-SR04 TRIG=7 ECHO=8`, `DHT22 DATA=A0`, `TSL2591 SDA=A4 SCL=A5`. Estado: en prueba. Hallazgos relevantes: la integración se diseñó para que el Bluetooth quede exclusivo para RemoteXY durante ese modo, sin mensajes de texto sobre el mismo enlace; la app debe poder reconectar sin reiniciar la placa porque el firmware sigue ejecutando `handler()`, detiene motores al perder enlace y deja rearmado el canal para una nueva sesión; además las variables de sensores se refrescan por ventanas de tiempo con `millis()`, conservando el último dato válido si una lectura falla para no inyectar valores basura mientras el robot se mueve. Procedimiento mínimo: abrir monitor serial a `9600`, energizar el robot, emparejar el HC-05, conectar desde RemoteXY, mover el joystick verificando avance, retroceso y giro proporcional, comprobar que `distancia`, `temperatura` y `radiacion` cambian en la app, luego forzar una desconexión y verificar `auto-stop por perdida de enlace` seguido de una reconexión limpia.
- `FC-51 derecho`: propósito actual detectar línea o contraste en el lado derecho del robot. Entradas: `OUT=12`. Salidas: nivel digital. Dependencias de hardware: sensor infrarrojo FC-51, `5V`, `GND` común. Pinout usado en la prueba: sensor derecho montado hacia el piso. Estado: en prueba. Hallazgos relevantes: altura declarada `25 mm`, separación entre centros `45 mm`, objetivo final seguimiento de línea negra de `17 mm` sobre cartulina blanca mate. El potenciómetro de al menos un sensor fue tocado previamente y la polaridad negro/blanco aún no está validada. Procedimiento mínimo: consultar lectura digital por Bluetooth o serial, probar negro y blanco manualmente, registrar si el sensor entrega `HIGH` o `LOW` sobre cada superficie antes de integrar control.
- `FC-51 izquierdo`: propósito actual detectar línea o contraste en el lado izquierdo del robot. Entradas: `OUT=13`. Salidas: nivel digital. Dependencias de hardware: sensor infrarrojo FC-51, `5V`, `GND` común. Pinout usado en la prueba: sensor izquierdo montado hacia el piso. Estado: en prueba. Hallazgos relevantes: comparte la misma geometría y limitaciones del sensor derecho; la altura actual de `25 mm` probablemente sea exigente para seguimiento fiable y debe validarse con pruebas reales antes de asumir funcionamiento correcto. Procedimiento mínimo: consultar lectura digital por Bluetooth o serial, probar negro y blanco manualmente y comparar con el sensor derecho.
- `HC-SR04 frontal`: propósito actual medir distancia al frente del robot y publicarla en RemoteXY sin frenar el control del robot. Entradas: `TRIG=7`, `ECHO=8`. Salidas: distancia en cm o lectura inválida si no hay eco. Dependencias de hardware: `5V`, `GND` común. Pinout usado en la prueba: sensor montado fijo mirando hacia adelante con `TRIG` en `D7` y `ECHO` en `D8`. Estado: en prueba. Hallazgos relevantes: el firmware actual dispara lecturas periódicas con timeout corto y conserva el último dato válido cuando no hay eco para no contaminar la interfaz ni bloquear maniobras durante demasiado tiempo. Procedimiento mínimo: abrir monitor serial a `9600`, conectar RemoteXY o usar el comando `distancia`, colocar obstáculos a distintas distancias y comprobar que la lectura cambia mientras el robot sigue respondiendo al joystick.
- `DHT22`: propósito actual medir temperatura ambiente y publicarla en RemoteXY sin saturar el bucle principal. Entradas: `DATA=A0`. Salidas: temperatura hacia diagnóstico serial y variable `temperatura` en la app; la humedad queda pendiente para una futura ampliación. Dependencias de hardware: módulo con pull-up integrada, `5V`, `GND` común. Pinout usado en la prueba: `DATA` al pin analógico `A0`. Estado: en prueba. Hallazgos relevantes: el firmware limita la frecuencia de lectura con `millis()` para respetar el tiempo mínimo del sensor y mantiene el último dato válido si una lectura devuelve `NaN`. Procedimiento mínimo: abrir monitor serial a `9600`, esperar algunos segundos tras el arranque, consultar `temperatura` por USB o en RemoteXY y verificar cambios al calentar o enfriar el sensor suavemente.
- `Adafruit TSL2591`: propósito actual medir luz ambiente por I2C y publicarla como `radiacion` en RemoteXY. Entradas: `SDA=A4`, `SCL=A5`. Salidas: lux estimados hacia diagnóstico serial y variable `radiacion` en la app. Dependencias de hardware: `5V`, `GND` común, `3Vo` e `INT` desconectados. Pinout usado en la prueba: conectado directamente al bus I2C del Arduino Uno con `SDA=A4` y `SCL=A5`. Estado: en prueba. Hallazgos relevantes: el firmware detecta el sensor al inicio, usa integración corta y ganancia media para refresco periódico, y si el sensor no responde mantiene el último dato válido para no degradar el control del robot. Procedimiento mínimo: abrir monitor serial a `9600`, confirmar el mensaje de detección I2C al arrancar, consultar `luz` o revisar `radiacion` en RemoteXY y tapar o iluminar el sensor para observar cambios.
- `L298N motor derecho`: propósito actual controlar el motor derecho del robot 2WD. Entradas: `ENA=5`, `IN1=11`, `IN2=10`. Salidas: giro adelante, atrás y velocidad PWM. Dependencias de hardware: driver L298N, batería externa, `GND` común. Estado: en prueba. Hallazgos relevantes: canal derecho confirmado en `ENA + IN1 + IN2`; en la calibración inicial `md adelante 100` movía el motor hacia atrás y `md atras 100` hacia adelante, por lo que se invirtió la lógica del canal derecho en firmware. Revalidación posterior: `md adelante 100` mueve el motor derecho hacia adelante, `md atras 100` lo mueve hacia atrás y `md stop` lo detiene correctamente.
- `L298N motor izquierdo`: propósito actual controlar el motor izquierdo del robot 2WD. Entradas: `ENB=6`, `IN3=9`, `IN4=4`. Salidas: giro adelante, atrás y velocidad PWM. Dependencias de hardware: driver L298N, batería externa, `GND` común. Estado: en prueba. Hallazgos relevantes: canal izquierdo confirmado en `ENB + IN3 + IN4`; `mi adelante 100` mueve el motor izquierdo hacia adelante y `mi atras 100` lo mueve hacia atrás respecto al avance esperado del robot. Procedimiento mínimo: probar `mi adelante`, `mi atras` y `mi stop`, registrar qué comando corresponde realmente al avance físico antes de integrar maniobras globales.

## Registro De Pruebas Actuales
- `HC-05`: realizado. Emparejamiento validado con Android, PIN `1234`, conexión y comunicación de texto confirmadas con `Serial Bluetooth Terminal`.
- `HC-05` modo app gráfica: reemplazado en firmware por `RemoteXY` para reducir consumo de memoria y simplificar la integración en Arduino Uno. Resultado actual: se dejó un modo Bluetooth fijo en el código para elegir entre `RemoteXY` y `Serial Bluetooth Terminal`; cuando el modo es `RemoteXY`, el enlace Bluetooth queda dedicado a la app, el joystick mueve el robot con mezcla diferencial proporcional, se aplica zona muerta, se detiene al centrar joystick o perder conexión, se publican `distancia`, `temperatura` y `radiacion` en la interfaz y se registran eventos de conexión y rearme por USB. Validación de hardware: pendiente.
- `RemoteXY HC-05`: preparado en firmware y pendiente de ejecución en hardware. Resultado actual: proyecto adaptado al cableado existente del HC-05, sin contraseña de acceso en RemoteXY, con auto-stop seguro al perder enlace, lógica de rearme periódico del canal para facilitar reconexiones después de errores tipo `socket closed` observados en la app y refresco periódico de sensores con retención del último dato válido.
- `Firmware de diagnóstico modular`: realizado. Compilación pendiente de revalidación final tras integrar `HC-SR04`, `DHT22` y `Adafruit TSL2591` junto con la nueva interfaz RemoteXY.
- `FC-51 derecho e izquierdo`: pendiente de ejecución en hardware. Falta registrar polaridad real sobre blanco y negro y respuesta comparativa entre ambos sensores.
- `L298N motor derecho`: en prueba. Resultado parcial: la primera calibración mostró sentido invertido (`md adelante` hacia atrás y `md atras` hacia adelante), `md stop` funcionó correctamente, luego se invirtió la lógica del canal derecho en firmware y la revalidación confirmó el nuevo mapeo correcto.
- `L298N motor izquierdo`: en prueba. Resultado parcial: `mi adelante 100` mueve el motor izquierdo hacia adelante, `mi atras 100` lo mueve hacia atrás y `mi stop` lo detiene correctamente.
- `L298N avance combinado`: en prueba. Resultado parcial: `motores adelante 100` mueve ambos motores hacia adelante respecto al avance esperado del robot.
- `L298N retroceso combinado`: en prueba. Resultado parcial: `motores atras 100` mueve ambos motores hacia atrás respecto al avance esperado del robot.
- `L298N giro izquierda`: en prueba. Resultado parcial: `giro izq 100` gira correctamente hacia la izquierda.
- `L298N giro derecha`: en prueba. Resultado parcial: `giro der 100` gira correctamente hacia la derecha.
- `HC-SR04 frontal`: integrado en firmware y pendiente de validación en hardware con obstáculos reales.
- `DHT22`: integrado en firmware y pendiente de validación en hardware térmico real.
- `Adafruit TSL2591`: integrado en firmware y pendiente de validación en hardware con lectura I2C real.