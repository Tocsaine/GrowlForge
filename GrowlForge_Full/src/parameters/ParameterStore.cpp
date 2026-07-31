#include "ParameterStore.h"

namespace growlforge {

ParameterStore::ParameterStore(const clap_host_t* host) : host_(host) {
    for (size_t i = 0; i < values.size(); ++i) {
        values[i] = defs[i].def;
        guiPendingValue[i] = defs[i].def;
        guiPendingFlags[i] = 0;
    }
}

void ParameterStore::bindHostParams(const clap_host_params_t* hostParams) {
    hostParams_ = hostParams;
}

void ParameterStore::requestParamFlush() const {
    if (hostParams_ && hostParams_->request_flush) hostParams_->request_flush(host_);
    else if (host_ && host_->request_process) host_->request_process(host_);
}

void ParameterStore::queueGuiFlag(clap_id id, uint8_t flag) {
    if (id >= kParamCount) return;
    guiPendingFlags[id].fetch_or(flag, std::memory_order_release);
    requestParamFlush();
}

void ParameterStore::beginGuiGesture(clap_id id) { queueGuiFlag(id, 1u); }
void ParameterStore::endGuiGesture(clap_id id) { queueGuiFlag(id, 4u); }

void ParameterStore::setGuiParameter(clap_id id, double value) {
    if (id >= kParamCount || id == AutoGainCorrection || id >= MeterSaturation) return;
    if (id == ApplyAutoGain) {
        applyAutoGainPending = true;
        requestParamFlush();
        return;
    }
    value = clamp(value, defs[id].min, defs[id].max);
    if (id == AutoGain || id == X2) value = value >= 0.5 ? 1.0 : 0.0;
    else value = quantize01(value);
    const double previous = values[id].exchange(value);
    if (id == AutoGain && previous != value) {
        if (value >= 0.5) {
            values[Output] = 0.0;
            guiPendingValue[Output] = 0.0;
            guiPendingFlags[Output].fetch_or(2u, std::memory_order_release);
        }
        autoGainResetPending = true;
    }
    guiPendingValue[id] = value;
    guiPendingFlags[id].fetch_or(2u, std::memory_order_release);
    configDirty = true;
    requestParamFlush();
}

} // namespace growlforge
