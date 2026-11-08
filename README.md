# MPU-6050 Linux Kernel I2C Device Driver

A Linux Kernel driver that exposes an MPU-6050 IMU as `/dev/mpu6050` character device file, plus a small userspace reader.

## DEMO

![](images/demo1.gif)

Top Left: leading the kernel driver module and device tree overlay. Bottom Left: userspace reader program reading sensor values device driver. Right: kernel log (`dmesg -w`).

## Hardware and Wiring

![](images/wiring.jpeg)

| MPU-6050 pin | Pi Zero 2W pin | Function |
| --- | --- | --- |
| VCC | Pin 1| 3.3V power |
| GND | Pin 6 | Ground |
| SDA | Pin 3 (GPIO 2) | I2C data |
| SCL | Pin 5 (GPIO 3) | I2C clock |

