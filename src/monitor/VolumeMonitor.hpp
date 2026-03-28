#pragma once

struct IMMDeviceEnumerator;
struct IMMDevice;
struct IAudioEndpointVolume;

namespace ssext {

struct VolumeState {
  int percent = 0;
  bool changed = false;
};

class VolumeMonitor {
 public:
  VolumeMonitor();
  ~VolumeMonitor();

  VolumeState Poll();

 private:
  bool EnsureEndpointReady();
  void ReleaseEndpoint();

  int last_percent_ = -1;
  IMMDeviceEnumerator* enumerator_ = nullptr;
  IMMDevice* device_ = nullptr;
  IAudioEndpointVolume* endpoint_volume_ = nullptr;
  bool com_initialized_ = false;
};

}  // namespace ssext
