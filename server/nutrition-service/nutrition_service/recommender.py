from __future__ import annotations

from .models import DoughRequest, FoodItem, Recommendation


def recommend_coarse_grain(
    items: list[FoodItem],
    dough_request: DoughRequest,
    digestive_risk: bool = False,
) -> Recommendation:
    refined_weight = sum(
        item.estimated_weight_g for item in items if item.category == "refined_staple"
    )
    coarse_weight = sum(
        item.estimated_weight_g for item in items if item.category == "coarse_grain"
    )

    ratio = 0.20
    if refined_weight >= 300:
        ratio += 0.15
    elif refined_weight >= 150:
        ratio += 0.10

    if coarse_weight >= 100:
        ratio -= 0.05

    if digestive_risk:
        ratio = min(ratio, 0.20)

    ratio = min(max(ratio, 0.10), 0.40)
    ratio = round(ratio / 0.05) * 0.05
    ratio = round(ratio, 2)

    total = dough_request.dough_total_weight_g
    water_weight = round(total * dough_request.water_ratio)
    flour_weight = total - water_weight
    coarse_grain_weight = round(flour_weight * ratio)

    reason = _build_reason(refined_weight, digestive_risk)
    return Recommendation(
        dough_total_weight_g=total,
        flour_weight_g=flour_weight,
        water_weight_g=water_weight,
        coarse_grain_ratio=ratio,
        coarse_grain_weight_g=coarse_grain_weight,
        reason=reason,
    )


def _build_reason(refined_weight: int, digestive_risk: bool) -> str:
    if digestive_risk:
        return "检测到消化风险描述，建议控制本次面团中的杂粮比例。"
    if refined_weight >= 300:
        return "今日精制主食摄入偏多，建议提高本次面团中的杂粮比例。"
    if refined_weight >= 150:
        return "今日已有一定精制主食摄入，建议适度提高本次面团中的杂粮比例。"
    return "今日精制主食摄入不高，建议维持基础杂粮比例。"
