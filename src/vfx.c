#include "vfx.h"

void spawn_impact_effect(ImpactEffectList *list, CombatShotResult result, Color color) {
    if (result.outcome == SHOT_OUTCOME_INVALID) return;
    int idx = list->count % MAX_IMPACT_EFFECTS;
    ImpactEffect *e = &list->items[idx];
    e->position = (Vector3){ (float)result.impact_point.x, (float)result.impact_point.z, (float)result.impact_point.y };
    e->max_life_s = 0.45f;
    e->life_s = e->max_life_s;
    e->color = color;
    list->count++;
}

void update_impact_effects(ImpactEffectList *list, float dt) {
    for (int i = 0; i < MAX_IMPACT_EFFECTS; i++) {
        if (list->items[i].life_s > 0.0f) list->items[i].life_s -= dt;
    }
}

void draw_impact_effects(const ImpactEffectList *list) {
    for (int i = 0; i < MAX_IMPACT_EFFECTS; i++) {
        const ImpactEffect *e = &list->items[i];
        if (e->life_s <= 0.0f) continue;
        float t = e->life_s / e->max_life_s;
        float radius = 1.6f * (1.0f - t) + 0.3f;
        DrawSphere(e->position, radius, Fade(e->color, t));
    }
}