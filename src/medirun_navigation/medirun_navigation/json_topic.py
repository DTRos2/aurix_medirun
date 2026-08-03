from __future__ import annotations

import json
from typing import Any, Dict


def load_json_message(raw: str) -> Dict[str, Any]:
    if not raw:
        return {}
    return json.loads(raw)


def dump_json_message(payload: Dict[str, Any]) -> str:
    return json.dumps(payload, ensure_ascii=True, sort_keys=True)
