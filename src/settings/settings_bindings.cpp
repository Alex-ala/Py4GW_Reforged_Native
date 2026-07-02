#include <pybind11/embed.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "settings/settings.h"
#include "system/system.h"

#include <string>
#include <utility>

namespace py = pybind11;

namespace {

constexpr const char* kDefaultSection = "settings";

PY4GW::SettingsScope ParseScope(const std::string& scope) {
    if (scope == "global") {
        return PY4GW::SettingsScope::Global;
    }
    if (scope == "account") {
        return PY4GW::SettingsScope::Account;
    }
    throw py::value_error("scope must be \"account\" or \"global\"");
}

// Flat keys land in the default section; "section/key" addresses one explicitly.
std::pair<std::string, std::string> SplitKey(const std::string& key) {
    const auto separator = key.find('/');
    if (separator == std::string::npos) {
        return {kDefaultSection, key};
    }
    return {key.substr(0, separator), key.substr(separator + 1)};
}

}  // namespace

PYBIND11_EMBEDDED_MODULE(PySettings, m) {
    m.doc() = "Py4GW per-account INI settings (see docs/settings-ini-design.md)";

    py::class_<PY4GW::IniFile, std::unique_ptr<PY4GW::IniFile, py::nodelete>> cls(m, "settings");
    cls.def(py::init([](const std::string& name, const std::string& scope) {
        return &PY4GW::SettingsManager::Instance().Open(name, ParseScope(scope));
    }), py::arg("name"), py::arg("scope") = "account",
        "Bind to a named settings document; no open/close/save needed");

    // write: value type selected by overload. bool is registered before int
    // because Python bool is a subclass of int.
    cls.def("write", [](PY4GW::IniFile& self, const std::string& key, bool value) {
        const auto [section, k] = SplitKey(key);
        self.SetBool(section, k, value);
    }, py::arg("key"), py::arg("value"));
    cls.def("write", [](PY4GW::IniFile& self, const std::string& key, long long value) {
        const auto [section, k] = SplitKey(key);
        self.SetInt(section, k, value);
    }, py::arg("key"), py::arg("value"));
    cls.def("write", [](PY4GW::IniFile& self, const std::string& key, double value) {
        const auto [section, k] = SplitKey(key);
        self.SetFloat(section, k, value);
    }, py::arg("key"), py::arg("value"));
    cls.def("write", [](PY4GW::IniFile& self, const std::string& key, const std::string& value) {
        const auto [section, k] = SplitKey(key);
        self.SetString(section, k, value);
    }, py::arg("key"), py::arg("value"));

    // read: the second argument is either a default value (fallback + type)
    // or a Python type token (bool/int/float/str) whose zero value is the
    // fallback.
    cls.def("read", [](PY4GW::IniFile& self, const std::string& key, py::object default_or_type) -> py::object {
        const auto [section, k] = SplitKey(key);
        PyObject* ptr = default_or_type.ptr();

        if (PyType_Check(ptr)) {
            if (ptr == reinterpret_cast<PyObject*>(&PyBool_Type)) {
                return py::bool_(self.GetBool(section, k, false));
            }
            if (ptr == reinterpret_cast<PyObject*>(&PyLong_Type)) {
                return py::int_(self.GetInt(section, k, 0));
            }
            if (ptr == reinterpret_cast<PyObject*>(&PyFloat_Type)) {
                return py::float_(self.GetFloat(section, k, 0.0));
            }
            if (ptr == reinterpret_cast<PyObject*>(&PyUnicode_Type)) {
                return py::str(self.GetString(section, k, ""));
            }
            throw py::type_error("read(): unsupported type token; use bool, int, float, or str");
        }

        if (py::isinstance<py::bool_>(default_or_type)) {
            return py::bool_(self.GetBool(section, k, default_or_type.cast<bool>()));
        }
        if (py::isinstance<py::int_>(default_or_type)) {
            return py::int_(self.GetInt(section, k, default_or_type.cast<long long>()));
        }
        if (py::isinstance<py::float_>(default_or_type)) {
            return py::float_(self.GetFloat(section, k, default_or_type.cast<double>()));
        }
        if (py::isinstance<py::str>(default_or_type)) {
            return py::str(self.GetString(section, k, default_or_type.cast<std::string>()));
        }
        throw py::type_error("read(): default must be bool, int, float, or str, or one of those types");
    }, py::arg("key"), py::arg("default") = py::str(""));

    // Escape hatches; never required in normal flow.
    cls.def("save", &PY4GW::IniFile::Save, "Force an immediate save");
    cls.def("reload", &PY4GW::IniFile::Reload, "Re-read from disk, discarding unsaved changes");
    cls.def("is_dirty", &PY4GW::IniFile::IsDirty);
    cls.def("is_bound", &PY4GW::IniFile::IsBound, "Whether the document is attached to disk yet");
    cls.def("has_key", [](const PY4GW::IniFile& self, const std::string& key) {
        const auto [section, k] = SplitKey(key);
        return self.HasKey(section, k);
    }, py::arg("key"));
    cls.def("keys", [](const PY4GW::IniFile& self, const std::string& section) {
        return self.GetKeys(section);
    }, py::arg("section") = kDefaultSection);
    cls.def("sections", &PY4GW::IniFile::GetSections);
    cls.def("delete", [](PY4GW::IniFile& self, const std::string& key) {
        const auto [section, k] = SplitKey(key);
        return self.DeleteKey(section, k);
    }, py::arg("key"));

    m.def("is_anchored", []() {
        return PY4GW::System::Instance().HasAccountEmail();
    }, "Whether account-scoped documents are bound to disk yet");
    m.def("get_settings_directory", []() -> std::string {
        return PY4GW::System::Instance().GetSettingsDirectory().string();
    }, "Per-account settings directory (empty until the anchor resolves)");
}
