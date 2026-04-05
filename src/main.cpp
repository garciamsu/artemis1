#include <Arduino.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_TSL2591.h>
#include <DHT.h>
#include <RemoteXY.h>
#include <SoftwareSerial.h>
#include <Wire.h>
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
1) Consultar sensores disponibles por comando
2) Probar motores y maniobras simples por comando
3) Usar RemoteXY o Serial Bluetooth Terminal segun modo fijo
=========================================================
*/

// ---------------- PINES Y AJUSTES ----------------
const byte HC05_RX_ARDUINO = 2;
const byte HC05_TX_ARDUINO = 3;

const byte FC51_DERECHO_PIN = 12;
const byte FC51_IZQUIERDO_PIN = 13;
const byte HC_SR04_TRIG_PIN = 7;
const byte HC_SR04_ECHO_PIN = 8;
const byte DHT22_PIN = A0;
const byte TSL2591_SDA_PIN = A4;
const byte TSL2591_SCL_PIN = A5;

const byte MOTOR_DERECHO_ENA = 5;
const byte MOTOR_DERECHO_IN1 = 11;
const byte MOTOR_DERECHO_IN2 = 10;

const byte MOTOR_IZQUIERDO_ENB = 6;
const byte MOTOR_IZQUIERDO_IN3 = 9;
const byte MOTOR_IZQUIERDO_IN4 = 4;

const long SERIAL_USB_BAUD = 9600;
const long HC05_BAUD = 9600;

const unsigned long ESPERA_INICIO_MS = 1200;
const unsigned long TIEMPO_MAXIMO_SIN_COMANDO_RAPIDO_MS = 350;
const unsigned long TIEMPO_REARME_REMOTEXY_MS = 1500;
const unsigned long INTERVALO_LECTURA_ULTRASONIDO_MS = 75;
const unsigned long INTERVALO_LECTURA_DHT22_MS = 2500;
const unsigned long INTERVALO_LECTURA_TSL2591_MS = 250;
const unsigned long TIMEOUT_ULTRASONIDO_US = 25000;

const byte TAMANO_BUFFER = 48;
const byte VELOCIDAD_POR_DEFECTO = 120;
const byte VELOCIDAD_BAJA = 100;
const byte VELOCIDAD_ALTA = 160;
const byte VELOCIDAD_REMOTEXY_FIJA = 120;
const byte JOYSTICK_ZONA_MUERTA = 5;
const byte DHT22_TIPO_SENSOR = DHT22;
const byte TSL2591_DIRECCION_I2C = 0x29;

enum FuenteComando {
    FUENTE_BLUETOOTH,
    FUENTE_USB
};

enum ModoAppBluetooth {
    MODO_APP_SERIAL_BT_TERMINAL,
    MODO_APP_REMOTEXY
};

const ModoAppBluetooth MODO_APP_BLUETOOTH_ACTIVO = MODO_APP_REMOTEXY;

//////////////////////////////////////////////
//        RemoteXY include library          //
//////////////////////////////////////////////

#pragma pack(push, 1)
uint8_t const PROGMEM RemoteXY_CONF_PROGMEM[] =
    {255, 2, 0, 12, 0, 88, 0, 19, 0, 0, 0, 0, 28, 1, 106, 200, 1, 1, 4, 0,
     5, 15, 7, 75, 75, 32, 2, 26, 31, 67, 14, 151, 76, 11, 77, 2, 26, 2,
     71, 22, 88, 59, 59, 56, 0, 2, 24, 195, 0, 0, 45, 195, 0, 0, 254, 66,
     0, 0, 240, 65, 0, 0, 32, 65, 0, 0, 0, 64, 24, 194, 176, 67, 0, 73,
     13, 168, 77, 24, 5, 128, 0, 204, 26, 0, 0, 0, 64, 0, 0, 200, 67, 0,
     0, 0, 0};

struct {
    int8_t joystick_01_x;
    int8_t joystick_01_y;
    float radiacion;
    float temperatura;
    float distancia;
    uint8_t connect_flag;
} RemoteXY;
#pragma pack(pop)

/////////////////////////////////////////////
//           END RemoteXY include          //
/////////////////////////////////////////////

SoftwareSerial bluetoothSerial(HC05_RX_ARDUINO, HC05_TX_ARDUINO);
DHT sensorDHT22(DHT22_PIN, DHT22_TIPO_SENSOR);
Adafruit_TSL2591 sensorTSL2591 = Adafruit_TSL2591(2591);

char bluetoothBuffer[TAMANO_BUFFER + 1];
char usbBuffer[TAMANO_BUFFER + 1];
byte bluetoothIndex = 0;
byte usbIndex = 0;
byte velocidadComandoRapido = VELOCIDAD_POR_DEFECTO;
unsigned long ultimoComandoRapidoMs = 0;
unsigned long ultimoRearmeRemoteXYMs = 0;
bool movimientoRapidoActivo = false;
bool suprimirRespuestaBluetoothRapida = false;
bool remotexyConectadoPrevio = false;
byte velocidadRemoteXYActual = VELOCIDAD_REMOTEXY_FIJA;
float distanciaFrontalCm = 0.0f;
float temperaturaAmbienteC = 0.0f;
float humedadAmbientePct = 0.0f;
float radiacionAmbienteLux = 0.0f;
bool distanciaFrontalValida = false;
bool temperaturaAmbienteValida = false;
bool humedadAmbienteValida = false;
bool radiacionAmbienteValida = false;
bool tsl2591Disponible = false;
unsigned long ultimaLecturaUltrasonidoMs = 0;
unsigned long ultimaLecturaDht22Ms = 0;
unsigned long ultimaLecturaTsl2591Ms = 0;

void detenerMotores();

// ---------------- SALIDA Y TEXTO ----------------
const char *textoNivel(int valorDigital) {

    return valorDigital == HIGH ? "HIGH" : "LOW";
}

const char *nombreModoAppBluetooth(ModoAppBluetooth modo) {

    if (modo == MODO_APP_REMOTEXY) {
        return "RemoteXY";
    }

    return "Serial Bluetooth Terminal";
}

void responder(FuenteComando fuente, const char *mensaje) {

    if (fuente == FUENTE_BLUETOOTH && suprimirRespuestaBluetoothRapida) {
        return;
    }

    if (fuente == FUENTE_BLUETOOTH && MODO_APP_BLUETOOTH_ACTIVO == MODO_APP_REMOTEXY) {
        return;
    }

    if (fuente == FUENTE_BLUETOOTH) {
        bluetoothSerial.println(mensaje);
        Serial.print(F("UNO -> BT: "));
        Serial.println(mensaje);
        return;
    }

    Serial.println(mensaje);
}

void responderFlash(FuenteComando fuente, const __FlashStringHelper *mensaje) {

    if (fuente == FUENTE_BLUETOOTH && suprimirRespuestaBluetoothRapida) {
        return;
    }

    if (fuente == FUENTE_BLUETOOTH && MODO_APP_BLUETOOTH_ACTIVO == MODO_APP_REMOTEXY) {
        return;
    }

    if (fuente == FUENTE_BLUETOOTH) {
        bluetoothSerial.println(mensaje);
        Serial.print(F("UNO -> BT: "));
        Serial.println(mensaje);
        return;
    }

    Serial.println(mensaje);
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

void copiarTextoSeguro(char *salida, size_t tamanoSalida, const char *texto) {

    if (tamanoSalida == 0) {
        return;
    }

    strncpy(salida, texto, tamanoSalida - 1);
    salida[tamanoSalida - 1] = '\0';
}

void formatearValorSensor(float valor, bool valido, byte decimales, char *salida, size_t tamanoSalida) {

    if (!valido) {
        copiarTextoSeguro(salida, tamanoSalida, "sin dato");
        return;
    }

    char temporal[16];
    dtostrf(valor, 0, decimales, temporal);

    byte inicio = 0;

    while (temporal[inicio] == ' ') {
        inicio++;
    }

    copiarTextoSeguro(salida, tamanoSalida, temporal + inicio);
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
        case 'U':
        case 'N':
        case 'L':
        case 'R':
        case 'X':
        case 'D':
        case 'E':
        case 'C':
        case 'I':
        case 'J':
        case 'K':
            return true;
        default:
            return false;
    }
}

bool leerLinea(Stream &puerto, char *buffer, byte &indiceActual) {

    while (puerto.available() > 0) {
        char caracter = static_cast<char>(puerto.read());

        if (indiceActual == 0 && esComandoRapido(caracter)) {
            buffer[0] = caracter;
            buffer[1] = '\0';
            return true;
        }

        if (caracter == '\r' || caracter == '\n') {
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
void actualizarSalidasRemoteXYSensores() {

    RemoteXY.radiacion = radiacionAmbienteLux;
    RemoteXY.temperatura = temperaturaAmbienteC;
    RemoteXY.distancia = distanciaFrontalCm;
}

float medirDistanciaFrontalCm() {

    digitalWrite(HC_SR04_TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(HC_SR04_TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(HC_SR04_TRIG_PIN, LOW);

    unsigned long duracionEco = pulseIn(HC_SR04_ECHO_PIN, HIGH, TIMEOUT_ULTRASONIDO_US);

    if (duracionEco == 0) {
        return -1.0f;
    }

    return static_cast<float>(duracionEco) / 58.0f;
}

void actualizarDistanciaFrontal(unsigned long ahoraMs) {

    if (ahoraMs - ultimaLecturaUltrasonidoMs < INTERVALO_LECTURA_ULTRASONIDO_MS) {
        return;
    }

    ultimaLecturaUltrasonidoMs = ahoraMs;

    float distanciaLeida = medirDistanciaFrontalCm();

    if (distanciaLeida < 0.0f) {
        distanciaFrontalValida = false;
        return;
    }

    distanciaFrontalCm = distanciaLeida;
    distanciaFrontalValida = true;
}

void actualizarTemperaturaAmbiente(unsigned long ahoraMs) {

    if (ahoraMs - ultimaLecturaDht22Ms < INTERVALO_LECTURA_DHT22_MS) {
        return;
    }

    ultimaLecturaDht22Ms = ahoraMs;

    float temperaturaLeida = sensorDHT22.readTemperature();
    float humedadLeida = sensorDHT22.readHumidity();

    if (isnan(temperaturaLeida)) {
        temperaturaAmbienteValida = false;
    }
    else {
        temperaturaAmbienteC = temperaturaLeida;
        temperaturaAmbienteValida = true;
    }

    if (isnan(humedadLeida)) {
        humedadAmbienteValida = false;
        return;
    }

    humedadAmbientePct = humedadLeida;
    humedadAmbienteValida = true;
}

void actualizarRadiacionAmbiente(unsigned long ahoraMs) {

    if (!tsl2591Disponible) {
        radiacionAmbienteValida = false;
        return;
    }

    if (ahoraMs - ultimaLecturaTsl2591Ms < INTERVALO_LECTURA_TSL2591_MS) {
        return;
    }

    ultimaLecturaTsl2591Ms = ahoraMs;

    uint32_t luminosidad = sensorTSL2591.getFullLuminosity();
    uint16_t infrarrojo = luminosidad >> 16;
    uint16_t espectroCompleto = luminosidad & 0xFFFF;

    if (infrarrojo == 0xFFFF && espectroCompleto == 0xFFFF) {
        radiacionAmbienteValida = false;
        return;
    }

    float lux = sensorTSL2591.calculateLux(espectroCompleto, infrarrojo);

    if (isnan(lux) || isinf(lux) || lux < 0.0f) {
        radiacionAmbienteValida = false;
        return;
    }

    radiacionAmbienteLux = lux;
    radiacionAmbienteValida = true;
}

void actualizarSensoresPeriodicamente() {

    unsigned long ahoraMs = millis();

    actualizarDistanciaFrontal(ahoraMs);
    actualizarTemperaturaAmbiente(ahoraMs);
    actualizarRadiacionAmbiente(ahoraMs);
    actualizarSalidasRemoteXYSensores();
}

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

void responderDistanciaActual(FuenteComando fuente) {

    char valor[16];
    char respuesta[48];

    formatearValorSensor(distanciaFrontalCm, distanciaFrontalValida, 1, valor, sizeof(valor));
    snprintf(respuesta, sizeof(respuesta), "Distancia=%s cm", valor);
    responder(fuente, respuesta);
}

void responderTemperaturaActual(FuenteComando fuente) {

    char valor[16];
    char respuesta[48];

    formatearValorSensor(temperaturaAmbienteC, temperaturaAmbienteValida, 1, valor, sizeof(valor));
    snprintf(respuesta, sizeof(respuesta), "Temperatura=%s C", valor);
    responder(fuente, respuesta);
}

void responderHumedadActual(FuenteComando fuente) {

    char valor[16];
    char respuesta[48];

    formatearValorSensor(humedadAmbientePct, humedadAmbienteValida, 1, valor, sizeof(valor));
    snprintf(respuesta, sizeof(respuesta), "Humedad=%s %%", valor);
    responder(fuente, respuesta);
}

void responderRadiacionActual(FuenteComando fuente) {

    char valor[16];
    char respuesta[48];

    formatearValorSensor(radiacionAmbienteLux, radiacionAmbienteValida, 1, valor, sizeof(valor));
    snprintf(respuesta, sizeof(respuesta), "Radiacion=%s lux", valor);
    responder(fuente, respuesta);
}

void responderResumenSensores(FuenteComando fuente) {

    char respuesta[48];
    leerLineaResumen(respuesta, sizeof(respuesta));
    responder(fuente, respuesta);
    responderDistanciaActual(fuente);
    responderTemperaturaActual(fuente);
    responderHumedadActual(fuente);
    responderRadiacionActual(fuente);
}

void responderEscaneoI2C(FuenteComando fuente) {

    char respuesta[48];
    byte cantidadDispositivos = 0;
    bool tsl2591Encontrado = false;

    responderFlash(fuente, F("Escaneando bus I2C..."));

    for (byte direccion = 1; direccion < 127; direccion++) {
        Wire.beginTransmission(direccion);
        byte error = Wire.endTransmission();

        if (error == 0) {
            cantidadDispositivos++;

            if (direccion == TSL2591_DIRECCION_I2C) {
                tsl2591Encontrado = true;
            }

            snprintf(respuesta, sizeof(respuesta), "I2C encontrado en 0x%02X", direccion);
            responder(fuente, respuesta);
            continue;
        }

        if (error == 4) {
            snprintf(respuesta, sizeof(respuesta), "I2C error en 0x%02X", direccion);
            responder(fuente, respuesta);
        }
    }

    snprintf(respuesta, sizeof(respuesta), "I2C total=%u", cantidadDispositivos);
    responder(fuente, respuesta);

    if (tsl2591Encontrado) {
        responderFlash(fuente, F("TSL2591 responde en 0x29"));
        return;
    }

    responderFlash(fuente, F("TSL2591 no responde en 0x29"));
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

byte escalarVelocidadDesdePorcentaje(int porcentaje, byte velocidadMaxima) {

    int valorAbsoluto = porcentaje < 0 ? -porcentaje : porcentaje;
    long velocidad = static_cast<long>(valorAbsoluto) * velocidadMaxima / 100;
    return static_cast<byte>(velocidad);
}

byte calcularVelocidadInterior(byte velocidadBase, int intensidadGiro) {

    int giroLimitado = constrain(intensidadGiro, 0, 100);
    long velocidad = map(giroLimitado, 0, 100, velocidadBase, 0);
    return static_cast<byte>(velocidad);
}

int aplicarZonaMuertaJoystick(int valor) {

    int valorAbsoluto = valor < 0 ? -valor : valor;

    if (valorAbsoluto < JOYSTICK_ZONA_MUERTA) {
        return 0;
    }

    return valor;
}

void ejecutarJoystickRemoteXY(int ejeX, int ejeY) {

    int giro = aplicarZonaMuertaJoystick(ejeX);
    int avance = aplicarZonaMuertaJoystick(ejeY);
    int giroAbsoluto = giro < 0 ? -giro : giro;
    int avanceAbsoluto = avance < 0 ? -avance : avance;
    velocidadRemoteXYActual = VELOCIDAD_REMOTEXY_FIJA;

    if (giro == 0 && avance == 0) {
        detenerMotores();
        return;
    }

    if (avance == 0) {
        bool giroDerecha = giro > 0;
        moverMotoresDiferencial(!giroDerecha, velocidadRemoteXYActual, giroDerecha, velocidadRemoteXYActual);
        return;
    }

    bool adelante = avance > 0;

    if (giro == 0 || giroAbsoluto <= avanceAbsoluto) {
        moverMotoresDiferencial(adelante, velocidadRemoteXYActual, adelante, velocidadRemoteXYActual);
        return;
    }

    int intensidadGiro = map(giroAbsoluto, avanceAbsoluto, 100, 0, 100);
    byte velocidadInterior = calcularVelocidadInterior(velocidadRemoteXYActual, intensidadGiro);

    if (giro > 0) {
        moverMotoresDiferencial(adelante, velocidadInterior, adelante, velocidadRemoteXYActual);
        return;
    }

    moverMotoresDiferencial(adelante, velocidadRemoteXYActual, adelante, velocidadInterior);
}

void registrarCambioConexionRemoteXY(bool conectado) {

    if (conectado == remotexyConectadoPrevio) {
        return;
    }

    remotexyConectadoPrevio = conectado;

    if (conectado) {
        Serial.println(F("RemoteXY conectado"));
        return;
    }

    detenerMotores();
    Serial.println(F("RemoteXY desconectado"));
    Serial.println(F("Auto-stop por perdida de enlace"));
}

void mantenerRemoteXYListoParaReconectar() {

    if (remotexyConectadoPrevio) {
        ultimoRearmeRemoteXYMs = 0;
        return;
    }

    if (ultimoRearmeRemoteXYMs == 0) {
        ultimoRearmeRemoteXYMs = millis();
        return;
    }

    if (millis() - ultimoRearmeRemoteXYMs < TIEMPO_REARME_REMOTEXY_MS) {
        return;
    }

    bluetoothSerial.listen();
    Serial.println(F("RemoteXY listo para reconexion"));
    ultimoRearmeRemoteXYMs = millis();
}

// ---------------- REMOTEXY ----------------
void configurarRemoteXY() {

    RemoteXYGui *gui = RemoteXYEngine.addGui(RemoteXY_CONF_PROGMEM, &RemoteXY);
    gui->addConnection(bluetoothSerial);
    bluetoothSerial.listen();

    RemoteXY.radiacion = 0.0f;
    RemoteXY.temperatura = 0.0f;
    RemoteXY.distancia = 0.0f;
    velocidadRemoteXYActual = VELOCIDAD_REMOTEXY_FIJA;
    remotexyConectadoPrevio = false;
    ultimoRearmeRemoteXYMs = millis();
}

void atenderRemoteXY() {

    bluetoothSerial.listen();
    RemoteXY_Handler();

    bool conectado = RemoteXY.connect_flag != 0;
    registrarCambioConexionRemoteXY(conectado);

    if (!conectado) {
        detenerMotores();
        mantenerRemoteXYListoParaReconectar();
        return;
    }

    ultimoRearmeRemoteXYMs = 0;
    ejecutarJoystickRemoteXY(RemoteXY.joystick_01_x, RemoteXY.joystick_01_y);
}

// ---------------- COMANDOS RAPIDOS ----------------
bool procesarComandoRapido(const char *mensajeOriginal, FuenteComando fuente) {

    if (mensajeOriginal[0] == '\0' || mensajeOriginal[1] != '\0') {
        return false;
    }

    if (fuente == FUENTE_BLUETOOTH) {
        suprimirRespuestaBluetoothRapida = true;
    }

    switch (mensajeOriginal[0]) {
        case '1':
            registrarActividadComandoRapido(false, false, fuente);
            actualizarVelocidadComandoRapido(VELOCIDAD_BAJA, fuente);
            break;
        case '2':
            registrarActividadComandoRapido(false, false, fuente);
            actualizarVelocidadComandoRapido(VELOCIDAD_POR_DEFECTO, fuente);
            break;
        case '3':
            registrarActividadComandoRapido(false, false, fuente);
            actualizarVelocidadComandoRapido(VELOCIDAD_ALTA, fuente);
            break;
        case 'U':
            registrarActividadComandoRapido(true, false, fuente);
            ejecutarMotores("adelante", velocidadComandoRapido, fuente);
            break;
        case 'N':
            registrarActividadComandoRapido(true, false, fuente);
            ejecutarMotores("atras", velocidadComandoRapido, fuente);
            break;
        case 'L':
            registrarActividadComandoRapido(true, false, fuente);
            ejecutarGiro("izq", velocidadComandoRapido, fuente);
            break;
        case 'R':
            registrarActividadComandoRapido(true, false, fuente);
            ejecutarGiro("der", velocidadComandoRapido, fuente);
            break;
        case 'X':
            registrarActividadComandoRapido(false, true, fuente);
            detenerMotores();
            if (fuente == FUENTE_USB) {
                responderFlash(fuente, F("Motores detenidos"));
            }
            break;
        case 'D':
            registrarActividadComandoRapido(true, false, fuente);
            ejecutarMotorIndividual(true, "adelante", velocidadComandoRapido, fuente);
            break;
        case 'E':
            registrarActividadComandoRapido(true, false, fuente);
            ejecutarMotorIndividual(true, "atras", velocidadComandoRapido, fuente);
            break;
        case 'C':
            registrarActividadComandoRapido(false, true, fuente);
            ejecutarMotorIndividual(true, "stop", velocidadComandoRapido, fuente);
            break;
        case 'I':
            registrarActividadComandoRapido(true, false, fuente);
            ejecutarMotorIndividual(false, "adelante", velocidadComandoRapido, fuente);
            break;
        case 'J':
            registrarActividadComandoRapido(true, false, fuente);
            ejecutarMotorIndividual(false, "atras", velocidadComandoRapido, fuente);
            break;
        case 'K':
            registrarActividadComandoRapido(false, true, fuente);
            ejecutarMotorIndividual(false, "stop", velocidadComandoRapido, fuente);
            break;
        default:
            suprimirRespuestaBluetoothRapida = false;
            return false;
    }

    suprimirRespuestaBluetoothRapida = false;
    return true;
}

// ---------------- COMANDOS ----------------
void mostrarAyuda(FuenteComando fuente) {

    responderFlash(fuente, F("Comandos: ayuda, estado, linea, distancia, temperatura, humedad, luz, sensores, i2cscan, stop"));
    responderFlash(fuente, F("Motores: md adelante 120 | md atras 120 | md stop"));
    responderFlash(fuente, F("Motores: mi adelante 120 | mi atras 120 | mi stop"));
    responderFlash(fuente, F("Motores: motores adelante 120 | motores atras 120 | motores stop"));
    responderFlash(fuente, F("Motores: giro izq 120 | giro der 120"));

    if (MODO_APP_BLUETOOTH_ACTIVO == MODO_APP_REMOTEXY) {
        responderFlash(fuente, F("Modo BT: RemoteXY con joystick y auto-stop de seguridad"));
        responderFlash(fuente, F("RemoteXY muestra distancia, temperatura y radiacion en vivo"));
        responderFlash(fuente, F("Bluetooth reservado a RemoteXY; USB sigue disponible"));
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
    responderFlash(fuente, F("HC-SR04 7/8, DHT22 A0 y TSL2591 A4/A5 disponibles"));
    responderFlash(fuente, F("L298N derecho 5/11/10 e izquierdo 6/9/4 listos"));

    char respuesta[64];
    snprintf(respuesta, sizeof(respuesta), "App Bluetooth activa: %s", nombreModoAppBluetooth(MODO_APP_BLUETOOTH_ACTIVO));
    responder(fuente, respuesta);

    if (MODO_APP_BLUETOOTH_ACTIVO == MODO_APP_REMOTEXY) {
        snprintf(
            respuesta,
            sizeof(respuesta),
            "RemoteXY conectado=%s velFija=%u",
            remotexyConectadoPrevio ? "si" : "no",
            VELOCIDAD_REMOTEXY_FIJA
        );
        responder(fuente, respuesta);

        snprintf(
            respuesta,
            sizeof(respuesta),
            "Velocidad RemoteXY fija=%u",
            velocidadRemoteXYActual
        );
        responder(fuente, respuesta);
        responderDistanciaActual(fuente);
        responderTemperaturaActual(fuente);
        responderHumedadActual(fuente);
        responderRadiacionActual(fuente);
        return;
    }

    snprintf(respuesta, sizeof(respuesta), "Velocidad rapida=%u", velocidadComandoRapido);
    responder(fuente, respuesta);
    responderDistanciaActual(fuente);
    responderTemperaturaActual(fuente);
    responderHumedadActual(fuente);
    responderRadiacionActual(fuente);
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
        return;
    }

    responderFlash(fuente, F("Comando de motor invalido"));
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

    if (strcmp(comandoNormalizado, "distancia") == 0 || strcmp(comandoNormalizado, "ultrasonido") == 0) {
        responderDistanciaActual(fuente);
        return;
    }

    if (strcmp(comandoNormalizado, "temperatura") == 0 || strcmp(comandoNormalizado, "temp") == 0) {
        responderTemperaturaActual(fuente);
        return;
    }

    if (strcmp(comandoNormalizado, "humedad") == 0 || strcmp(comandoNormalizado, "hum") == 0) {
        responderHumedadActual(fuente);
        return;
    }

    if (strcmp(comandoNormalizado, "luz") == 0 || strcmp(comandoNormalizado, "radiacion") == 0) {
        responderRadiacionActual(fuente);
        return;
    }

    if (strcmp(comandoNormalizado, "i2cscan") == 0 || strcmp(comandoNormalizado, "i2c") == 0) {
        responderEscaneoI2C(fuente);
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
    pinMode(HC_SR04_TRIG_PIN, OUTPUT);
    pinMode(HC_SR04_ECHO_PIN, INPUT);

    digitalWrite(HC_SR04_TRIG_PIN, LOW);
}

void configurarSensoresMonitoreo() {

    sensorDHT22.begin();

    Wire.begin();
    tsl2591Disponible = sensorTSL2591.begin();

    if (tsl2591Disponible) {
        sensorTSL2591.setGain(TSL2591_GAIN_MED);
        sensorTSL2591.setTiming(TSL2591_INTEGRATIONTIME_100MS);
    }

    actualizarSalidasRemoteXYSensores();
}

void configurarBluetoothSegunModo() {

    bluetoothSerial.begin(HC05_BAUD);

    if (MODO_APP_BLUETOOTH_ACTIVO == MODO_APP_REMOTEXY) {
        configurarRemoteXY();
        return;
    }

    bluetoothSerial.listen();
}

void setup() {

    Serial.begin(SERIAL_USB_BAUD);
    configurarPinesSensores();
    configurarSensoresMonitoreo();
    configurarPinesMotores();
    configurarBluetoothSegunModo();

    delay(ESPERA_INICIO_MS);
    actualizarSensoresPeriodicamente();

    Serial.println(F("=== DIAGNOSTICO MODULAR ROBOT 2WD ==="));
    Serial.print(F("Modo Bluetooth activo: "));
    Serial.println(nombreModoAppBluetooth(MODO_APP_BLUETOOTH_ACTIVO));
    Serial.println(F("Linea FC-51 I=13 D=12 disponibles"));
    Serial.println(F("HC-SR04 en 7/8, DHT22 en A0, TSL2591 en A4/A5"));
    Serial.println(F("Motores L298N listos en 5/11/10 y 6/9/4"));

    if (tsl2591Disponible) {
        Serial.println(F("TSL2591 detectado por I2C"));
    }
    else {
        Serial.println(F("TSL2591 no detectado; se conserva el ultimo dato valido"));
    }

    if (MODO_APP_BLUETOOTH_ACTIVO == MODO_APP_REMOTEXY) {
        Serial.println(F("Bluetooth dedicado a RemoteXY"));
        Serial.println(F("RemoteXY usa joystick con velocidad fija 120 y giro filtrado desde 45 grados"));
        Serial.println(F("RemoteXY publica distancia, temperatura y radiacion sin detener el movimiento"));
        Serial.println(F("USB mantiene comandos de diagnostico"));
    }
    else {
        Serial.println(F("Comandos rapidos: U N L R X D E C I J K 1 2 3"));
        Serial.println(F("Configura envio con fin de linea LF o CR+LF"));
        responderFlash(FUENTE_BLUETOOTH, F("Diagnostico modular listo"));
        responderFlash(FUENTE_BLUETOOTH, F("Escribe ayuda para ver los comandos"));
    }

    Serial.println(F("Escribe ayuda para ver los comandos"));
    Serial.println();
}

// ---------------- BUCLE PRINCIPAL ----------------
void loop() {

    if (MODO_APP_BLUETOOTH_ACTIVO == MODO_APP_REMOTEXY) {
        atenderRemoteXY();
    }
    else {
        detenerMotoresPorInactividadBluetooth();

        if (leerLinea(bluetoothSerial, bluetoothBuffer, bluetoothIndex)) {
            procesarComando(bluetoothBuffer, FUENTE_BLUETOOTH);
        }
    }

    if (leerLinea(Serial, usbBuffer, usbIndex)) {
        procesarComando(usbBuffer, FUENTE_USB);
    }

    actualizarSensoresPeriodicamente();
}