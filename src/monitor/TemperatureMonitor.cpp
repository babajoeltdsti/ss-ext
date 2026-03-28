#include "monitor/TemperatureMonitor.hpp"

#include <Windows.h>
#include <Wbemidl.h>
#include <comdef.h>

#pragma comment(lib, "wbemuuid.lib")

namespace ssext {

TemperatureState TemperatureMonitor::Poll() {
  TemperatureState state;

  HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  bool uninit = SUCCEEDED(hr);

  hr = CoInitializeSecurity(nullptr, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_DEFAULT,
                            RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE, nullptr);
  if (FAILED(hr) && hr != RPC_E_TOO_LATE) {
    if (uninit) {
      CoUninitialize();
    }
    return state;
  }

  IWbemLocator* locator = nullptr;
  hr = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_IWbemLocator,
                        reinterpret_cast<LPVOID*>(&locator));
  if (FAILED(hr) || locator == nullptr) {
    if (uninit) {
      CoUninitialize();
    }
    return state;
  }

  IWbemServices* services = nullptr;
  hr = locator->ConnectServer(_bstr_t(L"ROOT\\WMI"), nullptr, nullptr, nullptr, 0, nullptr,
                              nullptr, &services);
  if (FAILED(hr) || services == nullptr) {
    locator->Release();
    if (uninit) {
      CoUninitialize();
    }
    return state;
  }

  hr = CoSetProxyBlanket(services, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
                         RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr,
                         EOAC_NONE);
  if (FAILED(hr)) {
    services->Release();
    locator->Release();
    if (uninit) {
      CoUninitialize();
    }
    return state;
  }

  IEnumWbemClassObject* enumerator = nullptr;
  hr = services->ExecQuery(_bstr_t("WQL"),
                           _bstr_t("SELECT CurrentTemperature FROM MSAcpi_ThermalZoneTemperature"),
                           WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr,
                           &enumerator);
  if (SUCCEEDED(hr) && enumerator != nullptr) {
    IWbemClassObject* obj = nullptr;
    ULONG returned = 0;
    hr = enumerator->Next(WBEM_INFINITE, 1, &obj, &returned);
    if (SUCCEEDED(hr) && returned > 0 && obj != nullptr) {
      VARIANT vt_prop;
      VariantInit(&vt_prop);
      if (SUCCEEDED(obj->Get(L"CurrentTemperature", 0, &vt_prop, nullptr, nullptr))) {
        if (vt_prop.vt == VT_I4 || vt_prop.vt == VT_UI4) {
          const long kelvin_tenths = (vt_prop.vt == VT_I4) ? vt_prop.lVal : static_cast<long>(vt_prop.ulVal);
          const int celsius = static_cast<int>((kelvin_tenths / 10) - 273.15);
          state.available = true;
          state.celsius = celsius;
        }
      }
      VariantClear(&vt_prop);
      obj->Release();
    }
    enumerator->Release();
  }

  services->Release();
  locator->Release();

  if (uninit) {
    CoUninitialize();
  }

  return state;
}

}  // namespace ssext
