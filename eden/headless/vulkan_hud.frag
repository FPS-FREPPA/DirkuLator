// SPDX-License-Identifier: GPL-3.0-or-later
#version 450
#extension GL_GOOGLE_include_directive : enable
layout(push_constant) uniform Text { uint glyphs[28]; uint width; uint x; uint y; uint loading; } text;
// The loading screen's picture (loading_art.h); the overlay does not read it.
layout(set = 0, binding = 0) uniform sampler2D loading_art;
layout(location = 0) out vec4 color;
#include "loading_text.glsl"
#include "loading_scene.glsl"
void main() {
    // Vulkan push-constant arrays require dynamically uniform indices. Load
    // each word at a constant index before the per-pixel local-array lookup.
    uint glyphs[28] = uint[28](
        text.glyphs[0], text.glyphs[1], text.glyphs[2], text.glyphs[3],
        text.glyphs[4], text.glyphs[5], text.glyphs[6], text.glyphs[7],
        text.glyphs[8], text.glyphs[9], text.glyphs[10], text.glyphs[11],
        text.glyphs[12], text.glyphs[13], text.glyphs[14], text.glyphs[15],
        text.glyphs[16], text.glyphs[17], text.glyphs[18], text.glyphs[19],
        text.glyphs[20], text.glyphs[21], text.glyphs[22], text.glyphs[23],
        text.glyphs[24], text.glyphs[25], text.glyphs[26], text.glyphs[27]);
    // The loading screen owns the small numbers; the overlay marks its snapshot with a bit the
    // loading path never sets, so the two are told apart here (hud.h, kHudOverlayFlag).
    if (text.loading != 0u && (text.loading & 0x80000000u) == 0u) {
        // The loading screen: x and y carry the picture's size, loading the milliseconds since
        // it began (plus one), the glyphs' words what the start is doing (hud.h). Vulkan counts
        // rows from the top.
        vec2 size = vec2(float(text.x), float(text.y));
        uint state[24];
        for (int i = 0; i < 24; ++i) state[i] = glyphs[i];
        color = vec4(loading_scene(vec2(gl_FragCoord.x, size.y - gl_FragCoord.y), size,
                                   float(text.loading - 1u) * 0.001, state), 1.0);
        return;
    }
    // The overlay: x and y are its box's top left corner in this picture, width the picture's
    // height (vulkan_hud_draw.inc sets all three). Everything is laid out for a picture 1080
    // rows high; unit is how far one of those rows reaches here. The chip's scale comes from the
    // size field in `loading` (2, 3 or 4 for 1x, 1.5x or 2x; hud.h, HudSizeField), so every size
    // below is taken that many times over. The colour bit says the line is drawn in its inks.
    const float unit = float(text.width) / 1080.0;
    const float scale = float((text.loading >> 28) & 7u) * 0.5;
    const bool colors = (text.loading & 0x08000000u) != 0u;
    // Vulkan counts rows from the top, hud_pick (like loading_scene) from the bottom left.
    const vec2 pixel = vec2(gl_FragCoord.x, float(text.width) - gl_FragCoord.y);
    if ((text.loading & 0x04000000u) != 0u) {
        // DirkuLator 0.5.20: a graph (hud.h, MakeHudGraphs, kHudGraphFlag): its label in the
        // first two words, 104 one-byte samples in the rest, oldest first. Its box is 220 x 60
        // units (kHudGraphWidth, kHudGraphHeight), the bars fill it under the label.
        const vec2 box = vec2(220.0, 60.0) * unit * scale;
        const vec2 local = gl_FragCoord.xy - vec2(float(text.x), float(text.y));
        const vec4 panel = hud_panel(local, vec2(box.x, kHudBoxUnits * unit * scale));
        const float pad = kHudPadUnits * unit * scale;
        const float cap = kHudCapUnits * 0.8 * unit * scale;
        const float top = pad + cap * 1.35;  // the plot starts under the label
        const vec2 plot = vec2(box.x - 2.0 * pad, box.y - top - pad);
        // The panel's corners for the whole box: hud_panel is sized by a chip's height, so take
        // its rounding from a chip and its body from this box.
        const vec2 q = abs(local - 0.5 * box) - 0.5 * box + kHudRadiusUnits * unit * scale;
        const float edge = min(max(q.x, q.y), 0.0) + length(max(q, vec2(0.0))) - kHudRadiusUnits * unit * scale;
        vec4 result = vec4(panel.rgb, 0.78 * (1.0 - smoothstep(-0.75, 0.75, edge)));
        const float px = local.x - pad;
        const float up = (box.y - pad) - local.y;  // height above the plot's floor
        if (px >= 0.0 && px < plot.x && up >= 0.0 && up <= plot.y) {
            const int i = clamp(int(px / plot.x * 104.0), 0, 103);
            const float sample_value = float((glyphs[2 + i / 4] >> uint(8 * (i % 4))) & 255u);
            const float bad = float((text.loading >> 16) & 255u);
            const float warn = float((text.loading >> 8) & 255u);
            const float guide = float(text.loading & 255u);
            const bool higher_better = (text.loading & 0x01000000u) != 0u;
            int ink = kHudInkGood;
            if (higher_better ? sample_value < warn : sample_value > warn) ink = kHudInkWarn;
            if (higher_better ? sample_value < bad : sample_value > bad) ink = kHudInkBad;
            // A faint floor for the plot, the bar, and the guide line over both.
            result.rgb = mix(result.rgb, vec3(0.16, 0.18, 0.24), 0.6);
            if (up <= sample_value / 255.0 * plot.y) result = vec4(hud_ink(ink), 0.92);
            if (guide > 0.0 && abs(up - guide / 255.0 * plot.y) < 0.75 * unit * scale)
                result = vec4(mix(result.rgb, kHudInk, 0.55), max(result.a, 0.85));
        }
        // The label: the first two words, the rest of the line empty.
        uint label[28];
        for (int i = 0; i < 28; ++i) label[i] = 0u;
        label[0] = glyphs[0];
        label[1] = glyphs[1];
        Pick label_pick = Pick(-1, 0, vec2(0.0), 1.0);
        hud_pick(label_pick, label, pixel,
                 vec2(float(text.x) + pad, float(text.width) - (float(text.y) + pad + cap)), cap);
        const float label_cover = pick_cover(label_pick);
        color = vec4(mix(result.rgb, hud_ink(label_pick.ink), label_cover), mix(result.a, 1.0, label_cover));
        return;
    }
    const vec2 at = vec2(float(text.x) + kHudPadUnits * unit * scale,
                         float(text.width) - (float(text.y) + kHudBaselineUnits * unit * scale));
    Pick pick = Pick(-1, 0, vec2(0.0), 1.0);
    hud_pick(pick, glyphs, pixel, at, kHudCapUnits * unit * scale);
    const float cover = pick_cover(pick);
    // The chip the line stands in, walked out here because Vulkan's push constants have no room
    // left to carry it: this pixel's place in the box, and the box's own size in this picture.
    const vec2 box = vec2(hud_line_width(glyphs, kHudCapUnits * unit * scale) +
                              2.0 * kHudPadUnits * unit * scale,
                          kHudBoxUnits * unit * scale);
    const vec4 panel = hud_panel(gl_FragCoord.xy - vec2(float(text.x), float(text.y)), box);
    // The line in its inks when colours are on, in the one ink when they are off.
    const vec3 ink = colors ? hud_ink(pick.ink) : kHudInk;
    color = vec4(mix(panel.rgb, ink, cover), mix(panel.a, 1.0, cover));
}
