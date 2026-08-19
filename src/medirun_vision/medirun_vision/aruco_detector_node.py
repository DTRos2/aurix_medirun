from __future__ import annotations

import math
from typing import Optional

import cv2
import numpy as np
import rclpy
import tf2_ros
from cv_bridge import CvBridge
from rclpy.node import Node
from rclpy.time import Time
from sensor_msgs.msg import CameraInfo, Image

from medirun_msgs.msg import ArucoMarker

# OpenCV 4.5.x (ROS2 Humble / Ubuntu 22.04 apt 배포판)의 레거시 함수형 aruco API.
# 4.7+에서 도입된 cv2.aruco.ArucoDetector 클래스형 API는 이 버전에 없다.


class ArucoDetectorNode(Node):
    def __init__(self) -> None:
        super().__init__("aruco_detector_node")

        # 여러 개를 등록해두면 그중 아무거나 먼저 보이는 것을 홈 마커로
        # 취급한다(같은 위치에 여러 개 붙여둔 경우의 여분/재시도용).
        self.declare_parameter("home_aruco_ids", [14, 15])
        self.declare_parameter("marker_size_m", 0.10)
        self.declare_parameter("camera_frame", "camera_link")
        self.declare_parameter("base_frame", "base_link")
        self.declare_parameter("dictionary", "DICT_4X4_50")
        # 실측 결과 yaw_error가 반대로 나와서 기본값을 -1.0으로 뒤집어뒀다.
        # 여전히 반대 방향으로 돌면 1.0으로, lateral도 마찬가지면 -1.0으로.
        self.declare_parameter("yaw_sign", -1.0)
        self.declare_parameter("lateral_sign", 1.0)

        self._bridge = CvBridge()
        self._camera_matrix: Optional[np.ndarray] = None
        self._dist_coeffs: Optional[np.ndarray] = None
        self._camera_info_frame_id: str = ""

        dictionary_name = str(self.get_parameter("dictionary").value)
        dictionary_id = getattr(cv2.aruco, dictionary_name)
        self._aruco_dict = cv2.aruco.Dictionary_get(dictionary_id)
        self._aruco_params = cv2.aruco.DetectorParameters_create()

        self._tf_buffer = tf2_ros.Buffer()
        self._tf_listener = tf2_ros.TransformListener(self._tf_buffer, self)
        self._warned_no_camera_info = False
        self._warned_no_tf = False

        self._pose_pub = self.create_publisher(ArucoMarker, "/aruco/marker_pose", 10)
        self.create_subscription(CameraInfo, "/camera/camera_info", self._handle_camera_info, 10)
        self.create_subscription(Image, "/camera/image_raw", self._handle_image, 10)

    def _handle_camera_info(self, msg: CameraInfo) -> None:
        self._camera_matrix = np.array(msg.k, dtype=np.float64).reshape(3, 3)
        self._dist_coeffs = np.array(msg.d, dtype=np.float64)
        self._camera_info_frame_id = msg.header.frame_id

    def _handle_image(self, msg: Image) -> None:
        if self._camera_matrix is None:
            if not self._warned_no_camera_info:
                self.get_logger().warning("camera_info를 아직 받지 못해 마커 검출을 건너뜁니다.")
                self._warned_no_camera_info = True
            return

        frame = self._bridge.imgmsg_to_cv2(msg, desired_encoding="bgr8")
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        corners, ids, _ = cv2.aruco.detectMarkers(gray, self._aruco_dict, parameters=self._aruco_params)

        out = ArucoMarker()
        out.header = msg.header
        out.id = 0
        out.detected = False

        target_ids = {int(i) for i in self.get_parameter("home_aruco_ids").value}
        if ids is not None:
            for corner, marker_id in zip(corners, ids.flatten()):
                if int(marker_id) not in target_ids:
                    continue
                out.id = int(marker_id)
                self._fill_pose(out, corner)
                break

        self._pose_pub.publish(out)

    def _fill_pose(self, out: ArucoMarker, corner: np.ndarray) -> None:
        marker_size = float(self.get_parameter("marker_size_m").value)
        rvecs, tvecs, _ = cv2.aruco.estimatePoseSingleMarkers(
            [corner], marker_size, self._camera_matrix, self._dist_coeffs
        )
        tvec = tvecs[0][0]
        rvec = rvecs[0][0]

        distance = float(tvec[2])
        lateral_error = float(tvec[0])

        rotation_matrix, _ = cv2.Rodrigues(rvec)
        # 부호는 카메라 장착 방향에 따라 반대로 나올 수 있어(실측으로만 확인
        # 가능) yaw_sign/lateral_sign 파라미터로 뒤집을 수 있게 해뒀다
        # (2026-08-19: 실측 결과 yaw가 반대로 나와 기본값을 -1.0으로 바꿈).
        yaw_sign = float(self.get_parameter("yaw_sign").value)
        lateral_sign = float(self.get_parameter("lateral_sign").value)
        yaw_error = yaw_sign * math.atan2(rotation_matrix[0][2], rotation_matrix[2][2])
        lateral_error *= lateral_sign

        base_frame = str(self.get_parameter("base_frame").value)
        camera_frame = str(self.get_parameter("camera_frame").value)
        if base_frame != camera_frame:
            transformed = self._transform_to_base(distance, lateral_error, base_frame, camera_frame)
            if transformed is not None:
                distance, lateral_error = transformed

        out.detected = True
        out.distance = distance
        out.lateral_error = lateral_error
        out.yaw_error = yaw_error

    def _transform_to_base(
        self, distance: float, lateral_error: float, base_frame: str, camera_frame: str
    ) -> Optional[tuple]:
        try:
            transform = self._tf_buffer.lookup_transform(base_frame, camera_frame, Time())
        except (tf2_ros.LookupException, tf2_ros.ConnectivityException, tf2_ros.ExtrapolationException):
            if not self._warned_no_tf:
                self.get_logger().warning(
                    "%s -> %s TF를 찾을 수 없어 카메라 프레임 값을 그대로 사용합니다. "
                    "거리 오차가 트레일러 전장만큼 누적될 수 있습니다." % (base_frame, camera_frame)
                )
                self._warned_no_tf = True
            return None

        t = transform.transform.translation
        q = transform.transform.rotation
        # 카메라는 base_link 정면을 보도록 장착된다고 가정하고, 평면(x-y) 오프셋만 반영한다.
        yaw = math.atan2(2.0 * (q.w * q.z + q.x * q.y), 1.0 - 2.0 * (q.y * q.y + q.z * q.z))
        cos_y, sin_y = math.cos(yaw), math.sin(yaw)
        camera_x = distance
        camera_y = -lateral_error
        base_x = t.x + camera_x * cos_y - camera_y * sin_y
        base_y = t.y + camera_x * sin_y + camera_y * cos_y
        return math.hypot(base_x, base_y), -base_y


def main() -> None:
    rclpy.init()
    node = ArucoDetectorNode()
    try:
        rclpy.spin(node)
    finally:
        node.destroy_node()
        rclpy.shutdown()
