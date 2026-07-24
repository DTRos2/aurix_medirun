# Rotation Test Report

## Summary

- Test ID: T-00
- Executed at: 2026-07-24 13:24:04
- Repeat count: 10
- Requested angles (deg): [90.0]
- Command compensation (deg): 0.00
- Angular speed (rad/s): 0.35
- Min angular speed (rad/s): 0.20
- Repeat delay (sec): 3.0
- Odom topic: /odom
- IMU topic: /imu

## Results

| Run | Step | Request (deg) | Target (deg) | Odom Measured (deg) | Odom Error (deg) | Odom Residual (deg) | IMU Measured (deg) | IMU Error (deg) |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | 1 | 90.00 | 90.00 | 87.37 | -2.63 | 2.63 | 87.37 | -2.63 |
| 2 | 1 | 90.00 | 90.00 | 87.03 | -2.97 | 2.97 | 87.03 | -2.97 |
| 3 | 1 | 90.00 | 90.00 | 88.27 | -1.73 | 1.73 | 88.27 | -1.73 |
| 4 | 1 | 90.00 | 90.00 | 87.01 | -2.99 | 2.99 | 87.01 | -2.99 |
| 5 | 1 | 90.00 | 90.00 | 87.04 | -2.96 | 2.96 | 87.04 | -2.96 |
| 6 | 1 | 90.00 | 90.00 | 87.77 | -2.23 | 2.23 | 87.77 | -2.23 |
| 7 | 1 | 90.00 | 90.00 | 87.57 | -2.43 | 2.43 | 87.57 | -2.43 |
| 8 | 1 | 90.00 | 90.00 | 88.52 | -1.48 | 1.48 | 88.52 | -1.48 |
| 9 | 1 | 90.00 | 90.00 | 87.70 | -2.30 | 2.30 | 87.70 | -2.30 |
| 10 | 1 | 90.00 | 90.00 | 88.04 | -1.96 | 1.96 | 88.04 | -1.96 |

## Statistics

- Average odom error (deg): -2.37
- Average absolute odom error (deg): 2.37
- Average imu error (deg): -2.37
- Average absolute imu error (deg): 2.37

## Notes

- If the robot under-rotates in real-world measurement, increase `command_compensation_deg`.
- Compare this report with manual protractor or floor marker measurements.
