"""Food lexicon for the rule-based MVP parser.

Weights are rough single-serving estimates for local testing, not medical data.
"""

FOOD_DICTIONARY = {
    # Refined staples
    "包子": ("baozi", "refined_staple", 80, "个"),
    "馒头": ("mantou", "refined_staple", 100, "个"),
    "面包": ("bread", "refined_staple", 80, "片"),
    "米饭": ("rice", "refined_staple", 150, "碗"),
    "白米饭": ("rice", "refined_staple", 150, "碗"),
    "面条": ("noodles", "refined_staple", 200, "碗"),
    "米线": ("rice_noodles", "refined_staple", 220, "碗"),
    "河粉": ("rice_noodles", "refined_staple", 220, "碗"),
    "饺子": ("dumpling", "refined_staple", 20, "个"),
    "馄饨": ("wonton", "refined_staple", 18, "个"),
    "年糕": ("rice_cake", "refined_staple", 150, "份"),
    "蛋糕": ("cake", "fat_sugar", 100, "块"),
    "饼干": ("biscuit", "fat_sugar", 30, "块"),
    # Coarse grains, tubers, whole grains
    "燕麦": ("oats", "coarse_grain", 50, "份"),
    "燕麦粥": ("oatmeal", "coarse_grain", 250, "碗"),
    "小米粥": ("millet_porridge", "coarse_grain", 250, "碗"),
    "玉米": ("corn", "coarse_grain", 180, "根"),
    "红薯": ("sweet_potato", "coarse_grain", 200, "个"),
    "紫薯": ("purple_potato", "coarse_grain", 180, "个"),
    "土豆": ("potato", "coarse_grain", 180, "个"),
    "山药": ("yam", "coarse_grain", 150, "段"),
    "杂豆": ("mixed_beans", "coarse_grain", 80, "份"),
    "绿豆": ("mung_beans", "coarse_grain", 80, "份"),
    "红豆": ("red_beans", "coarse_grain", 80, "份"),
    # Protein
    "鸡蛋": ("egg", "protein", 50, "个"),
    "牛奶": ("milk", "protein", 250, "杯"),
    "酸奶": ("yogurt", "protein", 200, "杯"),
    "豆浆": ("soy_milk", "protein", 250, "杯"),
    "豆腐": ("tofu", "protein", 150, "份"),
    "鸡胸肉": ("chicken_breast", "protein", 120, "份"),
    "鸡肉": ("chicken", "protein", 120, "份"),
    "牛肉": ("beef", "protein", 100, "份"),
    "猪肉": ("pork", "protein", 100, "份"),
    "鱼": ("fish", "protein", 120, "份"),
    "虾": ("shrimp", "protein", 100, "份"),
    # Vegetables
    "青菜": ("greens", "vegetable", 150, "份"),
    "白菜": ("cabbage", "vegetable", 150, "份"),
    "菠菜": ("spinach", "vegetable", 150, "份"),
    "生菜": ("lettuce", "vegetable", 120, "份"),
    "西兰花": ("broccoli", "vegetable", 150, "份"),
    "胡萝卜": ("carrot", "vegetable", 100, "根"),
    "番茄": ("tomato", "vegetable", 150, "个"),
    "黄瓜": ("cucumber", "vegetable", 150, "根"),
    # Fruits
    "苹果": ("apple", "fruit", 200, "个"),
    "香蕉": ("banana", "fruit", 120, "根"),
    "橙子": ("orange", "fruit", 180, "个"),
    "梨": ("pear", "fruit", 200, "个"),
    "葡萄": ("grapes", "fruit", 150, "份"),
    "火龙果": ("dragon_fruit", "fruit", 250, "个"),
    # Fat and sugar
    "油条": ("youtiao", "fat_sugar", 70, "根"),
    "炸鸡": ("fried_chicken", "fat_sugar", 150, "份"),
    "薯条": ("fries", "fat_sugar", 120, "份"),
    "奶茶": ("milk_tea", "fat_sugar", 500, "杯"),
    "可乐": ("cola", "fat_sugar", 330, "瓶"),
}

COMPOSITE_FOOD_DICTIONARY = {
    "荞麦面条": ("buckwheat_noodles", "coarse_grain", 200, "碗", "面条", "荞麦"),
    "荞麦面": ("buckwheat_noodles", "coarse_grain", 200, "碗", "面条", "荞麦"),
    "全麦馒头": ("whole_wheat_mantou", "coarse_grain", 100, "个", "馒头", "全麦"),
    "全麦面包": ("whole_wheat_bread", "coarse_grain", 80, "片", "面包", "全麦"),
    "全麦吐司": ("whole_wheat_toast", "coarse_grain", 40, "片", "吐司", "全麦"),
    "杂粮包子": ("mixed_grain_baozi", "coarse_grain", 80, "个", "包子", "杂粮"),
    "玉米面条": ("corn_noodles", "coarse_grain", 200, "碗", "面条", "玉米"),
    "玉米馒头": ("corn_mantou", "coarse_grain", 100, "个", "馒头", "玉米"),
    "黑米饭": ("black_rice", "coarse_grain", 150, "碗", "米饭", "黑米"),
    "糙米饭": ("brown_rice", "coarse_grain", 150, "碗", "米饭", "糙米"),
    "杂粮饭": ("mixed_grain_rice", "coarse_grain", 150, "碗", "米饭", "杂粮"),
    "牛肉面": ("beef_noodles", "refined_staple", 250, "碗", "面条", "牛肉"),
    "鸡蛋面": ("egg_noodles", "refined_staple", 230, "碗", "面条", "鸡蛋"),
}

TARGET_FOODS = {
    "饺子": ("dumpling", "饺子"),
    "面条": ("noodles", "面条"),
    "面": ("noodles", "面条"),
    "包子": ("baozi", "包子"),
    "馒头": ("mantou", "馒头"),
    "烙饼": ("pancake", "烙饼"),
    "饼": ("pancake", "烙饼"),
}
