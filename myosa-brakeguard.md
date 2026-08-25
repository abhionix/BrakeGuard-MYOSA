---
publishDate: 2026-08-25
title: BrakeGuard - A Continuous IoT-Based Capacitive Brake Fluid Moisture Health Monitoring System
excerpt: BrakeGuard is a low-cost embedded safety system that monitors brake fluid moisture and braking conditions to identify brake-fluid degradation and provide an early indication of brake fade risk.
image: image1.jpg
tags:
  - smart-sensing
  - condition-monitoring
  - automotive-safety
  - edge-iot
  - sensor-fusion
---
> Continuous brake-fluid health monitoring for safer, condition-based brake maintenance.

---

## Acknowledgements

This project was developed using the MYOSA Mini Kit and its sensor ecosystem as part of the IEEE Sensors Council MYOSA 6.0 Global Student Competition. We acknowledge the MYOSA platform for providing the embedded hardware and sensor modules that enabled the development and testing of BrakeGuard. We also thank our mentor Dr.Rajesh Kannan Megalingam for his support and encouragement during the prototyping and validation process.

---

## Overview

Brake fluid is a critical yet often overlooked part of a vehicle's braking system. Glycol-based brake fluids such as DOT 3, DOT 4, and DOT 5.1 gradually absorb moisture from their surroundings. As moisture increases, the fluid's boiling point decreases and the risk of thermal degradation and internal corrosion increases. Despite this continuous degradation, brake-fluid condition is typically assessed only during periodic maintenance, providing no continuous view of how the fluid is changing between service intervals.

BrakeGuard is designed for vehicle owners, automotive technicians, and safety-conscious drivers who want continuous visibility into their brake-fluid condition. It is especially valuable for performance vehicles, commercial fleets, and regions with high humidity where brake-fluid degradation accelerates.

BrakeGuard addresses this gap by converting brake-fluid condition into a continuously monitored safety parameter. A capacitive sensing unit detects changes in the electrical properties of the fluid associated with moisture contamination, while the ESP32-based MYOSA Mini Kit processes the measurement alongside braking-event data from the MPU6050 and environmental data from the BMP180.

Instead of reporting moisture content alone, BrakeGuard combines these measurements and evaluates them against experimentally established thresholds to generate a Brake Fade Risk Index. The resulting status is presented locally through the MYOSA OLED and can also be transmitted for remote monitoring and trend analysis.

The project demonstrates a low-cost approach to condition-based brake-fluid monitoring, bringing together capacitive sensing, sensor fusion, edge processing, and IoT telemetry to make a normally invisible maintenance concern measurable and actionable.

---

## Demo / Examples

### Images

<p align="center">
  <img src="image1.jpg" width="800"><br/>
  <i>BrakeGuard - Complete system overview</i>
</p>

<p align="center">
  <img src="image2.jpg" width="800"><br/>
  <i>BrakeGuard - System in action</i>
</p>

<!-- Add more images here when available -->

### Videos

<video controls width="100%">
  <source src="/myosa-demovideo.mp4" type="video/mp4">
</video>

<video controls width="100%">
  <source src="/myosa-demovideo2.mp4" type="video/mp4">
</video>

---

## Features (Detailed)

### 1. Continuous Brake-Fluid Health Sensing

BrakeGuard uses a custom parallel-plate capacitive sensing unit to monitor changes in brake-fluid condition. Two conductive plates form the sensing element, with the brake fluid occupying the controlled gap between them.

As the dielectric properties of the fluid change with increasing moisture content, the effective capacitance of the sensing unit changes. This electrical variation provides the primary sensing signal used by BrakeGuard.

<p align="center">
  <img src="feature1.jpg" width="800"><br/>
  <i>BrakeGuard capacitive sensing prototype</i>
</p>

### 2. RC-Based Capacitance Measurement

The sensing unit is interfaced with the ESP32 through an RC charge-time measurement circuit. Instead of requiring a dedicated capacitance-measurement IC, the system measures how quickly the sensing circuit charges towards a defined voltage level.

The charging behaviour of the RC circuit follows:

V(t) = VCC × (1 − e^(−t/RC))

where R is the known resistance and C represents the effective capacitance of the sensing unit.

As the capacitance changes, the charging time also changes. The ESP32 measures this time and uses the resulting electrical response as the sensing signal for brake-fluid condition.

### 3. Experimental Calibration

The raw capacitive response is not directly treated as a moisture percentage. BrakeGuard establishes a calibration relationship by measuring the sensing unit under controlled reference conditions.

The measured electrical response is mapped against known reference states to determine the usable operating range and condition thresholds. This experimental approach also allows factors such as probe geometry, fluid type, temperature, and other practical effects to be evaluated during calibration.

### 4. Braking-Aware Condition Monitoring

Brake-fluid condition becomes particularly important when the braking system is subjected to repeated or severe braking. BrakeGuard therefore uses the MPU6050 to detect significant deceleration events.

The acceleration signal is filtered and compared against experimentally selected thresholds. Braking events are then tracked over a defined time window, allowing both braking severity and braking frequency to contribute to the system's assessment.

This provides operational context that a standalone moisture sensor cannot provide.

### 5. Brake Fade Risk Index

BrakeGuard combines the measured brake-fluid condition with braking behaviour to produce a single Brake Fade Risk Index.

The current risk model combines three normalised parameters:

R = Wm × Sm + Wd × Sd + Wf × Sf

where R is the Brake Fade Risk Index, Sm represents the moisture-condition score, Sd represents deceleration severity, Sf represents braking frequency, and Wm, Wd, and Wf are their respective weights.

Multiple thresholds classify the resulting index into different operating states. This converts several raw sensor measurements into a simple, actionable indication of increasing brake-fluid-related risk.

### 6. Real-Time Edge Feedback

The sensing, filtering, and risk-assessment pipeline runs locally on the ESP32. The MYOSA OLED provides immediate feedback by displaying the current BrakeGuard status and selected sensor information.

The core safety indication therefore does not depend on a computer, cloud service, or continuous internet connection.

### 7. IoT Telemetry and Condition Tracking

BrakeGuard can transmit sensor readings, braking data, risk index, and system status over Wi-Fi using MQTT. This enables remote monitoring and long-term tracking of brake-fluid condition, supporting a shift from periodic inspection towards condition-based maintenance.

This creates the foundation for moving from periodic inspection towards condition-based maintenance, where changes in brake-fluid condition can be observed over time rather than only during scheduled servicing.

---

## Usage Instructions

<!-- TODO: Add operating and testing procedure after hardware validation -->

1. Connect the capacitive sensing unit to the ESP32
2. Power on the system
3. The OLED display will show the current brake-fluid status
4. Monitor remotely via MQTT dashboard

---

## Tech Stack

- **ESP32** - Main microcontroller (MYOSA Mini Kit)
- **MPU6050** - Accelerometer/Gyroscope for braking detection
- **BMP180** - Barometric pressure and temperature sensor
- **OLED Display** - Local status display
- **MQTT** - IoT communication protocol
- **Wi-Fi** - Wireless connectivity

---

## Requirements / Installation

### Hardware Requirements
- MYOSA Mini Kit (ESP32-based)
- MPU6050 sensor module
- BMP180 sensor module
- OLED display module
- Custom capacitive sensing unit
- Brake fluid samples (DOT 3/4/5.1)

### Software Requirements
- Arduino IDE or PlatformIO
- ESP32 board support package

### Libraries
```bash
Wire.h
Adafruit_GFX.h
Adafruit_SSD1306.h
MPU6050.h
Adafruit_BMP085.h
PubSubClient.h
WiFi.h
```

### Installation Steps
1. Install Arduino IDE
2. Add ESP32 board support
3. Install required libraries via Library Manager
4. Connect MYOSA Mini Kit via USB
5. Select correct board and port
6. Upload the firmware

---

## File Structure

```
/brakeguard-myosa
├─ myosa-brakeguard.md
├─ image1.jpg
├─ image2.jpg
├─ myosa-demovid.mp4
└─ firmware/
    └─ brakeguard.ino
```

---

## License

MIT License

---

## Contribution Notes

Contributions welcome. Open an issue or submit a pull request.
