Early Wildfire Detection System
 - An IoT-enabled early wildfire detection node combining environmental sensing with onboard AI vision analysis. 

//

Overview

This project is mainly encouraged by the idea of wildfires. Wildfire has become more and more of an issue, especially affecting areas that are extremely dry. Over the past two decades, wildfires have increased rapidly and have become common. This fire detection project aims to solve the wildfire problems by having an "early" detection of wildfires. In this project, it uses the PMS2.5 sensor to detect any changes in air quality. If it exceeds a threshold, the ESP32-Cam activates. Then it uses AI detection to check and confirm if there actually is fire.

Main Issue Wildfires spread rapidly, often before traditional thermal cameras or satellite imagery can spot them.
The Goal this project tends to aim: A low-cost, multi-sensor node that continuously monitors air quality (PMS2.5) and triggers visual detection, an AI image classification model before alerting authorities or sending data online.

//

Features:

Multi-Sensor triggers: Monitors change in air quality (PMS2.5) to detect early smoke signatures.
AI Detection: Runs a custom-trained model (Yolov11) to visually confirm the smoke/flames.
Cloud: Transmits alerts and image data online for real-time monitoring without local SD storage dependencies.
Low Power: Optimized logic flow to keep the project active only when necessary --> saves power

//

Hardware

Camera: ESP32-CAM
Sensor: PMS2.5 Dust/Smoke Sensor
Power Supply: Solar Panel, 3v/5v battery

Software/Cloud
Language: C++ / Arduino Framework
AI: Custom Model (YOLOv11) ; Roboflow

//

System Architecture

The PMS2.5 sensor triggers every 15 seconds - the threshold is set to a value
If the threshold is exceeded, the system will then trigger the ESP32-CAM.
This camera then captures a frame. AI will analyze the frame and detect .
If AI confirms the fire, the frame will be uploaded to cloud and an email will be sent to the recipient.
The email will contain the location, picture and basically acts like an alarm or warning, notifying the recipient of an early detection and to seek immediate action to prevent the fire from starting a more chaotic wildfire.
If AI confirms it ISN'T a fire, it'll go back to 15 seconds interval of the PMS2.5 sensor.
To prevent overload of emails being sent, there is a email cooldown for approximately 60 seconds. 
However, if during this interval, the fire is confirmed and sure, it'll still send to prevent wildfires.
