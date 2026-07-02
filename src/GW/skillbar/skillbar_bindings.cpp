#include <pybind11/embed.h>
#include <pybind11/pybind11.h>

#include "GW/skillbar/skillbar.h"

#include <string>
#include <vector>

namespace py = pybind11;

PYBIND11_EMBEDDED_MODULE(PySkillbar, m) {
    m.doc() = "Py4GW Skillbar bindings";

    m.def("get_skill_slot", [](uint32_t skill_id) -> int {
        return GW::skillbar::GetSkillSlot(static_cast<GW::Constants::SkillID>(skill_id));
    }, py::arg("skill_id"));

    m.def("use_skill", [](uint32_t slot, uint32_t target) -> bool {
        return GW::skillbar::UseSkill(slot, target);
    }, py::arg("slot"), py::arg("target") = 0);

    m.def("point_blank_use_skill", [](uint32_t slot) -> bool {
        return GW::skillbar::PointBlankUseSkill(slot);
    }, py::arg("slot"));

    m.def("use_skill_by_id", [](uint32_t skill_id, uint32_t target) -> bool {
        return GW::skillbar::UseSkillByID(skill_id, target);
    }, py::arg("skill_id"), py::arg("target") = 0);

    m.def("get_is_skill_unlocked", [](uint32_t skill_id) -> bool {
        return GW::skillbar::GetIsSkillUnlocked(static_cast<GW::Constants::SkillID>(skill_id));
    }, py::arg("skill_id"));

    m.def("get_is_skill_learnt", [](uint32_t skill_id) -> bool {
        return GW::skillbar::GetIsSkillLearnt(static_cast<GW::Constants::SkillID>(skill_id));
    }, py::arg("skill_id"));

    m.def("get_skill_profession", [](uint32_t skill_id) -> uint32_t {
        return static_cast<uint32_t>(GW::skillbar::GetSkillProfession(
            static_cast<GW::Constants::SkillID>(skill_id)));
    }, py::arg("skill_id"));

    m.def("get_attribute_profession", [](uint32_t attribute_id) -> uint32_t {
        return static_cast<uint32_t>(GW::skillbar::GetAttributeProfession(
            static_cast<GW::Constants::Attribute>(attribute_id)));
    }, py::arg("attribute_id"));

    m.def("change_second_profession", [](uint32_t profession, uint32_t hero_index) -> bool {
        return GW::skillbar::ChangeSecondProfession(
            static_cast<GW::Constants::Profession>(profession), hero_index);
    }, py::arg("profession"), py::arg("hero_index") = 0);

    m.def("load_skill_template", [](const std::string& template_str, uint32_t hero_index) -> bool {
        return GW::skillbar::LoadSkillTemplate(template_str.c_str(), hero_index);
    }, py::arg("template"), py::arg("hero_index") = 0);

    m.def("load_skillbar", [](const py::list& skill_ids, uint32_t hero_index) -> bool {
        size_t count = static_cast<size_t>(std::min(static_cast<size_t>(skill_ids.size()), size_t(8)));
        GW::Constants::SkillID skills[8] = {};
        for (size_t i = 0; i < count; ++i)
            skills[i] = static_cast<GW::Constants::SkillID>(skill_ids[i].cast<uint32_t>());
        return GW::skillbar::LoadSkillbar(skills, count, hero_index);
    }, py::arg("skill_ids"), py::arg("hero_index") = 0);

    m.def("set_attributes", [](const py::list& attr_ids, const py::list& attr_values, uint32_t hero_index) -> bool {
        size_t count = std::min(attr_ids.size(), attr_values.size());
        std::vector<uint32_t> ids(count), vals(count);
        for (size_t i = 0; i < count; ++i) {
            ids[i] = attr_ids[i].cast<uint32_t>();
            vals[i] = attr_values[i].cast<uint32_t>();
        }
        return GW::skillbar::SetAttributes(static_cast<uint32_t>(count), ids.data(), vals.data(), hero_index);
    }, py::arg("attribute_ids"), py::arg("attribute_values"), py::arg("hero_index") = 0);

    m.def("encode_skill_template", [](uint32_t hero_index) -> std::string {
        auto tpl = GW::skillbar::GetSkillTemplate(hero_index);
        char buffer[256] = {};
        if (GW::skillbar::EncodeSkillTemplate(tpl, buffer, sizeof(buffer)))
            return std::string(buffer);
        return {};
    }, py::arg("hero_index") = 0);

    m.def("decode_skill_template", [](const std::string& template_str) -> py::dict {
        py::dict result;
        GW::Context::SkillTemplate tpl;
        if (GW::skillbar::DecodeSkillTemplate(&tpl, template_str.c_str())) {
            result["profession"] = static_cast<uint32_t>(tpl.primary);
            result["secondary_profession"] = static_cast<uint32_t>(tpl.secondary);
            py::list skills;
            for (uint32_t i = 0; i < 8; ++i)
                skills.append(static_cast<uint32_t>(tpl.skills[i]));
            result["skills"] = skills;
            py::list attrs;
            for (uint32_t i = 0; i < 16; ++i) {
                if (tpl.attributes[i].attribute != GW::Constants::Attribute::None) {
                    py::dict attr;
                    attr["id"] = static_cast<uint32_t>(tpl.attributes[i].attribute);
                    attr["level"] = tpl.attributes[i].points;
                    attrs.append(attr);
                }
            }
            result["attributes"] = attrs;
        }
        return result;
    }, py::arg("template"));
}
