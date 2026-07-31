#pragma once

#include "ParameterDefinitions.h"
#include "../common/Math.h"
#include <array>
#include <atomic>
#include <cstdint>

namespace growlforge {

class ParameterStore {
public:
    explicit ParameterStore(const clap_host_t* host);

    void bindHostParams(const clap_host_params_t* hostParams);
    void requestParamFlush() const;
    void queueGuiFlag(clap_id id, uint8_t flag);
    void beginGuiGesture(clap_id id);
    void endGuiGesture(clap_id id);
    void setGuiParameter(clap_id id, double value);

    std::array<std::atomic<double>, kParamCount> values{};
    std::array<std::atomic<double>, kParamCount> guiPendingValue{};
    std::array<std::atomic<uint8_t>, kParamCount> guiPendingFlags{};
    std::atomic<bool> configDirty{false};
    std::atomic<bool> autoGainResetPending{false};
    std::atomic<bool> applyAutoGainPending{false};

private:
    const clap_host_t* host_ = nullptr;
    const clap_host_params_t* hostParams_ = nullptr;
};

} // namespace growlforge
