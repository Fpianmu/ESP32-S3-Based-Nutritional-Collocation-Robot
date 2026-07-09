from __future__ import annotations

import json
import os
import re
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Protocol

import openai
import yaml

from .models import CONTROLLED_CATEGORIES, DEFAULT_WATER_RATIO, DoughRequest, FoodItem, ParseResult, UserProfile
from .parser import _detect_dough_weight


@dataclass
class LLMParseConfig:
    enabled: bool = False
    model_name: str = "deepseek-chat"
    base_url: str = "https://api.deepseek.com"
    api_key: str = ""
    timeout_seconds: float = 30.0


class LLMClient(Protocol):
    def parse(self, prompt: str) -> str:
        ...


class OpenAICompatibleLLMClient:
    def __init__(self, config: LLMParseConfig):
        self.config = config
        self.client = openai.OpenAI(
            api_key=config.api_key,
            base_url=config.base_url,
            timeout=config.timeout_seconds,
        )

    def parse(self, prompt: str) -> str:
        response = self.client.chat.completions.create(
            model=self.config.model_name,
            messages=[
                {
                    "role": "system",
                    "content": "你是饮食记录结构化解析器。只返回严格 JSON，不要输出解释。",
                },
                {"role": "user", "content": prompt},
            ],
            temperature=0,
            stream=False,
        )
        return response.choices[0].message.content or ""


def parse_with_llm(
    text: str,
    profile: UserProfile | None = None,
    client: LLMClient | None = None,
    config: LLMParseConfig | None = None,
) -> ParseResult | None:
    config = config or load_llm_parse_config()
    if not config.enabled:
        return None
    client = client or OpenAICompatibleLLMClient(config)
    try:
        raw = client.parse(_build_prompt(text))
        payload = _extract_json_object(raw)
        return _payload_to_parse_result(payload, profile or UserProfile(), text)
    except Exception:
        return None


def load_llm_parse_config(config_path: str | Path | None = None) -> LLMParseConfig:
    enabled = os.getenv("NUTRITION_LLM_ENABLED", "").lower() in {"1", "true", "yes"}
    model_name = os.getenv("NUTRITION_LLM_MODEL", "")
    base_url = os.getenv("NUTRITION_LLM_BASE_URL", "")
    api_key = os.getenv("NUTRITION_LLM_API_KEY", "")

    if not api_key:
        file_config = _read_xiaozhi_deepseek_config(config_path)
        model_name = model_name or file_config.get("model_name", "")
        base_url = base_url or file_config.get("base_url", "")
        api_key = file_config.get("api_key", "")

    return LLMParseConfig(
        enabled=enabled,
        model_name=model_name or "deepseek-chat",
        base_url=base_url or "https://api.deepseek.com",
        api_key=api_key,
    )


def _read_xiaozhi_deepseek_config(config_path: str | Path | None = None) -> dict[str, str]:
    path = Path(config_path) if config_path else Path(__file__).resolve().parents[2] / "main" / "xiaozhi-server" / "data" / ".config.yaml"
    if not path.exists():
        return {}
    try:
        data = yaml.safe_load(path.read_text(encoding="utf-8")) or {}
        deepseek = data.get("LLM", {}).get("DeepSeekLLM", {})
        return {
            "model_name": str(deepseek.get("model_name", "")),
            "base_url": str(deepseek.get("base_url", deepseek.get("url", ""))),
            "api_key": str(deepseek.get("api_key", "")),
        }
    except Exception:
        return {}


def _build_prompt(text: str) -> str:
    return f"""
请把下面用户饮食描述解析为 JSON。

只允许返回如下结构：
{{
  "items": [
    {{
      "meal": "breakfast|lunch|dinner|unknown",
      "food_name": "英文标识",
      "display_name": "中文食物名",
      "amount_text": "数量文本",
      "estimated_weight_g": 估算克数,
      "category": "refined_staple|coarse_grain|protein|vegetable|fruit|fat_sugar|unknown",
      "confidence": 0.0到1.0,
      "base_food": "主体食物，可为空",
      "modifier": "修饰词，可为空"
    }}
  ],
  "dough_request": {{
    "dough_total_weight_g": 可选整数,
    "target_food": "noodles|dumpling|baozi|mantou|pancake|unknown",
    "target_food_display": "面条|饺子|包子|馒头|烙饼|未知"
  }},
  "digestive_risk": true或false
}}

用户输入：{text}
"""


def _extract_json_object(raw: str) -> dict[str, Any]:
    cleaned = raw.strip()
    if cleaned.startswith("```"):
        cleaned = re.sub(r"^```(?:json)?", "", cleaned).strip()
        cleaned = re.sub(r"```$", "", cleaned).strip()
    return json.loads(cleaned)


def _payload_to_parse_result(payload: dict[str, Any], profile: UserProfile, source_text: str) -> ParseResult:
    items_payload = payload.get("items")
    if not isinstance(items_payload, list):
        raise ValueError("items must be a list")

    items = [
        parsed_item
        for item in items_payload
        for parsed_item in [_validate_food_item(item)]
        if not _is_target_food_context(source_text, parsed_item.display_name)
    ]
    dough_payload = payload.get("dough_request") or {}
    if not isinstance(dough_payload, dict):
        dough_payload = {}

    target_food = str(dough_payload.get("target_food", "unknown"))
    target_food_display = str(dough_payload.get("target_food_display", "未知"))
    explicit_weight = _detect_dough_weight(source_text)
    if explicit_weight:
        dough_request = DoughRequest(
            dough_total_weight_g=int(explicit_weight),
            water_ratio=DEFAULT_WATER_RATIO,
            target_food=target_food,
            target_food_display=target_food_display,
            weight_source="explicit",
        )
    else:
        dough_request = DoughRequest.estimated_for(target_food, profile)

    return ParseResult(
        items=items,
        dough_request=dough_request,
        needs_confirmation=True,
        digestive_risk=bool(payload.get("digestive_risk", False)) or profile.digestive_sensitivity,
    )


def _validate_food_item(item: dict[str, Any]) -> FoodItem:
    if not isinstance(item, dict):
        raise ValueError("food item must be object")
    category = str(item.get("category", "unknown"))
    if category not in CONTROLLED_CATEGORIES:
        raise ValueError("invalid food category")
    confidence = float(item.get("confidence", 0.0))
    if confidence < 0 or confidence > 1:
        raise ValueError("invalid confidence")
    weight = int(item.get("estimated_weight_g") or 0)
    if weight < 0:
        raise ValueError("invalid weight")
    return FoodItem(
        meal=str(item.get("meal", "unknown")),
        food_name=str(item.get("food_name", item.get("display_name", "unknown"))),
        display_name=str(item.get("display_name", "")),
        amount_text=str(item.get("amount_text", "")),
        estimated_weight_g=weight,
        category=category,
        confidence=confidence,
        source="llm",
        base_food=str(item.get("base_food", "")),
        modifier=str(item.get("modifier", "")),
    )


def _is_target_food_context(text: str, display_name: str) -> bool:
    index = text.find(display_name)
    if index < 0:
        return False
    prefix = text[max(0, index - 10):index]
    return any(word in prefix for word in ("想吃", "想包", "准备", "想做", "做"))
