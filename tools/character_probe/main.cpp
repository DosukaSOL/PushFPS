// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
//
// Character Hold alone, on this machine's graphics card or the software renderer: the pass, its seven shaders
// and the matting network through DirectML, against pipeline/PushCharacter.h (the test itself is
// tests/integration/PushCharacterGpu.h, which the Windows build runs too). Built with the compiler alone from
// this file, src/pipeline/PushCharacterPass.cpp and src/pushflow/PushMatteBackend.cpp (the "matte" job of the
// pushflow-models workflow), so a change to Character Hold is compiled and run in minutes.
//   character_probe <folder that holds pushflow\>     (shaders: the folder in VEYRA_PROBE_SHADERS)
#include "../../tests/integration/PushCharacterGpu.h"
#include <d3d12sdklayers.h>
#include <dxgi1_6.h>

namespace {
int failures = 0;
struct Gpu {
    Microsoft::WRL::ComPtr<ID3D12Device> device;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> list;
    Microsoft::WRL::ComPtr<ID3D12Fence> fence;
    HANDLE event = nullptr;
    uint64_t value = 0;
    bool debug = false;
    std::string adapter;
    bool create() {
        Microsoft::WRL::ComPtr<ID3D12Debug> d;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&d)))) { d->EnableDebugLayer(); debug = true; }
        Microsoft::WRL::ComPtr<IDXGIFactory6> factory;
        if (FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)))) return false;
        for (UINT i = 0; !device; ++i) {
            Microsoft::WRL::ComPtr<IDXGIAdapter1> a;
            if (factory->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&a)) == DXGI_ERROR_NOT_FOUND) break;
            DXGI_ADAPTER_DESC1 desc{}; a->GetDesc1(&desc);
            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;
            if (SUCCEEDED(D3D12CreateDevice(a.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device)))) { for (const wchar_t* w = desc.Description; *w; ++w) adapter.push_back(*w < 128 ? char(*w) : '?'); }
        }
        if (!device) {
            Microsoft::WRL::ComPtr<IDXGIAdapter> warp;
            if (FAILED(factory->EnumWarpAdapter(IID_PPV_ARGS(&warp))) || FAILED(D3D12CreateDevice(warp.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)))) return false;
            adapter = "WARP";
        }
        D3D12_COMMAND_QUEUE_DESC qd{}; qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        return SUCCEEDED(device->CreateCommandQueue(&qd, IID_PPV_ARGS(&queue))) &&
               SUCCEEDED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator))) &&
               SUCCEEDED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr, IID_PPV_ARGS(&list))) &&
               SUCCEEDED(list->Close()) && SUCCEEDED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))) &&
               (event = CreateEventW(nullptr, FALSE, FALSE, nullptr)) != nullptr;
    }
    bool begin() { return SUCCEEDED(allocator->Reset()) && SUCCEEDED(list->Reset(allocator.Get(), nullptr)); }
    bool submitAndWait() {
        if (FAILED(list->Close())) return false;
        ID3D12CommandList* lists[] = {list.Get()};
        queue->ExecuteCommandLists(1, lists);
        if (FAILED(queue->Signal(fence.Get(), ++value))) return false;
        if (fence->GetCompletedValue() < value) { fence->SetEventOnCompletion(value, event); WaitForSingleObject(event, 600000); }
        return fence->GetCompletedValue() >= value && SUCCEEDED(device->GetDeviceRemovedReason());
    }
    bool debugErrors() {
        if (!debug) return false;
        Microsoft::WRL::ComPtr<ID3D12InfoQueue> info;
        if (FAILED(device.As(&info))) return false;
        bool errors = false;
        for (UINT64 i = 0, n = info->GetNumStoredMessages(); i < n; ++i) {
            SIZE_T size = 0; info->GetMessage(i, nullptr, &size);
            std::vector<char> buffer(size); auto* m = reinterpret_cast<D3D12_MESSAGE*>(buffer.data());
            if (SUCCEEDED(info->GetMessage(i, m, &size)) && m->Severity <= D3D12_MESSAGE_SEVERITY_ERROR) { std::printf("D3D12 %s\n", m->pDescription); errors = true; }
        }
        info->ClearStoredMessages();
        return errors;
    }
};
}

int wmain(int argc, wchar_t** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc < 2) { std::printf("usage: character_probe <folder that holds pushflow\\>\n"); return 2; }
    Gpu gpu;
    if (!gpu.create()) { std::printf("FAIL no D3D12 device\n"); return 1; }
    std::printf("device: %s; debug layer %s\n", gpu.adapter.c_str(), gpu.debug ? "on" : "not installed");
    const bool started = charactergpu::run(gpu, argv[1], [](bool ok, const std::string& label) { std::printf("%s %s\n", ok ? "PASS" : "FAIL", label.c_str()); if (!ok) ++failures; });
    std::printf("%s (%d failed)\n", failures || !started ? "FAILED" : "ALL PASS", failures);
    return failures || !started ? 1 : 0;
}
