# 🌱 Smart Agriculture IoT System
### 5FTC2140 – Connected Systems and IoT

This repository contains the implementation of our **Smart Agriculture IoT System** developed for the **5FTC2140 – Connected Systems and IoT module**.

The project demonstrates how multiple IoT technologies can be integrated to monitor environmental conditions and agricultural parameters using distributed sensor nodes and a cloud-connected dashboard.

The system consists of:

• Two sensing nodes  
• One gateway device  
• Firebase cloud database  
• Web dashboard for monitoring  

Sensor nodes communicate with the gateway using **LoRa**, and the gateway sends data to the cloud through **Wi‑Fi**.

---

# 📡 Project Overview

The goal of this project was to design and integrate a **multi‑node IoT monitoring system** suitable for agriculture environments.

Main functions of the system:

• Monitor environmental conditions  
• Monitor soil‑related data  
• Transmit sensor data using LoRa  
• Process data at a central gateway  
• Upload real‑time data to Firebase  
• Display information through a web dashboard  

---

# 🧠 Coursework Context

This project is part of the **Connected Systems and IoT module coursework**.

### CW1 – Communication Protocols
In CW1 we studied different **communication protocols used in IoT systems**, including:

• LoRa  
• Wi‑Fi  
• Ethernet  
• Bluetooth  

This helped us understand how communication works in connected systems.

### CW2 – System Integration
In CW2 we applied that knowledge to build a **complete IoT system**, integrating sensors, microcontrollers, communication modules, cloud services, and a dashboard.

This repository contains all files related to the CW2 implementation.

---

# 🏗 System Architecture
Node 1 [Lora]
│ 
▼
Node 2 [Lora]
│ 
▼
Gateway
│ 
▼
Wi‑Fi                         
│                   
▼                           
Firebase Cloud                        
│                   
▼
Web Dashboard 

---

# 🔧 System Components

## Node 1 – Environmental Monitoring Node

Hardware:

• Arduino Nano  
• LDR sensor  
• DHT11 temperature & humidity sensor  
• LoRa module  

Functions:

• Read light intensity  
• Read temperature and humidity  
• Send data to the gateway using LoRa  

---

## Node 2 – Soil Monitoring Node

Hardware:

• ESP32‑C3  
• Soil moisture sensor  
• Flow sensor  
• Pump / relay module  
• LoRa module  

Functions:

• Monitor soil conditions  
• Monitor water flow  
• Send sensor data to the gateway using LoRa  

---

## Gateway – Central Node

Hardware:

• ESP32  
• LoRa module  
• OLED display  
• Wi‑Fi connectivity  

Functions:

• Receive LoRa packets from nodes  
• Decode sensor data  
• Display system status on OLED  
• Upload sensor data to Firebase  
• Synchronize with dashboard  

---

## Web Dashboard

Technologies:

• HTML  
• CSS  
• JavaScript  
• Firebase  

Functions:

• Display sensor readings in real time  
• Show system status  
• Provide monitoring interface  

---

# 📡 Communication Technologies

## LoRa
LoRa is used for long‑range communication between the nodes and the gateway.

Advantages:

• Long communication range  
• Low power consumption  
• Suitable for agriculture environments  

---

## Wi‑Fi
Wi‑Fi is used by the gateway to connect to the internet and send data to Firebase.

Uses:

• Cloud data upload  
• Dashboard updates  
• Remote monitoring  

---

# ☁ Cloud Platform

## Firebase

Firebase is used as the cloud database for real‑time data storage.

Firebase allows:

• storing live sensor data  
• synchronizing with the dashboard  
• updating system values in real time  

---

# 📂 Repository Structure

5FTC2140-Connected-Systems-and-IoT

node1/
Node1_Code.ino

node2/
Node2_Code.ino

gateway/
Gateway_Code.ino

dashboard/
index.html
style.css
script.js

images/

docs/
CW2_Report.pdf

README.md

---


# 🧪 Testing

The system was tested for:

• LoRa communication reliability  
• Sensor reading accuracy  
• Gateway data processing  
• Firebase updates  
• Dashboard visualization  

Testing screenshots and results are included in this repository.

---

# ⚠ Known Issue

Manual pump control from the dashboard updates Firebase and the gateway, but Node 2 does not always respond correctly.

This behaviour was observed during system testing and is documented for evaluation.

---

# 🚀 Future Improvements

Possible improvements:

• improve remote irrigation control reliability  
• implement automated irrigation logic  
• reduce power consumption of nodes  
• improve dashboard visualization  
• add more sensor nodes  

Possible future communication extensions:

• Bluetooth for local device configuration  
• Ethernet for stable gateway connectivity  

---

# 👨‍💻 Contribution

Main contribution areas:

• Gateway system development  
• Dashboard implementation  
• Firebase integration  
• System testing and debugging  

---

# 🔗 GitHub Repository

https://github.com/Dulina10/5FTC2140-Connected-Systems-and-IoT

---

# 📘 Academic Note

This repository was created for academic coursework for the **Connected Systems and IoT module**.  
It documents the design, implementation, and testing of the Smart Agriculture IoT system.
