#!/usr/bin/env python3
"""SOS 버튼 모듈 벤치 테스트: ROS2 없이 시리얼 출력만 눈으로 확인한다.
사용법: python3 bench_test.py --port /dev/ttyUSB0
"""
import argparse
import time

import serial


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", required=True)
    parser.add_argument("--baud", type=int, default=9600)
    args = parser.parse_args()

    last_seen_at = {}

    with serial.Serial(args.port, args.baud, timeout=1) as ser:
        print(f"[{args.port}] 연결됨, READY 대기 중...")
        while True:
            line = ser.readline().decode("utf-8", errors="replace").strip()
            if not line:
                continue

            now = time.monotonic()
            if line == "READY":
                print(f"[READY] 보드 초기화 완료")
                continue

            gap = None
            if line in last_seen_at:
                gap = now - last_seen_at[line]
            last_seen_at[line] = now

            gap_note = f"(이전 입력 후 {gap:.2f}s)" if gap is not None else ""
            print(f"[{time.strftime('%H:%M:%S')}] {line} {gap_note}")


if __name__ == "__main__":
    main()
