#include "monitor/VolumeMonitor.hpp"

#include <Windows.h>
#include <endpointvolume.h>
#include <mmdeviceapi.h>
#include <mmsystem.h>

#include <algorithm>
#include <cmath>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "winmm.lib")

namespace ssext {

VolumeMonitor::VolumeMonitor() = default;

VolumeMonitor::~VolumeMonitor() {
  ReleaseEndpoint();
}

void VolumeMonitor::ReleaseEndpoint() {
  if (endpoint_volume_ != nullptr) {
    endpoint_volume_->Release();
    endpoint_volume_ = nullptr;
  }
  if (device_ != nullptr) {
    device_->Release();
    device_ = nullptr;
  }
  if (enumerator_ != nullptr) {
    enumerator_->Release();
    enumerator_ = nullptr;
  }
  if (com_initialized_) {
    CoUninitialize();
    com_initialized_ = false;
  }
}

bool VolumeMonitor::EnsureEndpointReady() {
  if (endpoint_volume_ != nullptr) {
    return true;
  }

  if (!com_initialized_) {
    const HRESULT init_hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(init_hr) && init_hr != RPC_E_CHANGED_MODE) {
      return false;
    }
    if (SUCCEEDED(init_hr)) {
      com_initialized_ = true;
    }
  }

  if (endpoint_volume_ != nullptr) {
    endpoint_volume_->Release();
    endpoint_volume_ = nullptr;
  }
  if (device_ != nullptr) {
    device_->Release();
    device_ = nullptr;
  }
  if (enumerator_ != nullptr) {
    enumerator_->Release();
    enumerator_ = nullptr;
  }

  HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                __uuidof(IMMDeviceEnumerator),
                                reinterpret_cast<void**>(&enumerator_));
  if (FAILED(hr) || enumerator_ == nullptr) {
    return false;
  }

  hr = enumerator_->GetDefaultAudioEndpoint(eRender, eConsole, &device_);
  if (FAILED(hr) || device_ == nullptr) {
    return false;
  }

  hr = device_->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr,
                         reinterpret_cast<void**>(&endpoint_volume_));
  if (FAILED(hr) || endpoint_volume_ == nullptr) {
    return false;
  }

  return true;
}

VolumeState VolumeMonitor::Poll() {
  int percent = 0;

  if (EnsureEndpointReady()) {
    float level = 0.0f;
    const HRESULT hr = endpoint_volume_->GetMasterVolumeLevelScalar(&level);
    if (SUCCEEDED(hr)) {
      level = std::clamp(level, 0.0f, 1.0f);
      percent = static_cast<int>(std::round(level * 100.0f));
    } else {
      endpoint_volume_->Release();
      endpoint_volume_ = nullptr;
    }
  }

  if (endpoint_volume_ == nullptr) {
    DWORD volume = 0;
    if (waveOutGetVolume(nullptr, &volume) != MMSYSERR_NOERROR) {
      return {};
    }

    const int left = static_cast<int>(volume & 0xFFFF);
    const int right = static_cast<int>((volume >> 16) & 0xFFFF);
    const double avg =
        (static_cast<double>(left) + static_cast<double>(right)) / (2.0 * 65535.0);
    percent = static_cast<int>(std::round(avg * 100.0));
  }

  VolumeState state;
  state.percent = percent;

  if (last_percent_ < 0) {
    last_percent_ = percent;
    state.changed = true;
    return state;
  }

  state.changed = std::abs(percent - last_percent_) >= 1;
  if (state.changed) {
    last_percent_ = percent;
  }

  return state;
}

}  // namespace ssext
