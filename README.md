# esp32_linkedList

Purpose of this program is to create a schedule in real time. Program reads the strings(schedules) stored in esp32's NVS storage, sorts the schedule and the next event to be scheduled is printed. Schedules can be added in real time, using BLE commands to write to NVS storage.

Project demonstrates:
- BLE server/client read/write understanding
- Linked list - create, update, sort, remove elements
- NVS read/write understanding
