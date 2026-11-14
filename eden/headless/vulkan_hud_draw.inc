        // The overlay is laid out for a picture 1080 rows high and scaled to this one.
        const auto hud_units = [rows = render_area.height](uint32_t value) { return value * rows / 1080u; };
        // The chip's scale, from the size field in `loading`: 2, 3 or 4 for 1x, 1.5x or 2x
        // (hud.h, HudSizeField). Every one of the box's numbers is taken that many times over, so
        // the box's height is kHudBoxUnits times that field, half again for the middle size.
        const uint32_t hud_size_field = (hud.loading & Eden::kHudSizeMask) >> Eden::kHudSizeShift;
        const uint32_t hud_box = static_cast<uint32_t>(Eden::kHudBoxUnits) * hud_size_field / 2u;
        const bool loading = Eden::HudLoading(hud);
        if (hud.width && (loading || (render_area.width >= hud_units(hud.x + hud.width) &&
                                      render_area.height >= hud_units(hud.y + hud_box)))) {
            const VkRect2D scissor = loading ? VkRect2D{{0, 0}, render_area} :
                VkRect2D{{static_cast<int32_t>(hud_units(hud.x)), static_cast<int32_t>(hud_units(hud.y))},
                         {hud_units(hud.width), hud_units(hud_box)}};
            const VkViewport viewport{static_cast<float>(scissor.offset.x), static_cast<float>(scissor.offset.y),
                static_cast<float>(scissor.extent.width), static_cast<float>(scissor.extent.height), 0, 1};
            cmdbuf.SetViewport(0, viewport);
            cmdbuf.SetScissor(0, scissor);
            cmdbuf.BindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, hud_graphics_pipeline);
            cmdbuf.BindDescriptorSets(VK_PIPELINE_BIND_POINT_GRAPHICS, hud_pipeline_layout, 0,
                                      hud_descriptor_set, {});
            auto constants = hud;
            if (loading) {
                // The loading scene fills the picture: tell it how large that is.
                constants.x = render_area.width;
                constants.y = render_area.height;
            } else {
                // Where the text starts in this picture, and the picture's height for its size.
                constants.x = hud_units(hud.x);
                constants.y = hud_units(hud.y);
                constants.width = render_area.height;
            }
            cmdbuf.PushConstants(hud_pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT, constants);
            cmdbuf.Draw(4, 1, 0, 0);
        }
        // DirkuLator 0.5.20: the graphs, each in its own box with the same pipeline (hud.h,
        // MakeHudGraphs; vulkan_hud.frag draws them). A graph that does not fit is left out.
        for (const Eden::HudSnapshot& graph : hud_graphs) {
            const uint32_t graph_h = static_cast<uint32_t>(
                std::lround(Eden::kHudGraphHeight * Eden::HudSizeScale(graph.loading & Eden::kHudSizeMask)));
            if (!hud_graphics_pipeline || graph.width == 0 ||
                render_area.width < hud_units(graph.x + graph.width) ||
                render_area.height < hud_units(graph.y + graph_h))
                continue;
            const VkRect2D graph_scissor{
                {static_cast<int32_t>(hud_units(graph.x)), static_cast<int32_t>(hud_units(graph.y))},
                {hud_units(graph.width), hud_units(graph_h)}};
            const VkViewport graph_viewport{static_cast<float>(graph_scissor.offset.x),
                static_cast<float>(graph_scissor.offset.y), static_cast<float>(graph_scissor.extent.width),
                static_cast<float>(graph_scissor.extent.height), 0, 1};
            cmdbuf.SetViewport(0, graph_viewport);
            cmdbuf.SetScissor(0, graph_scissor);
            cmdbuf.BindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, hud_graphics_pipeline);
            cmdbuf.BindDescriptorSets(VK_PIPELINE_BIND_POINT_GRAPHICS, hud_pipeline_layout, 0,
                                      hud_descriptor_set, {});
            auto graph_constants = graph;
            graph_constants.x = hud_units(graph.x);
            graph_constants.y = hud_units(graph.y);
            graph_constants.width = render_area.height;
            cmdbuf.PushConstants(hud_pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT, graph_constants);
            cmdbuf.Draw(4, 1, 0, 0);
        }
