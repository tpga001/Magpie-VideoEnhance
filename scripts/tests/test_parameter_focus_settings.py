"""Generate a focused JSON round-trip test from the production setting paths."""
from pathlib import Path
import re
import sys
import subprocess

root = Path(__file__).resolve().parents[2]
settings = (root / 'src/Magpie/AppSettings.cpp').read_text(encoding='utf-8-sig')
header = (root / 'src/Magpie/AppSettings.h').read_text(encoding='utf-8-sig')
helper = (root / 'src/Magpie/JsonHelper.cpp').read_text(encoding='utf-8-sig')
reader = helper[helper.index('bool JsonHelper::ReadBool('):helper.index('bool JsonHelper::ReadBoolFlag(')]

harness = r'''
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <cassert>
#include <iostream>
#include <string>
struct JsonHelper {
    static bool ReadBool(const rapidjson::GenericObject<true, rapidjson::Value>&,
        const char*, bool&, bool required=false) noexcept;
};
READER
struct Settings {
    FIELD
    void Load(const char* text) {
        const rapidjson::Document document = [] (const char* text) {
            rapidjson::Document result; result.Parse(text); return result;
        }(text);
        assert(!document.HasParseError()); const auto root=document.GetObject();
        LOAD
    }
    std::string Save() const {
        const auto& data=*this;
        rapidjson::StringBuffer buffer; rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        writer.StartObject(); SAVE writer.EndObject();
        return buffer.GetString();
    }
};
int main() {
    Settings data; assert(!data._isParameterFocusSwitchingEnabled);
    for (const char* input : {"{}", R"({"frontEdgeSync":true})", R"({"parameterFocusSwitching":false})",
            R"({"parameterFocusSwitching":null})", R"({"parameterFocusSwitching":1})",
            R"({"parameterFocusSwitching":"true"})"}) {
        data._isParameterFocusSwitchingEnabled=true; data.Load(input);
        assert(!data._isParameterFocusSwitchingEnabled);
    }
    for (bool enabled : {true,false}) {
        data.Load(enabled ? R"({"parameterFocusSwitching":true})" : R"({"parameterFocusSwitching":false})");
        assert(data._isParameterFocusSwitchingEnabled==enabled);
        Settings reloaded; reloaded.Load(data.Save().c_str());
        assert(reloaded._isParameterFocusSwitchingEnabled==enabled);
    }
    std::cout << "PASS parameter focus setting: default/old/missing/invalid disabled, explicit opt-in and true/false JSON round-trip\n";
}
'''
output = Path(sys.argv[1]) / 'parameter_focus_settings.cpp'
field2 = re.search(r'bool _isStopEffectsOnTaskSwitchEnabled = [^;]+;', header).group()
load2 = re.search(r'_isStopEffectsOnTaskSwitchEnabled = false;\s+JsonHelper::ReadBool\(root, "stopEffectsOnTaskSwitch", _isStopEffectsOnTaskSwitchEnabled\);', settings).group()
save2 = re.search(r'writer.Key\("stopEffectsOnTaskSwitch"\);\s+writer.Bool\(data._isStopEffectsOnTaskSwitchEnabled\);', settings).group()
second = harness.replace('READER', reader).replace('FIELD', field2).replace('LOAD', load2).replace('SAVE', save2)
second = second.replace('_isParameterFocusSwitchingEnabled', '_isStopEffectsOnTaskSwitchEnabled').replace('parameterFocusSwitching', 'stopEffectsOnTaskSwitch').replace('parameter focus setting', 'task-switch setting')
other = Path(sys.argv[1]) / 'task_switch_settings.cpp'
other.write_text(second, encoding='utf-8')
subprocess.run([sys.executable, str(root/'scripts/tests/test_profile_parameter_focus.py'), sys.argv[1]], check=True)
print(other)
