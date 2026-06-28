#pragma once

#include "base/error_handling.h"

#include "GW/common/constants/constants.h"
#include "GW/common/gw_array.h"

#include <cstdint>

namespace GW::Context {

    struct Attribute { // total: 0x14/20
        /* +h0000 */ Constants::Attribute id; // ID of attribute
        /* +h0004 */ uint32_t level_base; // Level of attribute without modifiers (runes,pcons,etc)
        /* +h0008 */ uint32_t level; // Level with modifiers
        /* +h000C */ uint32_t decrement_points; // Points that you will receive back if you decrement level.
        /* +h0010 */ uint32_t increment_points; // Points you will need to increment level.
    };
    static_assert(sizeof(Attribute) == 0x14, "Attribute size mismatch");

    struct AttributeInfo {
        Constants::Profession profession_id;
        Constants::Attribute attribute_id;
        uint32_t name_id;
        uint32_t desc_id;
        uint32_t is_pve;
    };
    static_assert(sizeof(AttributeInfo) == 0x14, "AttributeInfo size mismatch");

    struct PartyAttribute {
        uint32_t agent_id;
        Attribute attribute[54];
    };
    static_assert(sizeof(PartyAttribute) == 0x43C, "PartyAttribute size mismatch");

    typedef GW::GWArray<PartyAttribute> PartyAttributeArray;

}  // namespace GW::Context
