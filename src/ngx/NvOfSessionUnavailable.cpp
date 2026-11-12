// PushFPS: NvOfSession for builds made without the NVIDIA Optical Flow SDK headers.
//
// The SDK's D3D12 interface headers are only distributed behind an NVIDIA developer login, so the
// automated release build cannot fetch them. This file keeps the class and its contract and simply
// reports "not available", exactly like a GPU without NVOF hardware: initialize() fails before
// touching any resource, nothing is registered, and teardown is a no-op. builtWithSdk() returns
// false, so the capability table marks NVIDIA optical flow (flow0) unavailable and the settings
// sanitizer moves every chain to AMD FidelityFX optical flow, which runs on any DirectX 12 GPU.
// A build that finds the SDK compiles NvOfSession.cpp instead (see CMakeLists.txt).

#include "veyra/ngx/NvOfSession.h"

#include "veyra/Log.h"

namespace veyra::ngx {

bool NvOfSession::builtWithSdk() { return false; }

NvOfSession::~NvOfSession() = default;

bool NvOfSession::initialize(ID3D12Device*, ID3D12Resource*, ID3D12Resource*, ID3D12Resource*, ID3D12Resource*,
                             const Desc&, Status& status)
{
    veyra::log::warn("nvof", "this build has no NVIDIA Optical Flow SDK; NVOF is unavailable");
    initialized_ = false;
    status = Status::LoadLibraryFailure;
    return false;
}

bool NvOfSession::execute(uint64_t, Status& status)
{
    status = Status::DeviceFailure;
    return false;
}

bool NvOfSession::unregisterAll(Status& status)
{
    status = Status::Ok;
    return true;
}

void NvOfSession::shutdown() { initialized_ = false; }

} // namespace veyra::ngx
