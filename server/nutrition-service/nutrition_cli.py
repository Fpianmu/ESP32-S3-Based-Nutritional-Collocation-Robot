from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any

from nutrition_service.llm_parser import load_llm_parse_config
from nutrition_service.service import NutritionService
from nutrition_service.storage import SQLiteFoodRecordStore


def run_once(
    service: NutritionService,
    user_id: str,
    text: str,
    record_date: str | None = None,
) -> dict[str, Any]:
    parsed = service.parse_intake(user_id, text)
    recommendation = service.confirm_and_recommend(
        user_id=user_id,
        date=record_date or service.today(),
        confirmed_items=parsed["items"],
        dough_request=parsed["dough_request"],
    )
    return {"parsed": parsed, "recommendation": recommendation}


def main() -> None:
    parser = argparse.ArgumentParser(description="本地测试营养推荐服务")
    parser.add_argument("--user-id", default="popkik")
    parser.add_argument("--text")
    parser.add_argument("--date")
    parser.add_argument("--llm", action="store_true", help="启用 LLM 解析，失败时自动回退规则解析")
    args = parser.parse_args()

    llm_config = load_llm_parse_config()
    if args.llm:
        llm_config.enabled = True

    service = NutritionService(
        SQLiteFoodRecordStore(Path(__file__).resolve().parent / "data" / "nutrition.db"),
        llm_config=llm_config,
    )
    user_id = args.user_id or input("用户ID（默认 popkik）：").strip() or "popkik"
    text = args.text or input("请输入今天吃了什么，以及想要的面团重量：").strip()
    payload = run_once(service, user_id=user_id, text=text, record_date=args.date)
    print(json.dumps(payload, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
