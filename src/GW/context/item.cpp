#include "base/error_handling.h"

#include "GW/context/item.h"

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

}  // namespace GW::Context
