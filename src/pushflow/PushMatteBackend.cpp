// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
// Character Hold's matting network through ONNX Runtime + DirectML on the graph's queue (PushMatteBackend.h).
// How a session is made, held and released is PushFlow's way (src/pushflow/PushFlowBackend.cpp), where the
// reason for each step is written down from field reports; that file is not touched by this one.
#include "veyra/pushflow/PushMatteBackend.h"
#include "veyra/Log.h"

#include <windows.h>
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>
#include <vector>

#ifdef VEYRA_HAVE_ONNXRUNTIME
#include <DirectML.h>
#include <onnxruntime_c_api.h>
#include <dml_provider_factory.h>
#endif

namespace veyra::pushflow {
namespace {
namespace mfs = std::filesystem;
using Microsoft::WRL::ComPtr;

std::filesystem::path matteDirectory(const std::wstring& runtimeDir) { return mfs::path(runtimeDir) / L"pushflow"; }
constexpr const wchar_t* kMatteFiles[] = {L"onnxruntime.dll", L"DirectML.dll", L"models\\pushmatte.onnx", L"models\\pushmatte.onnx.txt"};

// What prepare_matte.py wrote beside the model: the picture's size and the four states' shapes.
struct MatteFacts {
    bool known = false;
    uint32_t width = 0, height = 0;
    int64_t states[4][4]{};
};
bool parseShape(const std::string& text, int64_t out[4]) {
    size_t at = 0;
    for (int i = 0; i < 4; ++i) {
        if (at >= text.size()) return false;
        char* end = nullptr;
        const long long v = std::strtoll(text.c_str() + at, &end, 10);
        if (end == text.c_str() + at || v <= 0 || v > 8192) return false;
        out[i] = v;
        at = size_t(end - text.c_str());
        if (i < 3) { if (at >= text.size() || text[at] != 'x') return false; ++at; }
    }
    return at == text.size();
}
MatteFacts readMatteFacts(const mfs::path& file) {
    MatteFacts f;
    std::ifstream in(file);
    std::string line;
    bool have[4]{};
    while (std::getline(in, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = line.substr(0, eq), value = line.substr(eq + 1);
        if (key == "width") f.width = uint32_t(std::strtoul(value.c_str(), nullptr, 10));
        else if (key == "height") f.height = uint32_t(std::strtoul(value.c_str(), nullptr, 10));
        else if (key.size() == 2 && key[0] == 'r' && key[1] >= '1' && key[1] <= '4') have[key[1] - '1'] = parseShape(value, f.states[key[1] - '1']);
    }
    f.known = f.width >= 64 && f.width <= 4096 && f.height >= 64 && f.height <= 4096 && have[0] && have[1] && have[2] && have[3];
    return f;
}

#ifdef VEYRA_HAVE_ONNXRUNTIME
// A fault inside ONNX Runtime ends Character Hold, not PushFPS (PushFlowBackend.cpp, 2026-11-17). Structured
// exception handling needs functions without C++ objects to unwind, hence these small ones.
bool matteReleaseSession(const OrtApi* api, OrtSession* s) { __try { api->ReleaseSession(s); return true; } __except (EXCEPTION_EXECUTE_HANDLER) { return false; } }
bool matteReleaseBinding(const OrtApi* api, OrtIoBinding* b) { __try { api->ReleaseIoBinding(b); return true; } __except (EXCEPTION_EXECUTE_HANDLER) { return false; } }
bool matteReleaseValue(const OrtApi* api, OrtValue* v) { __try { api->ReleaseValue(v); return true; } __except (EXCEPTION_EXECUTE_HANDLER) { return false; } }
bool matteReleaseMemoryInfo(const OrtApi* api, OrtMemoryInfo* m) { __try { api->ReleaseMemoryInfo(m); return true; } __except (EXCEPTION_EXECUTE_HANDLER) { return false; } }
bool matteReleaseEnv(const OrtApi* api, OrtEnv* e) { __try { api->ReleaseEnv(e); return true; } __except (EXCEPTION_EXECUTE_HANDLER) { return false; } }
bool matteReleaseAllocator(const OrtApi* api, OrtAllocator* a) { __try { api->ReleaseAllocator(a); return true; } __except (EXCEPTION_EXECUTE_HANDLER) { return false; } }
bool matteFreeAllocation(const OrtDmlApi* dml, void* a) { __try { dml->FreeGPUAllocation(a); return true; } __except (EXCEPTION_EXECUTE_HANDLER) { return false; } }
OrtStatus* matteCreateSession(const OrtApi* api, const OrtEnv* env, const wchar_t* model, const OrtSessionOptions* options, OrtSession** out, bool* faulted) {
    __try { return api->CreateSession(env, model, options, out); } __except (EXCEPTION_EXECUTE_HANDLER) { *faulted = true; *out = nullptr; return nullptr; }
}
OrtStatus* matteRun(const OrtApi* api, OrtSession* s, const OrtRunOptions* o, OrtIoBinding* b, bool* faulted) {
    __try { return api->RunWithBinding(s, o, b); } __except (EXCEPTION_EXECUTE_HANDLER) { *faulted = true; return nullptr; }
}
OrtStatus* matteSync(const OrtApi* api, OrtIoBinding* b, bool* faulted) {
    __try { return api->SynchronizeBoundOutputs(b); } __except (EXCEPTION_EXECUTE_HANDLER) { *faulted = true; return nullptr; }
}
OrtStatus* matteCreateAllocator(const OrtApi* api, const OrtSession* s, const OrtMemoryInfo* m, OrtAllocator** out, bool* faulted) {
    __try { return api->CreateAllocator(s, m, out); } __except (EXCEPTION_EXECUTE_HANDLER) { *faulted = true; *out = nullptr; return nullptr; }
}
#endif
}

struct PushMatteBackend::Impl {
    Status status;
    ComPtr<ID3D12Device> device;
    ComPtr<ID3D12CommandQueue> queue;
    MatteFacts facts;
    double runMs = 0.0;
    uint64_t runCount = 0;
    // Tensors: 0 the picture, 1 the matte, 2..5 the states of set 0, 6..9 of set 1, 10..13 states that stay zero.
    static constexpr int kTensors = 14;
    std::array<ComPtr<ID3D12Resource>, kTensors> buffers;
#ifdef VEYRA_HAVE_ONNXRUNTIME
    HMODULE ortModule = nullptr, dmlModule = nullptr;
    const OrtApi* api = nullptr;
    const OrtDmlApi* dml = nullptr;
    OrtEnv* env = nullptr;
    OrtSession* session = nullptr;
    OrtMemoryInfo* memory = nullptr;
    OrtAllocator* pool = nullptr;        // the provider's buffer pool, held until the session is gone
    OrtRunOptions* runOptions = nullptr;
    std::array<void*, kTensors> allocations{};
    std::array<OrtValue*, kTensors> values{};
    // Three bindings: 0 from the zero states into set 0; 1 from set 0 into set 1; 2 from set 1 into set 0.
    std::array<OrtIoBinding*, 3> bindings{};
    int next = 0;                        // the binding the next run takes
    int last = -1;                       // the one the last run took
    ComPtr<IDMLDevice> dmlDevice;
    bool queued = false, faulted = false;
    uint64_t sinceSync = 0;
    static constexpr uint64_t kSyncEvery = 120;   // a full wait every so many runs, as PushFlow

    bool ok(OrtStatus* s, const char* what) {
        if (!s) return true;
        veyra::log::error("character-hold", std::format("{} failed: {}", what, api->GetErrorMessage(s)));
        api->ReleaseStatus(s);
        return false;
    }
    void fault(const char* what) {
        if (!faulted) veyra::log::error("character-hold", std::format("ONNX Runtime faulted in {}; Character Hold is turned off for this session", what));
        faulted = true; status.ready = false; status.state = "failed";
    }
    bool makeBuffer(uint64_t bytes, ComPtr<ID3D12Resource>& out) {
        D3D12_HEAP_PROPERTIES hp{}; hp.Type = D3D12_HEAP_TYPE_DEFAULT;
        D3D12_RESOURCE_DESC bd{}; bd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; bd.Width = (bytes + 255) / 256 * 256; bd.Height = 1;
        bd.DepthOrArraySize = 1; bd.MipLevels = 1; bd.SampleDesc.Count = 1; bd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        bd.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        return SUCCEEDED(device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&out)));
    }
    // Waits until the queue has finished everything submitted so far. Bounded: a removed device completes at once.
    void drainQueue() {
        if (!device || !queue) return;
        ComPtr<ID3D12Fence> fence;
        if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))) || FAILED(queue->Signal(fence.Get(), 1))) return;
        if (fence->GetCompletedValue() >= 1) return;
        HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!event) return;
        if (SUCCEEDED(fence->SetEventOnCompletion(1, event))) WaitForSingleObject(event, 5000);
        CloseHandle(event);
    }
    // The states start as nothing: every state buffer is filled with zeros once (one list, waited for).
    bool clearStates(const uint64_t* bytes) {
        uint64_t largest = 0;
        for (int i = 2; i < kTensors; ++i) largest = std::max(largest, bytes[i]);
        D3D12_HEAP_PROPERTIES hp{}; hp.Type = D3D12_HEAP_TYPE_UPLOAD;
        D3D12_RESOURCE_DESC bd{}; bd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; bd.Width = largest; bd.Height = 1; bd.DepthOrArraySize = 1;
        bd.MipLevels = 1; bd.SampleDesc.Count = 1; bd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        ComPtr<ID3D12Resource> zeros;
        if (FAILED(device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&zeros)))) return false;
        void* mapped = nullptr;
        if (FAILED(zeros->Map(0, nullptr, &mapped)) || !mapped) return false;
        std::memset(mapped, 0, size_t(largest));
        zeros->Unmap(0, nullptr);
        ComPtr<ID3D12CommandAllocator> allocator;
        ComPtr<ID3D12GraphicsCommandList> list;
        ComPtr<ID3D12Fence> fence;
        if (FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator))) ||
            FAILED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr, IID_PPV_ARGS(&list))) ||
            FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)))) return false;
        for (int i = 2; i < kTensors; ++i) {
            D3D12_RESOURCE_BARRIER b{}; b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; b.Transition.pResource = buffers[i].Get();
            b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            b.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS; b.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
            list->ResourceBarrier(1, &b);
            list->CopyBufferRegion(buffers[i].Get(), 0, zeros.Get(), 0, bytes[i]);
            std::swap(b.Transition.StateBefore, b.Transition.StateAfter);
            list->ResourceBarrier(1, &b);
        }
        if (FAILED(list->Close())) return false;
        ID3D12CommandList* lists[] = {list.Get()};
        queue->ExecuteCommandLists(1, lists);
        if (FAILED(queue->Signal(fence.Get(), 1))) return false;
        HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!event) return false;
        fence->SetEventOnCompletion(1, event);
        WaitForSingleObject(event, 30000);
        CloseHandle(event);
        return fence->GetCompletedValue() >= 1;
    }
    bool bind(int index, int from, int into) {
        if (!ok(api->CreateIoBinding(session, &bindings[index]), "binding")) return false;
        OrtIoBinding* b = bindings[index];
        static const char* const ins[4] = {"r1i", "r2i", "r3i", "r4i"};
        static const char* const outs[4] = {"r1o", "r2o", "r3o", "r4o"};
        if (!ok(api->BindInput(b, "src", values[0]), "bind src") || !ok(api->BindOutput(b, "pha", values[1]), "bind pha")) return false;
        for (int k = 0; k < 4; ++k)
            if (!ok(api->BindInput(b, ins[k], values[from + k]), "bind a state in") || !ok(api->BindOutput(b, outs[k], values[into + k]), "bind a state out")) return false;
        return true;
    }
    bool runOnce(bool startOver, bool wait) {
        if (faulted || !session) return false;
        if (startOver) next = 0;
        OrtIoBinding* b = bindings[next];
        if (!b) return false;
        const bool async = queued && !wait && ++sinceSync < kSyncEvery;
        if (!async) sinceSync = 0;
        bool crashed = false;
        const auto t0 = std::chrono::steady_clock::now();
        const bool done = ok(matteRun(api, session, async ? runOptions : nullptr, b, &crashed), "RunWithBinding") && !crashed &&
                          (async || (ok(matteSync(api, b, &crashed), "SynchronizeBoundOutputs") && !crashed));
        runMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        if (crashed) { fault("RunWithBinding"); return false; }
        if (!done) return false;
        last = next;
        next = next == 1 ? 2 : 1;
        ++runCount;
        return true;
    }
    void release() {
        if (queued) drainQueue();
        if (api) {
            // The provider waits for its work and releases what it queued while everything it refers to is there.
            if (session && last >= 0 && bindings[last] && !faulted) {
                bool crashed = false;
                if (OrtStatus* st = matteSync(api, bindings[last], &crashed)) api->ReleaseStatus(st);
                if (crashed) fault("SynchronizeBoundOutputs");
            }
            if (runOptions) { api->ReleaseRunOptions(runOptions); runOptions = nullptr; }
            for (auto*& b : bindings) { if (b && !matteReleaseBinding(api, b)) fault("ReleaseIoBinding"); b = nullptr; }
            for (auto*& v : values) { if (v && !matteReleaseValue(api, v)) fault("ReleaseValue"); v = nullptr; }
            for (auto*& a : allocations) { if (dml && a && !matteFreeAllocation(dml, a)) fault("FreeGPUAllocation"); a = nullptr; }
            if (session && !matteReleaseSession(api, session)) fault("ReleaseSession");
            session = nullptr;
            if (pool && !matteReleaseAllocator(api, pool)) fault("ReleaseAllocator");
            pool = nullptr;
            if (memory && !matteReleaseMemoryInfo(api, memory)) fault("ReleaseMemoryInfo");
            if (env && !matteReleaseEnv(api, env)) fault("ReleaseEnv");
        }
        memory = nullptr; env = nullptr; dml = nullptr; api = nullptr;
        dmlDevice.Reset();
        // The DLLs stay loaded for the process, as for PushFlow.
    }
#endif
    ~Impl() {
#ifdef VEYRA_HAVE_ONNXRUNTIME
        release();
#endif
    }
};

PushMatteBackend::PushMatteBackend() : p_(std::make_unique<Impl>()) {}
PushMatteBackend::~PushMatteBackend() = default;
void PushMatteBackend::shutdown() { p_ = std::make_unique<Impl>(); }
const PushMatteBackend::Status& PushMatteBackend::status() const { return p_->status; }
double PushMatteBackend::lastRunCpuMs() const { return p_->runMs; }
uint64_t PushMatteBackend::runs() const { return p_->runCount; }
ID3D12Resource* PushMatteBackend::pictureBuffer() const { return p_->buffers[0].Get(); }
ID3D12Resource* PushMatteBackend::matteBuffer() const { return p_->buffers[1].Get(); }

bool PushMatteBackend::installed(const std::wstring& runtimeDir, std::wstring* missing) {
    const mfs::path dir = matteDirectory(runtimeDir);
    std::error_code ec;
    for (const wchar_t* f : kMatteFiles)
        if (!mfs::is_regular_file(dir / f, ec)) {
            if (missing) *missing = f;
            return false;
        }
    return true;
}

bool PushMatteBackend::run(bool startOver) {
#ifndef VEYRA_HAVE_ONNXRUNTIME
    (void)startOver;
    return false;
#else
    auto& p = *p_;
    if (!p.status.ready) return false;
    if (p.runOnce(startOver, false)) return true;
    if (!p.faulted) {
        veyra::log::error("character-hold", "the matting network did not run; Character Hold is turned off for this session");
        p.status.ready = false; p.status.state = "failed";
    }
    return false;
#endif
}

bool PushMatteBackend::initialize(ID3D12Device* device, ID3D12CommandQueue* queue, const std::wstring& runtimeDir) {
    p_ = std::make_unique<Impl>();
    auto& p = *p_;
#ifndef VEYRA_HAVE_ONNXRUNTIME
    (void)device; (void)queue; (void)runtimeDir;
    p.status.state = "not-built";
    return false;
#else
    std::wstring missing;
    if (!device || !queue) { p.status.state = "runtime"; return false; }
    if (!installed(runtimeDir, &missing)) {
        std::string name;
        for (wchar_t c : missing) name.push_back(c < 128 ? char(c) : '?');
        veyra::log::warn("character-hold", "not installed: runtime\\pushflow\\" + name);
        p.status.state = "no-files";
        return false;
    }
    const auto loadStart = std::chrono::steady_clock::now();
    p.device = device; p.queue = queue;
    const mfs::path dir = matteDirectory(runtimeDir);
    mfs::path model = dir / L"models" / L"pushmatte.onnx";
    {
        // For developers: another file of the same model in the models folder (VEYRA_PUSHMATTE_MODEL=<name>.onnx).
        wchar_t other[64]{};
        const DWORD n = GetEnvironmentVariableW(L"VEYRA_PUSHMATTE_MODEL", other, 64);
        if (n > 0 && n < 64 && !wcschr(other, L'\\') && !wcschr(other, L'/') && !wcsstr(other, L"..")) model = dir / L"models" / other;
    }
    p.facts = readMatteFacts(dir / L"models" / L"pushmatte.onnx.txt");
    if (!p.facts.known) { veyra::log::error("character-hold", "pushmatte.onnx.txt does not say what the model takes"); p.status.state = "model"; return false; }
    p.status.width = p.facts.width; p.status.height = p.facts.height;
    // DirectML first, from our folder, so ONNX Runtime and we use the same (redistributed) copy.
    p.dmlModule = LoadLibraryExW((dir / L"DirectML.dll").c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    p.ortModule = LoadLibraryExW((dir / L"onnxruntime.dll").c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if (!p.dmlModule || !p.ortModule) { veyra::log::error("character-hold", std::format("LoadLibrary failed error={}", GetLastError())); p.status.state = "runtime"; return false; }
    using CreateDml = HRESULT(WINAPI*)(ID3D12Device*, DML_CREATE_DEVICE_FLAGS, REFIID, void**);
    using GetBase = const OrtApiBase*(ORT_API_CALL*)();
    auto createDml = reinterpret_cast<CreateDml>(reinterpret_cast<void*>(GetProcAddress(p.dmlModule, "DMLCreateDevice")));
    auto getBase = reinterpret_cast<GetBase>(reinterpret_cast<void*>(GetProcAddress(p.ortModule, "OrtGetApiBase")));
    p.status.state = "runtime";
    if (!createDml || !getBase) { veyra::log::error("character-hold", "runtime entry points missing"); return false; }
    const OrtApiBase* base = getBase();
    p.api = base ? base->GetApi(ORT_API_VERSION) : nullptr;
    if (!p.api) { veyra::log::error("character-hold", std::format("ONNX Runtime {} does not offer API {}", base ? base->GetVersionString() : "?", ORT_API_VERSION)); return false; }
    if (FAILED(createDml(device, DML_CREATE_DEVICE_FLAG_NONE, IID_PPV_ARGS(&p.dmlDevice)))) { veyra::log::error("character-hold", "DMLCreateDevice failed"); return false; }
    auto* api = p.api;
    if (!p.ok(api->GetExecutionProviderApi("DML", ORT_API_VERSION, reinterpret_cast<const void**>(&p.dml)), "GetExecutionProviderApi") || !p.dml) return false;
    // Every operator gets its own DirectML kernel ("ep.dml.disable_graph_fusion"). With the provider's default,
    // one compiled graph for the whole model, DirectML refuses this model when the session is made
    // (DmlGraphFusionHelper.cpp(576), "The parameter is incorrect": ONNX Runtime 1.20.1 on the software renderer,
    // 23 November 2026; the same with the model optimised beforehand, with what the dropped output needed
    // removed, without the states' Expand nodes, and without the Split one part of which nobody reads: the
    // "matte" job of the pushflow-models workflow tries those again on every run). Operator by operator the
    // network runs and gives what the processor gives. What the whole-graph form would save on a graphics card,
    // and whether a graphics card's own driver takes it, is not known.
    // For developers (tools/character_probe): VEYRA_PUSHMATTE_FUSION=1 asks for the whole-graph form,
    // VEYRA_PUSHMATTE_VERBOSE=1 makes ONNX Runtime say what it does, VEYRA_PUSHMATTE_OPTIMIZE=0|1|2|99 sets the
    // graph optimisation level. Never set in the product.
    const auto setting = [](const wchar_t* name, int fallback) {
        wchar_t text[16]{};
        const DWORD n = GetEnvironmentVariableW(name, text, 16);
        return n > 0 && n < 16 ? _wtoi(text) : fallback;
    };
    const bool verbose = setting(L"VEYRA_PUSHMATTE_VERBOSE", 0) != 0, fusion = setting(L"VEYRA_PUSHMATTE_FUSION", 0) != 0;
    const int optimize = setting(L"VEYRA_PUSHMATTE_OPTIMIZE", 99);
    if (!p.ok(api->CreateEnv(verbose ? ORT_LOGGING_LEVEL_VERBOSE : ORT_LOGGING_LEVEL_WARNING, "pushmatte", &p.env), "CreateEnv")) return false;
    if (!p.ok(api->CreateMemoryInfo("DML", OrtDeviceAllocator, 0, OrtMemTypeDefault, &p.memory), "memory info")) return false;

    // The session: the DirectML provider on the graph's queue, set up as PushFlow's.
    p.status.state = "session";
    {
        OrtSessionOptions* options = nullptr;
        if (!p.ok(api->CreateSessionOptions(&options), "CreateSessionOptions")) return false;
        const GraphOptimizationLevel level = optimize == 0 ? ORT_DISABLE_ALL : optimize == 1 ? ORT_ENABLE_BASIC : optimize == 2 ? ORT_ENABLE_EXTENDED : ORT_ENABLE_ALL;
        bool good = p.ok(api->SetSessionGraphOptimizationLevel(options, level), "optimization level") &&
                    (fusion || p.ok(api->AddSessionConfigEntry(options, "ep.dml.disable_graph_fusion", "1"), "no graph fusion")) &&
                    p.ok(api->DisableMemPattern(options), "DisableMemPattern") &&
                    p.ok(api->SetSessionExecutionMode(options, ORT_SEQUENTIAL), "execution mode") &&
                    p.ok(api->SetIntraOpNumThreads(options, 1), "intra-op threads") &&
                    p.ok(api->AddSessionConfigEntry(options, "session.intra_op.allow_spinning", "0"), "no spinning");
        if (good) good = p.ok(p.dml->SessionOptionsAppendExecutionProvider_DML1(options, p.dmlDevice.Get(), queue), "DirectML provider");
        if (good) {
            bool crashed = false;
            good = p.ok(matteCreateSession(api, p.env, model.c_str(), options, &p.session, &crashed), "CreateSession") && !crashed;
            if (crashed) p.fault("CreateSession");
        }
        api->ReleaseSessionOptions(options);
        if (!good || !p.session) { p.status.state = p.faulted ? "failed" : "session"; return false; }
        bool crashed = false;
        if (OrtStatus* st = matteCreateAllocator(api, p.session, p.memory, &p.pool, &crashed)) { api->ReleaseStatus(st); p.pool = nullptr; }
        if (crashed) { p.fault("CreateAllocator"); return false; }
        p.status.poolHeld = p.pool != nullptr;
    }
    // Is it the model prepare_matte.py makes? Five inputs, five outputs, the picture's size as the facts say.
    {
        size_t inputs = 0, outputs = 0;
        api->SessionGetInputCount(p.session, &inputs);
        api->SessionGetOutputCount(p.session, &outputs);
        OrtTypeInfo* info = nullptr;
        bool sized = false;
        if (inputs == 5 && outputs == 5 && p.ok(api->SessionGetInputTypeInfo(p.session, 0, &info), "input type") && info) {
            const OrtTensorTypeAndShapeInfo* tensor = nullptr;
            if (p.ok(api->CastTypeInfoToTensorInfo(info, &tensor), "tensor info") && tensor) {
                size_t dims = 0;
                api->GetDimensionsCount(tensor, &dims);
                int64_t shape[4]{};
                if (dims == 4 && p.ok(api->GetDimensions(tensor, shape, 4), "dimensions"))
                    sized = shape[0] == 1 && shape[1] == 3 && shape[2] == int64_t(p.facts.height) && shape[3] == int64_t(p.facts.width);
            }
            api->ReleaseTypeInfo(info);
        }
        if (!sized) { veyra::log::error("character-hold", std::format("pushmatte.onnx is not the prepared model (inputs={} outputs={})", inputs, outputs)); p.status.state = "model"; return false; }
    }

    // The tensors' buffers, always in UNORDERED_ACCESS, each wrapped for ONNX Runtime.
    uint64_t bytes[Impl::kTensors]{};
    int64_t shapes[Impl::kTensors][4]{};
    const auto shape = [&](int i, int64_t a, int64_t b, int64_t c, int64_t d) { shapes[i][0] = a; shapes[i][1] = b; shapes[i][2] = c; shapes[i][3] = d; bytes[i] = uint64_t(a * b * c * d) * 4u; };
    shape(0, 1, 3, p.facts.height, p.facts.width);
    shape(1, 1, 1, p.facts.height, p.facts.width);
    for (int set = 0; set < 3; ++set)
        for (int k = 0; k < 4; ++k) shape(2 + 4 * set + k, p.facts.states[k][0], p.facts.states[k][1], p.facts.states[k][2], p.facts.states[k][3]);
    for (int i = 0; i < Impl::kTensors; ++i) {
        if (!p.makeBuffer(bytes[i], p.buffers[i])) { veyra::log::error("character-hold", "tensor buffers"); return false; }
        if (!p.ok(p.dml->CreateGPUAllocationFromD3DResource(p.buffers[i].Get(), &p.allocations[i]), "allocation")) return false;
        if (!p.ok(api->CreateTensorWithDataAsOrtValue(p.memory, p.allocations[i], size_t(bytes[i]), shapes[i], 4, ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT, &p.values[i]), "tensor")) return false;
    }
    if (!p.clearStates(bytes)) { veyra::log::error("character-hold", "the states could not be cleared"); return false; }
    if (!p.bind(0, 10, 2) || !p.bind(1, 2, 6) || !p.bind(2, 6, 2)) { p.status.state = "model"; return false; }

    // One run that is waited for: the provider's first run does its own setting up, and an error shows here.
    if (!p.runOnce(true, true)) { if (!p.faulted) p.status.state = "failed"; veyra::log::error("character-hold", "the matting network's first run failed"); return false; }
    p.next = 0; p.last = 0; p.runCount = 0;
    if (GetEnvironmentVariableW(L"VEYRA_PUSHFLOW_SYNC", nullptr, 0) == 0) {
        if (p.ok(api->CreateRunOptions(&p.runOptions), "CreateRunOptions") &&
            p.ok(api->AddRunConfigEntry(p.runOptions, "disable_synchronize_execution_providers", "1"), "run option")) p.queued = true;
    }
    p.status.queued = p.queued;
    p.status.loadMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - loadStart).count();
    p.status.ready = true; p.status.state = "on";
    veyra::log::info("character-hold", std::format("matting network ready runtime={} picture={}x{} queued={} poolHeld={} fusion={} optimize={} firstRunMs={:.0f} loadMs={:.0f}",
                                                    base->GetVersionString(), p.facts.width, p.facts.height, p.queued ? 1 : 0, p.status.poolHeld ? 1 : 0, fusion ? 1 : 0, optimize,
                                                    p.runMs, p.status.loadMs));
    return true;
#endif
}

}
