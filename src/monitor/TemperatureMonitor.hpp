#pragma once

namespace ssext {

struct TemperatureState {
  bool available = false;
  int celsius = 0;
};

class TemperatureMonitor {
 public:
  TemperatureState Poll();
};

}  // namespace ssext
