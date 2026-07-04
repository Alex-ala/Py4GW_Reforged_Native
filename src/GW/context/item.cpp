#include "base/error_handling.h"

#include "GW/context/item.h"

#include "GW/trade/trade.h"

namespace GW::Context {

size_t Bag::find1(uint32_t query_model_id, size_t pos) const {
    for (size_t i = pos; i < items.size(); ++i) {
        Item* item = items[i];
        if (!item && query_model_id == 0) {
            return i;
        }
        if (!item) {
            continue;
        }
        if (item->model_id == query_model_id) {
            return i;
        }
    }
    return npos;
}

size_t Bag::find_dye(uint32_t query_model_id, DyeInfo extra_id, size_t pos) const {
    for (size_t i = pos; i < items.size(); ++i) {
        Item* item = items[i];
        if (!item && query_model_id == 0) {
            return i;
        }
        if (!item) {
            continue;
        }
        if (item->model_id == query_model_id && std::memcmp(&item->dye, &extra_id, sizeof(item->dye)) == 0) {
            return i;
        }
    }
    return npos;
}

size_t Bag::find2(const Item* item, size_t pos) const {
    if (item->model_id == GW::Constants::ItemID::Dye) {
        return find_dye(item->model_id, item->dye, pos);
    }
    return find1(item->model_id, pos);
}

ItemModifier* Item::GetModifier(uint32_t identifier) const {
    for (size_t i = 0; i < mod_struct_size; ++i) {
        ItemModifier* mod = &mod_struct[i];
        if (mod->identifier() == identifier) {
            return mod;
        }
    }
    return nullptr;
}

bool Item::GetIsZcoin() const {
    return model_file_id == 31202U || model_file_id == 31203U || model_file_id == 31204U;
}

bool Item::GetIsMaterial() const {
    return type == GW::Constants::ItemType::Materials_Zcoins && !GetIsZcoin();
}

// ---- Migrated from legacy ItemExtension.cpp (folded into Item) ----

// Retrieve item uses
uint32_t Item::GetUses() const {
    ItemModifier* mod = GetModifier(0x2458);
    return mod ? mod->arg2() : quantity;
}

// Check if item is a Tome
bool Item::IsTome() const {
    const ItemModifier* mod = GetModifier(0x2788);
    const uint32_t use_id = mod ? mod->arg2() : 0;
    return use_id > 15 && use_id < 36;
}

// Check if item is an Identification Kit
bool Item::IsIdentificationKit() const {
    const ItemModifier* mod = GetModifier(0x25E8);
    return mod && mod->arg1() == 1;
}

// Check if item is a Lesser Kit
bool Item::IsLesserKit() const {
    const ItemModifier* mod = GetModifier(0x25E8);
    return mod && mod->arg1() == 3;
}

// Check if item is an Expert Salvage Kit
bool Item::IsExpertSalvageKit() const {
    const ItemModifier* mod = GetModifier(0x25E8);
    return mod && mod->arg1() == 2;
}

// Check if item is a Perfect Salvage Kit
bool Item::IsPerfectSalvageKit() const {
    const ItemModifier* mod = GetModifier(0x25E8);
    return mod && mod->arg1() == 6;
}

// Check if item is a Salvage Kit
bool Item::IsSalvageKit() const {
    return IsLesserKit() || IsExpertSalvageKit() || IsPerfectSalvageKit();
}

// Check if item is a Rare Material
bool Item::IsRareMaterial() const {
    const ItemModifier* mod = GetModifier(0x2508);
    return mod && mod->arg1() > 11;
}

// Get item rarity
GW::Constants::Rarity Item::GetRarity() const {
    if (IsGreen()) return GW::Constants::Rarity::Green;
    if (IsGold()) return GW::Constants::Rarity::Gold;
    if (IsPurple()) return GW::Constants::Rarity::Purple;
    if (IsBlue()) return GW::Constants::Rarity::Blue;
    return GW::Constants::Rarity::White;
}

// Check if item is a Weapon
bool Item::IsWeapon() const {
    switch (type) {
    case GW::Constants::ItemType::Axe:
    case GW::Constants::ItemType::Sword:
    case GW::Constants::ItemType::Shield:
    case GW::Constants::ItemType::Scythe:
    case GW::Constants::ItemType::Bow:
    case GW::Constants::ItemType::Wand:
    case GW::Constants::ItemType::Staff:
    case GW::Constants::ItemType::Offhand:
    case GW::Constants::ItemType::Daggers:
    case GW::Constants::ItemType::Hammer:
    case GW::Constants::ItemType::Spear:
        return true;
    default:
        return false;
    }
}

// Check if item is an Armor
bool Item::IsArmor() const {
    switch (type) {
    case GW::Constants::ItemType::Headpiece:
    case GW::Constants::ItemType::Chestpiece:
    case GW::Constants::ItemType::Leggings:
    case GW::Constants::ItemType::Boots:
    case GW::Constants::ItemType::Gloves:
        return true;
    default:
        return false;
    }
}

// Check if item is Salvageable
bool Item::IsSalvagable() const {
    if (item_formula == 0x5da) return false;
    if (IsUsable() || IsGreen()) return false;
    switch (type) {
    case GW::Constants::ItemType::Trophy:
        return GetRarity() == GW::Constants::Rarity::White && info_string && is_material_salvageable;
    case GW::Constants::ItemType::Salvage:
    case GW::Constants::ItemType::CC_Shards:
        return true;
    case GW::Constants::ItemType::Materials_Zcoins:
        return is_material_salvageable != 0;
    default:
        break;
    }
    if (IsWeapon() || IsArmor()) return true;
    return false;
}

// Check if item is offered in trade
bool Item::IsOfferedInTrade() const {
    return GW::trade::IsItemOffered(item_id) != nullptr;
}

}  // namespace GW::Context
