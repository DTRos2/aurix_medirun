from __future__ import annotations

from dataclasses import dataclass
from typing import Iterable, Optional


try:
    import serial  # type: ignore
    from serial import SerialException  # type: ignore
except ImportError:  # pragma: no cover - depends on runtime image
    serial = None

    class SerialException(Exception):
        pass


@dataclass
class SerialConfig:
    port: str
    baud_rate: int = 9600
    timeout_sec: float = 0.1


class SerialLineReader:
    def __init__(self, config: SerialConfig) -> None:
        self._config = config
        self._client = None

    def open(self) -> bool:
        if serial is None:
            return False
        self._client = serial.Serial(
            self._config.port,
            self._config.baud_rate,
            timeout=self._config.timeout_sec,
        )
        return True

    def read_line(self) -> Optional[str]:
        if self._client is None:
            return None
        try:
            raw = self._client.readline()
        except SerialException:
            return None
        if not raw:
            return None
        return raw.decode("utf-8", errors="replace").strip()

    def write_line(self, line: str) -> None:
        if self._client is None:
            return
        self._client.write(f"{line}\n".encode("utf-8"))

    def close(self) -> None:
        if self._client is None:
            return
        self._client.close()
        self._client = None


def parse_key_value_line(line: str) -> tuple[str, str]:
    if ":" not in line:
        return line.strip(), ""
    key, value = line.split(":", 1)
    return key.strip(), value.strip()


def cycle_mock_lines(lines: Iterable[str], index: int) -> str:
    values = list(lines)
    if not values:
        return ""
    return values[index % len(values)]
