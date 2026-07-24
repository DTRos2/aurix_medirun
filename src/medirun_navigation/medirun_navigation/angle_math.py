import math


def normalize_angle(angle: float) -> float:
    """Wrap any angle to the [-pi, pi] range."""
    wrapped = math.fmod(angle + math.pi, 2.0 * math.pi)
    if wrapped < 0.0:
        wrapped += 2.0 * math.pi
    return wrapped - math.pi


def shortest_angular_distance(from_angle: float, to_angle: float) -> float:
    """Return the signed smallest distance from one angle to another."""
    return normalize_angle(to_angle - from_angle)


def yaw_from_quaternion(quaternion) -> float:
    """Extract yaw from a geometry_msgs quaternion."""
    siny_cosp = 2.0 * (
        quaternion.w * quaternion.z + quaternion.x * quaternion.y
    )
    cosy_cosp = 1.0 - 2.0 * (
        quaternion.y * quaternion.y + quaternion.z * quaternion.z
    )
    return math.atan2(siny_cosp, cosy_cosp)
