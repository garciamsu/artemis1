#include <Arduino.h>
#include <SoftwareSerial.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
=========================================================
DIAGNOSTICO MODULAR DEL ROBOT 2WD POR BLUETOOTH
---------------------------------------------------------
Objetivo:
1) Consultar sensores disponibles por comando
2) Probar motores y maniobras simples por comando
3) Mantener trazas claras por Bluetooth y monitor serial USB
=========================================================
*/

// ---------------- PINES Y AJUSTES ----------------
const byte HC05_RX_ARDUINO = 2;
const byte HC05_TX_ARDUINO = 3;

const byte FC51_DERECHO_PIN = 12;
const byte FC51_IZQUIERDO_PIN = 13;

const byte MOTOR_DERECHO_ENA = 5;
const byte MOTOR_DERECHO_IN1 = 11;
const byte MOTOR_DERECHO_IN2 = 10;

const byte MOTOR_IZQUIERDO_ENB = 6;
const byte MOTOR_IZQUIERDO_IN3 = 9;
const byte MOTOR_IZQUIERDO_IN4 = 4;

const long SERIAL_USB_BAUD = 9600;
const long HC05_BAUD = 9600;

const unsigned long ESPERA_INICIO_MS = 1200;
const byte TAMANO_BUFFER = 48;
const byte VELOCIDAD_POR_DEFECTO = 120;
const byte VELOCIDAD_BAJA = 100;
const byte VELOCIDAD_ALTA = 160;
const byte VELOCIDAD_ABC_1 = 100;
const byte VELOCIDAD_ABC_2 = 120;
const byte VELOCIDAD_ABC_3 = 140;
const byte VELOCIDAD_ABC_4 = 160;
const byte DIVISOR_CURVA_SUAVE = 4;
const byte VELOCIDAD_MINIMA_CURVA = 80;
const unsigned long TIEMPO_MAXIMO_SIN_COMANDO_RAPIDO_MS = 350;

enum FuenteComando {
    FUENTE_BLUETOOTH,
    FUENTE_USB
};

enum PerfilComandoRapido {
    PERFIL_COMANDO_SIMPLE,
    PERFIL_COMANDO_ARDUINO_BT_CONTROLLER
};

const PerfilComandoRapido PERFIL_COMANDO_INICIAL = PERFIL_COMANDO_ARDUINO_BT_CONTROLLER;

SoftwareSerial bluetoothSerial(HC05_RX_ARDUINO, HC05_TX_ARDUINO);

char bluetoothBuffer[TAMANO_BUFFER + 1];
char usbBuffer[TAMANO_BUFFER + 1];
byte bluetoothIndex = 0;
byte usbIndex = 0;
byte velocidadComandoRapido = VELOCIDAD_POR_DEFECTO;
PerfilComandoRapido perfilComandoRapidoActivo = PERFIL_COMANDO_INICIAL;
unsigned long ultimoComandoRapidoMs = 0;
bool movimientoRapidoActivo = false;
bool suprimirRespuestaBluetoothRapida = false;

void detenerMotores();

// ---------------- SALIDA Y TEXTO ----------------
void responder(FuenteComando fuente, const char *mensaje) {

    if (fuente == FUENTE_BLUETOOTH && suprimirRespuestaBluetoothRapida) {
        return;
    }

    if (fuente == FUENTE_BLUETOOTH) {
        bluetoothSerial.println(mensaje);
        Serial.print("UNO -> BT: ");
        Serial.println(mensaje);
    }
    else {
        Serial.println(mensaje);
    }
}

void responderFlash(FuenteComando fuente, const __FlashStringHelper *mensaje) {

    if (fuente == FUENTE_BLUETOOTH && suprimirRespuestaBluetoothRapida) {
        return;
    }

    if (fuente == FUENTE_BLUETOOTH) {
        bluetoothSerial.println(mensaje);
        Serial.print(F("UNO -> BT: "));
        Serial.println(mensaje);
    }
    else {
        Serial.println(mensaje);
    }
}

void registrarEntrada(FuenteComando fuente, const char *mensaje) {

    if (fuente == FUENTE_BLUETOOTH) {
        Serial.print(F("BT -> UNO: "));
    }
    else {
        Serial.print(F("USB -> UNO: "));
    }

    Serial.println(mensaje);
}

void normalizarMensaje(const char *entrada, char *salida, byte tamanoSalida) {

    while (*entrada == ' ' || *entrada == '\t') {
        entrada++;
    }

    byte indice = 0;

    while (*entrada != '\0' && indice < tamanoSalida - 1) {
        salida[indice] = static_cast<char>(tolower(static_cast<unsigned char>(*entrada)));
        indice++;
        entrada++;
    }

    while (indice > 0 && (salida[indice - 1] == ' ' || salida[indice - 1] == '\t')) {
        indice--;
    }

    salida[indice] = '\0';
}

bool esComandoRapido(char caracter) {

    switch (caracter) {
        case '1':
        case '2':
        case '3':
        case '4':
        case 'F':
        case 'B':
        case 'U':
        case 'u':
        case 'N':
        case 'L':
        case 'R':
        case 'X':
        case 'G':
        case 'H':
        case 'D':
        case 'E':
        case 'C':
        case 'I':
        case 'J':
        case 'K':
        case 'S':
        case 'Y':
            return true;
        default:
            return false;
    }
}

bool leerLinea(Stream &puerto, char *buffer, byte &indiceActual) {

    while (puerto.available() > 0) {
        char caracter = static_cast<char>(puerto.read());

        if (caracter == '\r') {
            continue;
        }

        if (indiceActual == 0 && esComandoRapido(caracter)) {
            buffer[0] = caracter;
            buffer[1] = '\0';
            return true;
        }

        if (caracter == '\n') {
            if (indiceActual == 0) {
                continue;
            }

            buffer[indiceActual] = '\0';
            indiceActual = 0;
            return true;
        }

        if (indiceActual < TAMANO_BUFFER) {
            buffer[indiceActual] = caracter;
            indiceActual++;
        }
    }

    return false;
}

const char *textoNivel(int valorDigital) {

    return valorDigital == HIGH ? "HIGH" : "LOW";
}

const char *nombrePerfilComandoRapido(PerfilComandoRapido perfil) {

    if (perfil == PERFIL_COMANDO_ARDUINO_BT_CONTROLLER) {
        return "arduino";
    }

    return "simple";
}

byte velocidadDesdeTexto(const char *texto) {

    if (texto == NULL || *texto == '\0') {
        return VELOCIDAD_POR_DEFECTO;
    }

    int valor = atoi(texto);

    if (valor < 0) {
        valor = 0;
    }

    if (valor > 255) {
        valor = 255;
    }

    return static_cast<byte>(valor);
}

void actualizarVelocidadComandoRapido(byte velocidad, FuenteComando fuente) {

    velocidadComandoRapido = velocidad;

    if (fuente == FUENTE_USB) {
        char respuesta[48];
        snprintf(respuesta, sizeof(respuesta), "Velocidad rapida=%u", velocidadComandoRapido);
        responder(fuente, respuesta);
    }
    else {
        Serial.print(F("Velocidad rapida="));
        Serial.println(velocidadComandoRapido);
    }
}

byte calcularVelocidadInteriorCurva(byte velocidadExterior) {

    byte velocidadInterior = velocidadExterior / DIVISOR_CURVA_SUAVE;

    if (velocidadInterior == 0) {
        return 0;
    }

    if (velocidadInterior < VELOCIDAD_MINIMA_CURVA) {
        velocidadInterior = VELOCIDAD_MINIMA_CURVA;
    }

    if (velocidadInterior > velocidadExterior) {
        velocidadInterior = velocidadExterior;
    }

    return velocidadInterior;
}

void registrarActividadComandoRapido(bool hayMovimiento, bool esStop, FuenteComando fuente) {

    if (fuente != FUENTE_BLUETOOTH) {
        return;
    }

    if (hayMovimiento) {
        ultimoComandoRapidoMs = millis();
        movimientoRapidoActivo = true;
        return;
    }

    if (esStop) {
        ultimoComandoRapidoMs = 0;
        movimientoRapidoActivo = false;
    }
}

void detenerMotoresPorInactividadBluetooth() {

    if (!movimientoRapidoActivo) {
        return;
    }

    if (ultimoComandoRapidoMs == 0) {
        movimientoRapidoActivo = false;
        return;
    }

    if (millis() - ultimoComandoRapidoMs < TIEMPO_MAXIMO_SIN_COMANDO_RAPIDO_MS) {
        return;
    }

    detenerMotores();
    movimientoRapidoActivo = false;
    ultimoComandoRapidoMs = 0;
    Serial.println(F("Auto-stop Bluetooth por inactividad"));
}

// ---------------- SENSORES ----------------
void leerLineaResumen(char *salida, size_t tamanoSalida) {

    int derecho = digitalRead(FC51_DERECHO_PIN);
    int izquierdo = digitalRead(FC51_IZQUIERDO_PIN);

    snprintf(
        salida,
        tamanoSalida,
        "Linea I=%s D=%s",
        textoNivel(izquierdo),
        textoNivel(derecho)
    );
}

void responderResumenSensores(FuenteComando fuente) {

    char respuesta[48];

    leerLineaResumen(respuesta, sizeof(respuesta));
    responder(fuente, respuesta);
}

// ---------------- ACTUADORES ----------------
void detenerMotores() {

    analogWrite(MOTOR_DERECHO_ENA, 0);
    analogWrite(MOTOR_IZQUIERDO_ENB, 0);

    digitalWrite(MOTOR_DERECHO_IN1, LOW);
    digitalWrite(MOTOR_DERECHO_IN2, LOW);
    digitalWrite(MOTOR_IZQUIERDO_IN3, LOW);
    digitalWrite(MOTOR_IZQUIERDO_IN4, LOW);
}

void moverMotorDerecho(bool adelante, byte velocidad) {

    digitalWrite(MOTOR_DERECHO_IN1, adelante ? LOW : HIGH);
    digitalWrite(MOTOR_DERECHO_IN2, adelante ? HIGH : LOW);
    analogWrite(MOTOR_DERECHO_ENA, velocidad);
}

void moverMotorIzquierdo(bool adelante, byte velocidad) {

    digitalWrite(MOTOR_IZQUIERDO_IN3, adelante ? HIGH : LOW);
    digitalWrite(MOTOR_IZQUIERDO_IN4, adelante ? LOW : HIGH);
    analogWrite(MOTOR_IZQUIERDO_ENB, velocidad);
}

void moverMotoresDiferencial(
    bool adelanteDerecho,
    byte velocidadDerecha,
    bool adelanteIzquierdo,
    byte velocidadIzquierda
) {

    moverMotorDerecho(adelanteDerecho, velocidadDerecha);
    moverMotorIzquierdo(adelanteIzquierdo, velocidadIzquierda);
}

void ejecutarMotorIndividual(bool esDerecho, const char *direccion, byte velocidad, FuenteComando fuente) {

    char respuesta[64];

    if (strcmp(direccion, "stop") == 0) {
        if (esDerecho) {
            analogWrite(MOTOR_DERECHO_ENA, 0);
            digitalWrite(MOTOR_DERECHO_IN1, LOW);
            digitalWrite(MOTOR_DERECHO_IN2, LOW);
            responderFlash(fuente, F("Motor derecho detenido"));
        }
        else {
            analogWrite(MOTOR_IZQUIERDO_ENB, 0);
            digitalWrite(MOTOR_IZQUIERDO_IN3, LOW);
            digitalWrite(MOTOR_IZQUIERDO_IN4, LOW);
            responderFlash(fuente, F("Motor izquierdo detenido"));
        }
        return;
    }

    bool adelante = strcmp(direccion, "adelante") == 0;
    bool atras = strcmp(direccion, "atras") == 0;

    if (!adelante && !atras) {
        responderFlash(fuente, F("Direccion invalida. Usa adelante, atras o stop"));
        return;
    }

    if (esDerecho) {
        moverMotorDerecho(adelante, velocidad);
        snprintf(respuesta, sizeof(respuesta), "Motor derecho %s vel=%u", direccion, velocidad);
    }
    else {
        moverMotorIzquierdo(adelante, velocidad);
        snprintf(respuesta, sizeof(respuesta), "Motor izquierdo %s vel=%u", direccion, velocidad);
    }

    responder(fuente, respuesta);
}

void ejecutarMotores(const char *direccion, byte velocidad, FuenteComando fuente) {

    char respuesta[64];

    if (strcmp(direccion, "stop") == 0) {
        detenerMotores();
        responderFlash(fuente, F("Motores detenidos"));
        return;
    }

    bool adelante = strcmp(direccion, "adelante") == 0;
    bool atras = strcmp(direccion, "atras") == 0;

    if (!adelante && !atras) {
        responderFlash(fuente, F("Direccion invalida. Usa adelante, atras o stop"));
        return;
    }

    moverMotorDerecho(adelante, velocidad);
    moverMotorIzquierdo(adelante, velocidad);

    snprintf(respuesta, sizeof(respuesta), "Motores %s vel=%u", direccion, velocidad);
    responder(fuente, respuesta);
}

void ejecutarGiro(const char *direccion, byte velocidad, FuenteComando fuente) {

    char respuesta[64];

    if (strcmp(direccion, "izq") == 0 || strcmp(direccion, "izquierda") == 0) {
        moverMotorDerecho(true, velocidad);
        moverMotorIzquierdo(false, velocidad);
        snprintf(respuesta, sizeof(respuesta), "Giro izquierda vel=%u", velocidad);
        responder(fuente, respuesta);
        return;
    }

    if (strcmp(direccion, "der") == 0 || strcmp(direccion, "derecha") == 0) {
        moverMotorDerecho(false, velocidad);
        moverMotorIzquierdo(true, velocidad);
        snprintf(respuesta, sizeof(respuesta), "Giro derecha vel=%u", velocidad);
        responder(fuente, respuesta);
        return;
    }

    responderFlash(fuente, F("Direccion invalida. Usa izq o der"));
}

void ejecutarCurvaSuave(bool adelante, bool haciaIzquierda, byte velocidad, FuenteComando fuente) {

    char respuesta[64];
    byte velocidadInterior = calcularVelocidadInteriorCurva(velocidad);

    if (haciaIzquierda) {
        moverMotoresDiferencial(adelante, velocidad, adelante, velocidadInterior);
        snprintf(
            respuesta,
            sizeof(respuesta),
            "Curva %s izq vel=%u interior=%u",
            adelante ? "adelante" : "atras",
            velocidad,
            velocidadInterior
        );
    }
    else {
        moverMotoresDiferencial(adelante, velocidadInterior, adelante, velocidad);
        snprintf(
            respuesta,
            sizeof(respuesta),
            "Curva %s der vel=%u interior=%u",
            adelante ? "adelante" : "atras",
            velocidad,
            velocidadInterior
        );
    }

    responder(fuente, respuesta);
}

bool procesarComandoRapidoSimple(char comando, FuenteComando fuente) {

    switch (comando) {
        case '1':
            registrarActividadComandoRapido(false, false, fuente);
            actualizarVelocidadComandoRapido(VELOCIDAD_BAJA, fuente);
            return true;
        case '2':
            registrarActividadComandoRapido(false, false, fuente);
            actualizarVelocidadComandoRapido(VELOCIDAD_POR_DEFECTO, fuente);
            return true;
        case '3':
            registrarActividadComandoRapido(false, false, fuente);
            actualizarVelocidadComandoRapido(VELOCIDAD_ALTA, fuente);
            return true;
        case 'U':
            registrarActividadComandoRapido(true, false, fuente);
            ejecutarMotores("adelante", velocidadComandoRapido, fuente);
            return true;
        case 'N':
            registrarActividadComandoRapido(true, false, fuente);
            ejecutarMotores("atras", velocidadComandoRapido, fuente);
            return true;
        case 'L':
            registrarActividadComandoRapido(true, false, fuente);
            ejecutarGiro("izq", velocidadComandoRapido, fuente);
            return true;
        case 'R':
            registrarActividadComandoRapido(true, false, fuente);
            ejecutarGiro("der", velocidadComandoRapido, fuente);
            return true;
        case 'X':
            registrarActividadComandoRapido(false, true, fuente);
            detenerMotores();
            if (fuente == FUENTE_USB) {
                responderFlash(fuente, F("Motores detenidos"));
            }
            return true;
        case 'D':
            registrarActividadComandoRapido(true, false, fuente);
            ejecutarMotorIndividual(true, "adelante", velocidadComandoRapido, fuente);
            return true;
        case 'E':
            registrarActividadComandoRapido(true, false, fuente);
            ejecutarMotorIndividual(true, "atras", velocidadComandoRapido, fuente);
            return true;
        case 'C':
            registrarActividadComandoRapido(false, true, fuente);
            ejecutarMotorIndividual(true, "stop", velocidadComandoRapido, fuente);
            return true;
        case 'I':
            registrarActividadComandoRapido(true, false, fuente);
            ejecutarMotorIndividual(false, "adelante", velocidadComandoRapido, fuente);
            return true;
        case 'J':
            registrarActividadComandoRapido(true, false, fuente);
            ejecutarMotorIndividual(false, "atras", velocidadComandoRapido, fuente);
            return true;
        case 'K':
            registrarActividadComandoRapido(false, true, fuente);
            ejecutarMotorIndividual(false, "stop", velocidadComandoRapido, fuente);
            return true;
        default:
            return false;
    }
}

bool procesarComandoRapidoArduinoBt(char comando, FuenteComando fuente) {

    switch (comando) {
        case '1':
            registrarActividadComandoRapido(false, false, fuente);
            actualizarVelocidadComandoRapido(VELOCIDAD_ABC_1, fuente);
            return true;
        case '2':
            registrarActividadComandoRapido(false, false, fuente);
            actualizarVelocidadComandoRapido(VELOCIDAD_ABC_2, fuente);
            return true;
        case '3':
            registrarActividadComandoRapido(false, false, fuente);
            actualizarVelocidadComandoRapido(VELOCIDAD_ABC_3, fuente);
            return true;
        case '4':
            registrarActividadComandoRapido(false, false, fuente);
            actualizarVelocidadComandoRapido(VELOCIDAD_ABC_4, fuente);
            return true;
        case 'F':
            registrarActividadComandoRapido(true, false, fuente);
            ejecutarMotores("adelante", velocidadComandoRapido, fuente);
            return true;
        case 'B':
            registrarActividadComandoRapido(true, false, fuente);
            ejecutarMotores("atras", velocidadComandoRapido, fuente);
            return true;
        case 'L':
            registrarActividadComandoRapido(true, false, fuente);
            ejecutarGiro("izq", velocidadComandoRapido, fuente);
            return true;
        case 'R':
            registrarActividadComandoRapido(true, false, fuente);
            ejecutarGiro("der", velocidadComandoRapido, fuente);
            return true;
        case 'G':
            registrarActividadComandoRapido(true, false, fuente);
            ejecutarCurvaSuave(true, true, velocidadComandoRapido, fuente);
            return true;
        case 'H':
            registrarActividadComandoRapido(true, false, fuente);
            ejecutarCurvaSuave(true, false, velocidadComandoRapido, fuente);
            return true;
        case 'I':
            registrarActividadComandoRapido(true, false, fuente);
            ejecutarCurvaSuave(false, true, velocidadComandoRapido, fuente);
            return true;
        case 'J':
            registrarActividadComandoRapido(true, false, fuente);
            ejecutarCurvaSuave(false, false, velocidadComandoRapido, fuente);
            return true;
        case 'S':
            registrarActividadComandoRapido(false, true, fuente);
            detenerMotores();
            if (fuente == FUENTE_USB) {
                responderFlash(fuente, F("Motores detenidos"));
            }
            return true;
        case 'U':
        case 'u':
            registrarActividadComandoRapido(false, false, fuente);
            if (fuente == FUENTE_USB) {
                responderFlash(fuente, F("LED no configurado en este robot"));
            }
            else {
                Serial.println(F("LED no configurado en este robot"));
            }
            return true;
        case 'Y':
            registrarActividadComandoRapido(false, false, fuente);
            if (fuente == FUENTE_USB) {
                responderFlash(fuente, F("Buzzer no configurado en este robot"));
            }
            else {
                Serial.println(F("Buzzer no configurado en este robot"));
            }
            return true;
        default:
            return false;
    }
}

bool procesarComandoRapido(const char *mensajeOriginal, FuenteComando fuente) {

    if (mensajeOriginal[0] == '\0' || mensajeOriginal[1] != '\0') {
        return false;
    }

    char comando = mensajeOriginal[0];
    bool resultado = false;

    if (fuente == FUENTE_BLUETOOTH) {
        suprimirRespuestaBluetoothRapida = true;
    }

    if (perfilComandoRapidoActivo == PERFIL_COMANDO_ARDUINO_BT_CONTROLLER) {
        resultado = procesarComandoRapidoArduinoBt(comando, fuente);
    }
    else {
        resultado = procesarComandoRapidoSimple(comando, fuente);
    }

    suprimirRespuestaBluetoothRapida = false;
    return resultado;
}

// ---------------- COMANDOS ----------------
void mostrarAyuda(FuenteComando fuente) {

    responderFlash(fuente, F("Comandos: ayuda, estado, linea, sensores, stop"));
    responderFlash(fuente, F("Perfil app: perfil arduino | perfil simple | perfil"));
    responderFlash(fuente, F("Motores: md adelante 120 | md atras 120 | md stop"));
    responderFlash(fuente, F("Motores: mi adelante 120 | mi atras 120 | mi stop"));
    responderFlash(fuente, F("Motores: motores adelante 120 | motores atras 120 | motores stop"));
    responderFlash(fuente, F("Motores: giro izq 120 | giro der 120"));

    if (perfilComandoRapidoActivo == PERFIL_COMANDO_ARDUINO_BT_CONTROLLER) {
        responderFlash(fuente, F("Arduino BT: F/B avance, L/R giro, G/H/I/J curvas, S stop"));
        responderFlash(fuente, F("Arduino BT: 1/2/3/4 velocidad, U/u LED, Y buzzer"));
    }
    else {
        responderFlash(fuente, F("Simple: U adelante, N atras, L/R giro, X stop"));
        responderFlash(fuente, F("Simple: D/E/C derecho, I/J/K izquierdo, 1/2/3 velocidad"));
    }
}

void mostrarEstado(FuenteComando fuente) {

    responderFlash(fuente, F("Estado: diagnostico modular activo"));
    responderFlash(fuente, F("Bluetooth HC-05 9600, USB 9600"));
    responderFlash(fuente, F("Linea FC-51 I=13 D=12 disponibles"));
    responderFlash(fuente, F("L298N derecho 5/11/10 e izquierdo 6/9/4 listos"));

    char respuesta[48];
    snprintf(respuesta, sizeof(respuesta), "Perfil app activo: %s", nombrePerfilComandoRapido(perfilComandoRapidoActivo));
    responder(fuente, respuesta);
}

void procesarComandoPerfil(char *comando, FuenteComando fuente) {

    char *token = strtok(comando, " ");
    char *perfil = strtok(NULL, " ");

    if (token == NULL || strcmp(token, "perfil") != 0) {
        responderFlash(fuente, F("Comando de perfil invalido"));
        return;
    }

    if (perfil == NULL) {
        char respuesta[48];
        snprintf(respuesta, sizeof(respuesta), "Perfil actual: %s", nombrePerfilComandoRapido(perfilComandoRapidoActivo));
        responder(fuente, respuesta);
        return;
    }

    if (
        strcmp(perfil, "arduino") == 0 ||
        strcmp(perfil, "abc") == 0 ||
        strcmp(perfil, "controller") == 0
    ) {
        perfilComandoRapidoActivo = PERFIL_COMANDO_ARDUINO_BT_CONTROLLER;
        velocidadComandoRapido = VELOCIDAD_ABC_2;
        responderFlash(fuente, F("Perfil Arduino Bluetooth Controller activo"));
        responderFlash(fuente, F("Velocidad inicial 2 = 120"));
        return;
    }

    if (strcmp(perfil, "simple") == 0) {
        perfilComandoRapidoActivo = PERFIL_COMANDO_SIMPLE;
        velocidadComandoRapido = VELOCIDAD_POR_DEFECTO;
        responderFlash(fuente, F("Perfil simple activo"));
        responderFlash(fuente, F("Velocidad inicial 2 = 150"));
        return;
    }

    responderFlash(fuente, F("Perfil invalido. Usa arduino o simple"));
}

void procesarComandoMotor(char *comando, FuenteComando fuente) {

    char *token = strtok(comando, " ");

    if (token == NULL) {
        responderFlash(fuente, F("Comando de motor vacio"));
        return;
    }

    if (strcmp(token, "md") == 0 || strcmp(token, "mi") == 0) {
        bool esDerecho = strcmp(token, "md") == 0;
        char *direccion = strtok(NULL, " ");
        char *velocidadTexto = strtok(NULL, " ");

        if (direccion == NULL) {
            responderFlash(fuente, F("Falta direccion para md o mi"));
            return;
        }

        ejecutarMotorIndividual(esDerecho, direccion, velocidadDesdeTexto(velocidadTexto), fuente);
        return;
    }

    if (strcmp(token, "motores") == 0) {
        char *direccion = strtok(NULL, " ");
        char *velocidadTexto = strtok(NULL, " ");

        if (direccion == NULL) {
            responderFlash(fuente, F("Falta direccion para motores"));
            return;
        }

        ejecutarMotores(direccion, velocidadDesdeTexto(velocidadTexto), fuente);
        return;
    }

    if (strcmp(token, "giro") == 0) {
        char *direccion = strtok(NULL, " ");
        char *velocidadTexto = strtok(NULL, " ");

        if (direccion == NULL) {
            responderFlash(fuente, F("Falta direccion para giro"));
            return;
        }

        ejecutarGiro(direccion, velocidadDesdeTexto(velocidadTexto), fuente);
    }
}

void procesarComando(const char *mensajeOriginal, FuenteComando fuente) {

    char comandoEditable[TAMANO_BUFFER + 1];
    char comandoNormalizado[TAMANO_BUFFER + 1];
    char respuesta[48];

    if (procesarComandoRapido(mensajeOriginal, fuente)) {
        return;
    }

    registrarEntrada(fuente, mensajeOriginal);

    normalizarMensaje(mensajeOriginal, comandoNormalizado, sizeof(comandoNormalizado));

    if (strcmp(comandoNormalizado, "hola") == 0) {
        responderFlash(fuente, F("Hola. Diagnostico del robot listo"));
        responderFlash(fuente, F("Escribe ayuda para ver los comandos"));
        return;
    }

    if (strcmp(comandoNormalizado, "ayuda") == 0) {
        mostrarAyuda(fuente);
        return;
    }

    if (strcmp(comandoNormalizado, "ping") == 0) {
        responderFlash(fuente, F("pong"));
        return;
    }

    if (strcmp(comandoNormalizado, "estado") == 0) {
        mostrarEstado(fuente);
        return;
    }

    if (strcmp(comandoNormalizado, "linea") == 0) {
        leerLineaResumen(respuesta, sizeof(respuesta));
        responder(fuente, respuesta);
        return;
    }

    if (strcmp(comandoNormalizado, "sensores") == 0) {
        responderResumenSensores(fuente);
        return;
    }

    if (strcmp(comandoNormalizado, "stop") == 0) {
        detenerMotores();
        responderFlash(fuente, F("Motores detenidos"));
        return;
    }

    if (strcmp(comandoNormalizado, "perfil") == 0) {
        strncpy(comandoEditable, comandoNormalizado, sizeof(comandoEditable) - 1);
        comandoEditable[sizeof(comandoEditable) - 1] = '\0';
        procesarComandoPerfil(comandoEditable, fuente);
        return;
    }

    strncpy(comandoEditable, comandoNormalizado, sizeof(comandoEditable) - 1);
    comandoEditable[sizeof(comandoEditable) - 1] = '\0';

    if (
        strncmp(comandoEditable, "perfil", 6) == 0 ||
        strncmp(comandoEditable, "md ", 3) == 0 ||
        strncmp(comandoEditable, "mi ", 3) == 0 ||
        strncmp(comandoEditable, "motores ", 8) == 0 ||
        strncmp(comandoEditable, "giro ", 5) == 0
    ) {
        if (strncmp(comandoEditable, "perfil", 6) == 0) {
            procesarComandoPerfil(comandoEditable, fuente);
        }
        else {
            procesarComandoMotor(comandoEditable, fuente);
        }
        return;
    }

    responderFlash(fuente, F("Comando no reconocido. Escribe ayuda"));
}

// ---------------- CONFIGURACION ----------------
void configurarPinesMotores() {

    pinMode(MOTOR_DERECHO_ENA, OUTPUT);
    pinMode(MOTOR_DERECHO_IN1, OUTPUT);
    pinMode(MOTOR_DERECHO_IN2, OUTPUT);

    pinMode(MOTOR_IZQUIERDO_ENB, OUTPUT);
    pinMode(MOTOR_IZQUIERDO_IN3, OUTPUT);
    pinMode(MOTOR_IZQUIERDO_IN4, OUTPUT);

    detenerMotores();
}

void configurarPinesSensores() {

    pinMode(FC51_DERECHO_PIN, INPUT);
    pinMode(FC51_IZQUIERDO_PIN, INPUT);
}

void setup() {

    Serial.begin(SERIAL_USB_BAUD);
    bluetoothSerial.begin(HC05_BAUD);

    configurarPinesSensores();
    configurarPinesMotores();

    delay(ESPERA_INICIO_MS);

    Serial.println(F("=== DIAGNOSTICO MODULAR ROBOT 2WD ==="));
    Serial.println(F("Perfil inicial: Arduino Bluetooth Controller"));
    Serial.println(F("Comandos Arduino BT: F B L R G H I J S 1 2 3 4"));
    Serial.println(F("Cambia con: perfil arduino | perfil simple"));
    Serial.println(F("Configura envio con fin de linea LF o CR+LF"));
    Serial.println(F("Escribe ayuda para ver los comandos"));
    Serial.println();

    responderFlash(FUENTE_BLUETOOTH, F("Diagnostico modular listo"));
    responderFlash(FUENTE_BLUETOOTH, F("Escribe ayuda para ver los comandos"));
}

// ---------------- BUCLE PRINCIPAL ----------------
void loop() {

    detenerMotoresPorInactividadBluetooth();

    if (leerLinea(bluetoothSerial, bluetoothBuffer, bluetoothIndex)) {
        procesarComando(bluetoothBuffer, FUENTE_BLUETOOTH);
    }

    if (leerLinea(Serial, usbBuffer, usbIndex)) {
        procesarComando(usbBuffer, FUENTE_USB);
    }
}