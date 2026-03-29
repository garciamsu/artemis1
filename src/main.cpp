#include <Arduino.h>
#include <SoftwareSerial.h>
#include <Wire.h>
#include <DHT.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_TSL2591.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
=========================================================
DIAGNOSTICO MODULAR DEL ROBOT 2WD POR BLUETOOTH
---------------------------------------------------------
Objetivo:
1) Consultar sensores individuales por comando
2) Probar motores y maniobras simples por comando
3) Mantener trazas claras por Bluetooth y monitor serial USB
=========================================================
*/

// ---------------- PINES Y AJUSTES ----------------
const byte HC05_RX_ARDUINO = 2;
const byte HC05_TX_ARDUINO = 3;

const byte FC51_DERECHO_PIN = 12;
const byte FC51_IZQUIERDO_PIN = 13;

const byte HCSR04_TRIG_PIN = 7;
const byte HCSR04_ECHO_PIN = 8;

const byte DHT22_PIN = A0;

const byte MOTOR_DERECHO_ENA = 5;
const byte MOTOR_DERECHO_IN1 = 11;
const byte MOTOR_DERECHO_IN2 = 10;

const byte MOTOR_IZQUIERDO_ENB = 6;
const byte MOTOR_IZQUIERDO_IN3 = 9;
const byte MOTOR_IZQUIERDO_IN4 = 4;

const long SERIAL_USB_BAUD = 9600;
const long HC05_BAUD = 9600;

const unsigned long ESPERA_INICIO_MS = 1200;
const unsigned long ULTRASONICO_TIMEOUT_US = 30000;
const byte TAMANO_BUFFER = 48;
const byte VELOCIDAD_POR_DEFECTO = 150;

const int GANANCIA_TSL = TSL2591_GAIN_MED;
const int INTEGRACION_TSL = TSL2591_INTEGRATIONTIME_100MS;

enum FuenteComando {
    FUENTE_BLUETOOTH,
    FUENTE_USB
};

SoftwareSerial bluetoothSerial(HC05_RX_ARDUINO, HC05_TX_ARDUINO);
DHT dht(DHT22_PIN, DHT22);
Adafruit_TSL2591 tsl = Adafruit_TSL2591(2591);

char bluetoothBuffer[TAMANO_BUFFER + 1];
char usbBuffer[TAMANO_BUFFER + 1];
byte bluetoothIndex = 0;
byte usbIndex = 0;
bool tslDisponible = false;

// ---------------- SALIDA Y TEXTO ----------------
void responder(FuenteComando fuente, const char *mensaje) {

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

bool leerLinea(Stream &puerto, char *buffer, byte &indiceActual) {

    while (puerto.available() > 0) {
        char caracter = static_cast<char>(puerto.read());

        if (caracter == '\r') {
            continue;
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

long leerDistanciaFrontalCm() {

    digitalWrite(HCSR04_TRIG_PIN, LOW);
    delayMicroseconds(2);

    digitalWrite(HCSR04_TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(HCSR04_TRIG_PIN, LOW);

    long duracion = pulseIn(HCSR04_ECHO_PIN, HIGH, ULTRASONICO_TIMEOUT_US);

    if (duracion == 0) {
        return -1;
    }

    return duracion * 0.034 / 2;
}

void leerClimaResumen(char *salida, size_t tamanoSalida) {

    float humedad = dht.readHumidity();
    float temperatura = dht.readTemperature();

    if (isnan(humedad) || isnan(temperatura)) {
        snprintf(salida, tamanoSalida, "DHT22 sin lectura valida");
        return;
    }

    int temperaturaDecimas = static_cast<int>(temperatura * 10.0f);
    int humedadDecimas = static_cast<int>(humedad * 10.0f);

    snprintf(
        salida,
        tamanoSalida,
        "Clima T=%d.%dC H=%d.%d%%",
        temperaturaDecimas / 10,
        abs(temperaturaDecimas % 10),
        humedadDecimas / 10,
        abs(humedadDecimas % 10)
    );
}

void leerLuzResumen(char *salida, size_t tamanoSalida) {

    if (!tslDisponible) {
        snprintf(salida, tamanoSalida, "TSL2591 no detectado");
        return;
    }

    sensors_event_t evento;
    tsl.getEvent(&evento);

    if (evento.light == 0) {
        uint32_t luminosidad = tsl.getFullLuminosity();
        uint16_t infrarrojo = luminosidad >> 16;
        uint16_t espectroCompleto = luminosidad & 0xFFFF;

        snprintf(
            salida,
            tamanoSalida,
            "Luz lux=0.0 full=%u ir=%u",
            espectroCompleto,
            infrarrojo
        );
        return;
    }

    int luxDecimas = static_cast<int>(evento.light * 10.0f);
    snprintf(salida, tamanoSalida, "Luz lux=%d.%d", luxDecimas / 10, abs(luxDecimas % 10));
}

void responderResumenSensores(FuenteComando fuente) {

    char respuesta[80];

    leerLineaResumen(respuesta, sizeof(respuesta));
    responder(fuente, respuesta);

    long distancia = leerDistanciaFrontalCm();
    if (distancia < 0) {
        responderFlash(fuente, F("Distancia sin eco"));
    }
    else {
        snprintf(respuesta, sizeof(respuesta), "Distancia %ld cm", distancia);
        responder(fuente, respuesta);
    }

    leerClimaResumen(respuesta, sizeof(respuesta));
    responder(fuente, respuesta);

    leerLuzResumen(respuesta, sizeof(respuesta));
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

    digitalWrite(MOTOR_DERECHO_IN1, adelante ? HIGH : LOW);
    digitalWrite(MOTOR_DERECHO_IN2, adelante ? LOW : HIGH);
    analogWrite(MOTOR_DERECHO_ENA, velocidad);
}

void moverMotorIzquierdo(bool adelante, byte velocidad) {

    digitalWrite(MOTOR_IZQUIERDO_IN3, adelante ? HIGH : LOW);
    digitalWrite(MOTOR_IZQUIERDO_IN4, adelante ? LOW : HIGH);
    analogWrite(MOTOR_IZQUIERDO_ENB, velocidad);
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

    bool adelante = strcmp(direccion, "adelante") == 0;
    bool atras = strcmp(direccion, "atras") == 0;

    if (!adelante && !atras) {
        responderFlash(fuente, F("Direccion invalida. Usa adelante o atras"));
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

// ---------------- COMANDOS ----------------
void mostrarAyuda(FuenteComando fuente) {

    responderFlash(fuente, F("Comandos: ayuda, estado, linea, dist, clima, luz, sensores, stop"));
    responderFlash(fuente, F("Motores: md adelante 120 | md atras 120 | md stop"));
    responderFlash(fuente, F("Motores: mi adelante 120 | mi atras 120 | mi stop"));
    responderFlash(fuente, F("Motores: motores adelante 120 | motores atras 120"));
    responderFlash(fuente, F("Motores: giro izq 120 | giro der 120"));
}

void mostrarEstado(FuenteComando fuente) {

    responderFlash(fuente, F("Estado: diagnostico modular activo"));
    responderFlash(fuente, F("Bluetooth HC-05 9600, USB 9600"));
    responderFlash(fuente, F("Linea FC-51 I=13 D=12, ultrasonico TRIG=7 ECHO=8"));
    responderFlash(fuente, F("DHT22=A0, TSL2591 por I2C, L298N listo"));
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
    char respuesta[80];

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

    if (strcmp(comandoNormalizado, "dist") == 0 || strcmp(comandoNormalizado, "distancia") == 0) {
        long distancia = leerDistanciaFrontalCm();

        if (distancia < 0) {
            responderFlash(fuente, F("Distancia sin eco"));
        }
        else {
            snprintf(respuesta, sizeof(respuesta), "Distancia %ld cm", distancia);
            responder(fuente, respuesta);
        }
        return;
    }

    if (strcmp(comandoNormalizado, "clima") == 0) {
        leerClimaResumen(respuesta, sizeof(respuesta));
        responder(fuente, respuesta);
        return;
    }

    if (strcmp(comandoNormalizado, "luz") == 0) {
        leerLuzResumen(respuesta, sizeof(respuesta));
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

    strncpy(comandoEditable, comandoNormalizado, sizeof(comandoEditable) - 1);
    comandoEditable[sizeof(comandoEditable) - 1] = '\0';

    if (
        strncmp(comandoEditable, "md ", 3) == 0 ||
        strncmp(comandoEditable, "mi ", 3) == 0 ||
        strncmp(comandoEditable, "motores ", 8) == 0 ||
        strncmp(comandoEditable, "giro ", 5) == 0
    ) {
        procesarComandoMotor(comandoEditable, fuente);
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

    pinMode(HCSR04_TRIG_PIN, OUTPUT);
    pinMode(HCSR04_ECHO_PIN, INPUT);

    digitalWrite(HCSR04_TRIG_PIN, LOW);
}

void setup() {

    Serial.begin(SERIAL_USB_BAUD);
    bluetoothSerial.begin(HC05_BAUD);

    configurarPinesSensores();
    configurarPinesMotores();

    dht.begin();
    Wire.begin();

    tslDisponible = tsl.begin();
    if (tslDisponible) {
        tsl.setGain(static_cast<tsl2591Gain_t>(GANANCIA_TSL));
        tsl.setTiming(static_cast<tsl2591IntegrationTime_t>(INTEGRACION_TSL));
    }

    delay(ESPERA_INICIO_MS);

    Serial.println(F("=== DIAGNOSTICO MODULAR ROBOT 2WD ==="));
    Serial.println(F("App recomendada Android: Serial Bluetooth Terminal"));
    Serial.println(F("Configura envio con fin de linea LF o CR+LF"));
    Serial.println(F("Escribe ayuda para ver los comandos"));
    Serial.println();

    responderFlash(FUENTE_BLUETOOTH, F("Diagnostico modular listo"));
    responderFlash(FUENTE_BLUETOOTH, F("Escribe ayuda para ver los comandos"));
}

// ---------------- BUCLE PRINCIPAL ----------------
void loop() {

    if (leerLinea(bluetoothSerial, bluetoothBuffer, bluetoothIndex)) {
        procesarComando(bluetoothBuffer, FUENTE_BLUETOOTH);
    }

    if (leerLinea(Serial, usbBuffer, usbIndex)) {
        procesarComando(usbBuffer, FUENTE_USB);
    }
}