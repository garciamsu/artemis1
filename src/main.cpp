#include <Arduino.h>

/*
=========================================================
BASTÓN INTELIGENTE - VERSIÓN FINAL CON PRIORIDAD
---------------------------------------------------------
Prioridad de riesgo:
1) Desnivel (mayor riesgo → caída)
2) Obstáculo frontal
3) Sin peligro

Sistema redundante:
- Buzzer (auditivo)
- Motor vibrador (háptico)
=========================================================
*/

// ---------------- PINES ----------------
#define BUZZER 7
#define MOTOR 6

#define TRIG_FRONT 8
#define ECHO_FRONT 9

#define TRIG_GROUND 10
#define ECHO_GROUND 11

// ---------------- FUNCIÓN DE LECTURA ----------------
/*
Lee distancia en centímetros desde un sensor HC-SR04.
Devuelve:
- Distancia en cm si la lectura es válida
- -1 si no hay eco (timeout)
*/
long readDistance(int trigPin, int echoPin) {

    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);

    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);

    long duration = pulseIn(echoPin, HIGH, 30000); // timeout 30 ms

    if (duration == 0) {
        return -1;  // no se recibió eco
    }

    long distance = duration * 0.034 / 2;
    return distance;
}

// ---------------- ALERTA FUERTE ----------------
/*
Se activa cuando hay DESNIVEL.
Patrón rápido tipo emergencia.
*/
void alertStrong() {

    digitalWrite(BUZZER, HIGH);
    analogWrite(MOTOR, 255);   // vibración máxima
    delay(120);

    digitalWrite(BUZZER, LOW);
    analogWrite(MOTOR, 0);
    delay(120);
}

// ---------------- ALERTA MEDIA ----------------
/*
Se activa cuando hay obstáculo frontal
y NO hay desnivel.
Patrón más lento.
*/
void alertMedium() {

    digitalWrite(BUZZER, HIGH);
    analogWrite(MOTOR, 180);   // vibración media
    delay(400);

    digitalWrite(BUZZER, LOW);
    analogWrite(MOTOR, 0);
    delay(400);
}

// ---------------- CONFIGURACIÓN ----------------
void setup() {

    Serial.begin(9600);

    pinMode(BUZZER, OUTPUT);
    pinMode(MOTOR, OUTPUT);

    pinMode(TRIG_FRONT, OUTPUT);
    pinMode(ECHO_FRONT, INPUT);

    pinMode(TRIG_GROUND, OUTPUT);
    pinMode(ECHO_GROUND, INPUT);
}

// ---------------- BUCLE PRINCIPAL ----------------
void loop() {

    long distanceFront  = readDistance(TRIG_FRONT, ECHO_FRONT);
    long distanceGround = readDistance(TRIG_GROUND, ECHO_GROUND);

    // Mostrar valores en monitor serial
    Serial.print("Frontal: ");
    Serial.print(distanceFront);
    Serial.print(" cm | Suelo: ");
    Serial.print(distanceGround);
    Serial.println(" cm");

    /*
    =====================================================
    LÓGICA DE PRIORIDAD
    =====================================================

    1️⃣ Si hay desnivel → ALERTA FUERTE
       (tiene prioridad por riesgo de caída)

    2️⃣ Si NO hay desnivel y hay obstáculo frontal
       → ALERTA MEDIA

    3️⃣ Si ninguno está activo → sin alerta
    */

    // 🔴 PRIORIDAD 1: DESNIVEL
    if (distanceGround >= 90) {
        alertStrong();
    }

    // 🟠 PRIORIDAD 2: OBSTÁCULO FRONTAL
    else if (distanceFront > 0 && distanceFront <= 50) {
        alertMedium();
    }

    // 🟢 SIN PELIGRO
    else {
        digitalWrite(BUZZER, LOW);
        analogWrite(MOTOR, 0);
        delay(100);
    }
}