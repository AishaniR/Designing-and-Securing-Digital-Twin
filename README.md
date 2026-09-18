# Designing-and-Securing-Digital-Twin
Secure IoT Digital Twin Indoor Monitoring System
This repository contains the implementation developed for an MSc dissertation
on secure IoT-based Digital Twin technology for indoor environmental monitoring.

## System Components

- Arduino Uno
- DHT22 temperature and humidity sensor
- MQ135 air-quality sensor
- Servo motor for ventilation control
- ESP32-S3-WROOM-2
- Eclipse Ditto Digital Twin
- Eclipse Mosquitto MQTT Broker
- MQTT over TLS (MQTTS)

## Cybersecurity Mechanisms

The system implements four security layers:

1. Secure Communication using TLS/MQTTS
2. Device and Service Authentication
3. MQTT Topic Authorization using ACLs
4. Eclipse Ditto Policy Authorization

## Communication Flow

Arduino → ESP32 → MQTTS → Mosquitto → Eclipse Ditto

Eclipse Ditto → Mosquitto → ESP32 → Arduino

## Notes

Sensitive credentials, passwords and private cryptographic keys
are included in this repository for experimental purposes only.
