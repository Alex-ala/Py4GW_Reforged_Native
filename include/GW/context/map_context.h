#pragma once

#include "base/error_handling.h"

#include "GW/common/gw_array.h"
#include "GW/common/gw_list.h"
#include "GW/context/pathing.h"

#include <cstddef>
#include <cstdint>

namespace GW::Context {

    struct PropsContext {
        /* +h0000 */ uint32_t pad1[0x1b];
        /* +h006C */ GW::GWArray<GwList<PropByType>> propsByType;
        /* +h007C */ uint32_t h007C[0xa];
        /* +h00A4 */ GW::GWArray<PropModelInfo> propModels;
        /* +h00B4 */ uint32_t h00B4[0x38];
        /* +h0194 */ GW::GWArray<MapProp*> propArray;
    };
    static_assert(sizeof(PropsContext) == 0x1A4, "PropsContext size mismatch");

    struct MapContext {
        /* +h0000 */ float map_boundaries[5];
        /* +h0014 */ uint32_t h0014[6];
        /* +h002C */ GW::GWArray<void*> spawns1; // Seem to be arena spawns. struct is X,Y,unk 4 byte value,unk 4 byte value.
        /* +h003C */ GW::GWArray<void*> spawns2; // Same as above
        /* +h004C */ GW::GWArray<void*> spawns3; // Same as above
        /* +h005C */ float h005C[6]; // Some trapezoid i think.
        /* +h0074 */ struct sub1 {
            struct sub2 {
                uint32_t pad1[6];
                PathingMapArray pmaps;
            } *sub2;
            /* +h0004 */ GW::GWArray<uint32_t> pathing_map_block;
            /* +h0018 */ uint32_t total_trapezoid_count;
            /* +h0018 */ uint32_t h0014[0x12];
            /* +h0060 */ GW::GWArray<GwList<void*>> something_else_for_props;
            //... Bunch of arrays and shit
        } *sub1;
        /* +h0078 */ uint8_t pad1[4];
        /* +h007C */ PropsContext* props;
        /* +h0080 */ uint32_t h0080;
        /* +h0084 */ void* terrain;
        /* +h0088 */ uint32_t h0088[42];
        /* +h0130 */ void* zones;
        //... Player coords and shit beyond this point if they are desirable :p
    };

static_assert(offsetof(MapContext, sub1) == 0x74, "MapContext::sub1 offset mismatch");
static_assert(offsetof(MapContext, props) == 0x7C, "MapContext::props offset mismatch");
static_assert(offsetof(MapContext, terrain) == 0x84, "MapContext::terrain offset mismatch");
static_assert(offsetof(MapContext, zones) == 0x130, "MapContext::zones offset mismatch");

}  // namespace GW::Context
