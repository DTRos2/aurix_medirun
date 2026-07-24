# Straight Test Report

## Summary

- Test ID: T-01
- Executed at: 2026-07-24 14:49:18
- Repeat count: 10
- Requested distance (m): 0.450
- Command compensation (m): 0.000
- Linear speed (m/s): 0.10
- Min linear speed (m/s): 0.05
- Slowdown distance (m): 0.150
- Distance tolerance (m): 0.010
- Repeat delay (sec): 3.0
- Odom topic: /odom
- IMU topic: /imu

## Results

| Run | Request (m) | Target (m) | Measured (m) | Error (m) | Residual (m) | Lateral Error (m) | Odom Yaw Change (deg) | IMU Yaw Change (deg) |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 2 | 0.450 | 0.450 | 0.452 | 0.002 | -0.002 | -0.001 | -0.07 | -0.07 |
| 3 | 0.450 | 0.450 | 0.452 | 0.002 | -0.002 | -0.000 | -0.20 | -0.20 |
| 4 | 0.450 | 0.450 | 0.451 | 0.001 | -0.001 | -0.000 | -0.07 | -0.07 |
| 5 | 0.450 | 0.450 | 0.451 | 0.001 | -0.001 | 0.001 | -0.14 | -0.14 |
| 6 | 0.450 | 0.450 | 0.453 | 0.003 | -0.003 | -0.000 | 0.12 | 0.12 |
| 7 | 0.450 | 0.450 | 0.453 | 0.003 | -0.003 | 0.000 | 0.05 | 0.05 |
| 8 | 0.450 | 0.450 | 0.452 | 0.002 | -0.002 | 0.001 | -0.31 | -0.31 |
| 9 | 0.450 | 0.450 | 0.451 | 0.001 | -0.001 | -0.000 | 0.06 | 0.06 |
| 10 | 0.450 | 0.450 | 0.452 | 0.002 | -0.002 | 0.000 | 0.09 | 0.09 |
| 11 | 0.450 | 0.450 | 0.452 | 0.002 | -0.002 | 0.001 | -0.35 | -0.35 |

## Statistics

- Average distance error (m): 0.002
- Average absolute distance error (m): 0.002
- Average lateral error (m): 0.000
- Average absolute lateral error (m): 0.001
- Average odom yaw change (deg): -0.082
- Average imu yaw change (deg): -0.081

## Notes

- One floor block is treated as `0.45 m` for this test.
- If the robot consistently stops short in real-world measurement, increase `command_compensation_m`.
- Compare odom distance with floor markings or a tape measure.
