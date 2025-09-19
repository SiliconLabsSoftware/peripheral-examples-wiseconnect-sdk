# Peripheral Example - Config Timer - PWM Generator #

![Type badge](https://img.shields.io/badge/Type-Application%20Examples-green)
![Technology badge](https://img.shields.io/badge/Technology-Peripheral-green)
![License badge](https://img.shields.io/badge/License-Zlib-green)
![SDK badge](https://img.shields.io/badge/SDK-v2025.6.0-green)
![Build badge](https://img.shields.io/badge/Build-passing-green)
![Flash badge](https://img.shields.io/badge/Flash-40.46%20KB-blue)
![RAM badge](https://img.shields.io/badge/RAM-18.63%20KB-blue)

## Summary ##

This project demonstrates how to generate a PWM signal using the Config Timer peripheral. The timer is configured to toggle a GPIO pin at a specified frequency and duty cycle, creating a PWM output suitable for controlling devices such as motors, LEDs, or other peripherals requiring pulse-width modulation.

## SDK Version ##

- [SiSDK v2025.6.0](https://github.com/SiliconLabs/simplicity_sdk/releases/tag/v2025.6.0)

## Software Required ##

- [Simplicity Studio v5 IDE](https://www.silabs.com/developers/simplicity-studio)

## Hardware Required ##

- 1x Silicon Labs Si91x device, such as:
  - [SIWX917-DK2605A](https://www.silabs.com/development-tools/wireless/wi-fi/siwx917-dk2605a-wifi-6-bluetooth-le-soc-dev-kit)
  - [SIWX917-RB4338A](https://www.silabs.com/development-tools/wireless/wi-fi/siwx917-rb4338a-wifi-6-bluetooth-le-soc-radio-board?tab=overview)
- An oscilloscope or logic analyzer to observe the PWM output.

## Connections Required ##

- Connect the PWM output pin (for example, GPIO_29) to the device you want to control or to an oscilloscope/logic analyzer for measurement.

  ![brd4338a_connectors](image/brd4338a_connectors.png)

  ![brd2605a_connectors](image/brd2605a_connectors.png)

> [!TIP]
> Refer to the official Silicon Labs documentation for correct hardware layout and pin mapping.

## Setup ##

### Create from EXAMPLE PROJECTS & DEMOS ###

1. From the Launcher Home, add your hardware to My Products, click on it, and click on the EXAMPLE PROJECTS & DEMOS tab. Find the example project by filtering for "config timer - PWM generator".
2. Create the project in Simplicity Studio.

### Create from an empty example project ###

1. Create an "Empty C Project" for your board using Simplicity Studio v5. Use the default project settings.

2. Copy `app.c` into the project root folder (overwriting existing file)

## How It Works ##

The Config Timer is set up to generate a PWM signal by toggling a GPIO pin at a specified frequency and duty cycle. The timer counts up to a value corresponding to the desired period, then toggles the output pin to create the high and low phases of the PWM waveform. By adjusting the timer's compare values, you can control both the frequency and the duty cycle of the PWM output.

## Testing ##

Connect the PWM output pin to an oscilloscope or logic analyzer. Observe the waveform and verify that the frequency and duty cycle match the configured values. You should see a stable PWM signal similar to the following:

![result](image/result.png)

## Reporting Bugs/Issues and Posting Questions and Comments ##

To report bugs in the Application Examples projects, please create a new "Issue" in the "Issues" section of this repo. Please reference the board, project, and source files associated with the bug, and reference line numbers. If you are proposing a fix, also include information on the proposed fix. Since these examples are provided as-is, there is no guarantee that these examples will be updated to fix these issues.

Questions and comments related to these examples should be made by creating a new "Issue" in the "Issues" section of this repo.
