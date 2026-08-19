# Product

## Register
product

## Users
Robotics engineers, embedded firmware developers, and automation makers operating and tuning a 6-axis closed-loop robotic arm / multi-axis mechanism over Wi-Fi (AP/STA) and Serial CLI.

## Product Purpose
Deliver high-precision, low-latency, real-time control, diagnostics, and telemetry for 6 NEMA stepper motors with TMC2209 drivers and AS5600 absolute encoders. Allows individual axis manual jog/step/spin, closed-loop angle tracking with Schmitt deadband, synchronized 6-axis arrival, auto-calibration, and hardware telemetry with zero downtime.

## Brand Personality
Precise, Industrial, Responsive, Trustworthy.

## Anti-references
- Cluttered, laggy hobbyist web interfaces with unstyled default HTML buttons.
- Over-decorated AI slop (glassmorphism, gradient text, arbitrary glow animations, nested cards).
- Slow polling dashboards that freeze under high network traffic.

## Design Principles
1. **Zero-Latency Visual Feedback**: Real-time dials and angle readouts update smoothly at high frequency with tabular numerical alignment.
2. **Clear Affordances & Safety**: Critical actions like Emergency Stop, Driver Enable/Free-Shaft, and Runaway Invert warnings are unambiguous and instant.
3. **Task-Focused Density**: High information density without visual clutter; tabs isolate deep per-joint tuning while overview grid surfaces all 6 axes simultaneously.
4. **State-Rich Semantic Hierarchy**: Unmistakable badges and indicators for Wi-Fi, Homing, Calibration, Driver status, and UART telemetry.

## Accessibility & Inclusion
- High contrast ratio (>= 4.5:1 for body and data text against dark industrial background).
- Keyboard accessibility and touch-friendly controls (min 44px hit targets) for mobile/tablet workshop operation.
- Monospace tabular numbers for all angle, step, and error displays to prevent layout jitter.
