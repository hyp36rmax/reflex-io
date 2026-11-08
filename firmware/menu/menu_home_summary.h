#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

// Pure text policy shared by Classic2USB Home and the native OLED exporter.
namespace home_summary {
struct Controllers { uint8_t count = 0; const char* singleName = nullptr; };
template<class FrameAt> Controllers connected(uint8_t slots, FrameAt frameAt) {
  Controllers result;
  for (uint8_t p = 0; p < slots; ++p) {
    const auto& frame = frameAt(p);
    if (!frame.connected) continue;
    ++result.count;
    result.singleName = frame.controller_type_name;
  }
  if (result.count != 1) result.singleName = nullptr;
  return result;
}
inline const char* family(const char* mode, const char* compact) {
  if (!std::strcmp(mode, "Dreamcast")) return "DC";
  if (!std::strcmp(mode, "Saturn")) return "Sat";
  if (!std::strcmp(mode, "PCE/TG-16")) return "PCE";
  return compact;
}
inline bool needsFamily(const char* name) {
  const char* generic[] = {"Pad", "Wheel", "Mission", "Mission6", "Digital", "3D Pad",
                          "Classic", "ClassicPro", "Guitar", "Drums", "2-Button", "6-Button"};
  for (const auto* item : generic) if (!std::strcmp(name, item)) return true;
  return !std::strncmp(name, "Pad +", 5) || !std::strncmp(name, "Whl+", 4) ||
         !std::strncmp(name, "Mission+", 8);
}
inline void format(char* out, size_t size, Controllers controllers, bool scanning, bool mixed,
                   const char* mode, const char* compactMode,
                   const char* output, const char* compactOutput) {
  if (!out || !size) return;
  const size_t budget = size - 1 < 21 ? size - 1 : 21;
  char qualified[48];
  const char* input = scanning ? "Auto" : mixed ? "Mixed" : mode;
  const char* compactInput = scanning ? "Auto" : mixed ? "Mixed" : compactMode;
  const char* name = controllers.singleName;
  if (!scanning && controllers.count == 1 && name && *name && std::strcmp(name,"Connected")) {
    if (needsFamily(name)) {
      std::snprintf(qualified, sizeof(qualified), "%s %s", family(mode, compactMode), name);
      input = compactInput = qualified;
    } else input = compactInput = name;
  }
  const auto attempt = [&](const char* left, const char* right, bool spaced) {
    if (std::strlen(left) + std::strlen(right) + (spaced ? 3u : 1u) > budget) return false;
    std::snprintf(out, size, spaced ? "%s > %s" : "%s>%s", left, right);
    return true;
  };
  if (attempt(input,output,true) || attempt(input,compactOutput,true) ||
      attempt(compactInput,compactOutput,true) || attempt(compactInput,compactOutput,false)) return;
  // Keep the output mode visible; mark an unusually long controller name as shortened.
  const size_t right = std::strlen(compactOutput);
  if (budget > right + 2) {
    std::snprintf(out,size,"%.*s~>%s",int(budget-right-2),compactInput,compactOutput);
  } else std::snprintf(out,size,"%.*s",int(budget),compactOutput);
}
} // namespace home_summary
