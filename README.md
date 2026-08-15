Early Wildfire Detection System

> An IoT-enabled early wildfire detection node combining environmental sensing with onboard AI vision analysis.

---

Overview

#Briefly describe what your project is, the problem it solves, and why you built it. 

* **The Problem:** Wildfires spread rapidly, often before traditional thermal cameras or satellite imagery can spot them.
* **The Solution:** A low-cost, multi-sensor node that continuously monitors air quality (PM2.5) and triggers visual validation via an AI image classification model before alerting authorities or sending data online.

---

## ⚡ Key Features

* **Multi-Sensor Trigger:** Monitors particulate matter (PM2.5) to detect early smoke signatures.
* **AI Computer Vision:** Runs a custom-trained model (e.g., YOLO) to visually confirm smoke/flames.
* **Cloud Integration:** Transmits alerts and image data online for real-time monitoring without local SD storage dependencies.
* **Low Power/Autonomous:** Optimized logic flow to keep the node active only when necessary.

---

## 🛠️ Hardware & Tech Stack

### Hardware
* **Microcontroller:** ESP32-CAM
* **Sensors:** PM2.5 Dust/Smoke Sensor
* **Power Supply:** [e.g., LiPo Battery / Solar Panel / 5V Adapter]

### Software & Cloud
* **Language:** C++ / Arduino Framework
* **AI/ML:** Custom Model (YOLO / Edge Impulse / Python training framework)
* **Cloud Platform:** [e.g., Blynk / Firebase / Custom Webhook]

---

## ⚙️ System Architecture

Provide a high-level flowchart or text diagram explaining how the hardware, AI, and cloud communicate.
