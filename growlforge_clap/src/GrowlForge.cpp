#include <clap/clap.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

namespace {

constexpr const char *kPluginId = "com.tocsaine.growlforge";
constexpr double kPi = 3.14159265358979323846;

enum ParamId : clap_id {
  kInput = 0,
  kDrive,
  kFuzz,
  kGrowl,
  kTight,
  kPresence,
  kCab,
  kGate,
  kMix,
  kOutput,
  kParamCount
};

struct ParamDef {
  clap_id id;
  const char *name;
  double min;
  double max;
  double def;
  const char *unit;
};

constexpr std::array<ParamDef, kParamCount> kParams{{
    {kInput, "Input", -18.0, 18.0, 0.0, "dB"},
    {kDrive, "Drive", 0.0, 10.0, 4.8, ""},
    {kFuzz, "Fuzz", 0.0, 10.0, 3.2, ""},
    {kGrowl, "Growl", 0.0, 10.0, 6.3, ""},
    {kTight, "Tight", 0.0, 10.0, 6.8, ""},
    {kPresence, "Presence", 0.0, 10.0, 5.4, ""},
    {kCab, "Cab Filter", 0.0, 10.0, 7.0, ""},
    {kGate, "Gate", -80.0, -30.0, -58.0, "dB"},
    {kMix, "Mix", 0.0, 100.0, 100.0, "%"},
    {kOutput, "Output", -24.0, 12.0, -5.0, "dB"},
}};

inline double dbToGain(double db) { return std::pow(10.0, db / 20.0); }
inline double clamp1(double x) { return std::clamp(x, -1.0, 1.0); }

class OnePole {
public:
  void setLowpass(double hz, double sr) {
    const double x = std::exp(-2.0 * kPi * std::clamp(hz, 10.0, sr * 0.45) / sr);
    a_ = x;
  }
  double lowpass(double x) {
    z_ = (1.0 - a_) * x + a_ * z_;
    return z_;
  }
  double highpass(double x) { return x - lowpass(x); }
  void reset() { z_ = 0.0; }
private:
  double a_ = 0.0;
  double z_ = 0.0;
};

class Biquad {
public:
  void setPeaking(double sr, double hz, double q, double gainDb) {
    const double A = std::pow(10.0, gainDb / 40.0);
    const double w0 = 2.0 * kPi * hz / sr;
    const double alpha = std::sin(w0) / (2.0 * q);
    const double c = std::cos(w0);
    const double b0 = 1.0 + alpha * A;
    const double b1 = -2.0 * c;
    const double b2 = 1.0 - alpha * A;
    const double a0 = 1.0 + alpha / A;
    const double a1 = -2.0 * c;
    const double a2 = 1.0 - alpha / A;
    set(b0, b1, b2, a0, a1, a2);
  }
  void setLowpass(double sr, double hz, double q = 0.70710678) {
    const double w0 = 2.0 * kPi * hz / sr;
    const double alpha = std::sin(w0) / (2.0 * q);
    const double c = std::cos(w0);
    set((1.0-c)*0.5, 1.0-c, (1.0-c)*0.5, 1.0+alpha, -2.0*c, 1.0-alpha);
  }
  void setHighpass(double sr, double hz, double q = 0.70710678) {
    const double w0 = 2.0 * kPi * hz / sr;
    const double alpha = std::sin(w0) / (2.0 * q);
    const double c = std::cos(w0);
    set((1.0+c)*0.5, -(1.0+c), (1.0+c)*0.5, 1.0+alpha, -2.0*c, 1.0-alpha);
  }
  double process(double x) {
    const double y = b0_ * x + z1_;
    z1_ = b1_ * x - a1_ * y + z2_;
    z2_ = b2_ * x - a2_ * y;
    return y;
  }
  void reset() { z1_ = z2_ = 0.0; }
private:
  void set(double b0, double b1, double b2, double a0, double a1, double a2) {
    b0_ = b0/a0; b1_ = b1/a0; b2_ = b2/a0; a1_ = a1/a0; a2_ = a2/a0;
  }
  double b0_ = 1.0, b1_ = 0.0, b2_ = 0.0, a1_ = 0.0, a2_ = 0.0;
  double z1_ = 0.0, z2_ = 0.0;
};

struct Channel {
  Biquad tightHp;
  Biquad growlPeak;
  Biquad presencePeak;
  Biquad cabLp;
  OnePole envelope;
  double gateGain = 1.0;
  double dc = 0.0;

  void reset() {
    tightHp.reset(); growlPeak.reset(); presencePeak.reset(); cabLp.reset();
    envelope.reset(); gateGain = 1.0; dc = 0.0;
  }
};

class GrowlForge {
public:
  explicit GrowlForge(const clap_host *host) : host_(host) {
    for (size_t i = 0; i < kParamCount; ++i) values_[i].store(kParams[i].def);

    plugin_.desc = &descriptor_;
    plugin_.plugin_data = this;
    plugin_.init = [](const clap_plugin *p) { return self(p)->init(); };
    plugin_.destroy = [](const clap_plugin *p) { delete self(p); };
    plugin_.activate = [](const clap_plugin *p, double sr, uint32_t, uint32_t) { return self(p)->activate(sr); };
    plugin_.deactivate = [](const clap_plugin *p) { self(p)->deactivate(); };
    plugin_.start_processing = [](const clap_plugin *p) { self(p)->processing_ = true; return true; };
    plugin_.stop_processing = [](const clap_plugin *p) { self(p)->processing_ = false; };
    plugin_.reset = [](const clap_plugin *p) { self(p)->reset(); };
    plugin_.process = [](const clap_plugin *p, const clap_process *pr) { return self(p)->process(pr); };
    plugin_.get_extension = [](const clap_plugin *p, const char *id) { return self(p)->getExtension(id); };
    plugin_.on_main_thread = [](const clap_plugin *) {};
  }

  const clap_plugin *plugin() const { return &plugin_; }

private:
  static GrowlForge *self(const clap_plugin *p) { return static_cast<GrowlForge *>(p->plugin_data); }
  bool init() { return true; }
  bool activate(double sr) { sampleRate_ = sr; updateFilters(); reset(); return true; }
  void deactivate() { processing_ = false; }
  void reset() { for (auto &c : ch_) c.reset(); }

  void updateFilters() {
    const double tight = values_[kTight].load();
    const double growl = values_[kGrowl].load();
    const double presence = values_[kPresence].load();
    const double cab = values_[kCab].load();
    const double hpHz = 55.0 + tight * 13.0;
    const double growlHz = 420.0 + growl * 42.0;
    const double growlDb = 1.5 + growl * 0.75;
    const double presenceDb = -1.5 + presence * 0.7;
    const double lpHz = 11000.0 - cab * 650.0;
    for (auto &c : ch_) {
      c.tightHp.setHighpass(sampleRate_, hpHz, 0.72);
      c.growlPeak.setPeaking(sampleRate_, growlHz, 0.85, growlDb);
      c.presencePeak.setPeaking(sampleRate_, 2900.0, 0.75, presenceDb);
      c.cabLp.setLowpass(sampleRate_, std::clamp(lpHz, 3800.0, 11000.0), 0.72);
      c.envelope.setLowpass(35.0, sampleRate_);
    }
  }

  void handleEvents(const clap_input_events *events) {
    if (!events) return;
    const uint32_t n = events->size(events);
    bool filterDirty = false;
    for (uint32_t i = 0; i < n; ++i) {
      const clap_event_header *h = events->get(events, i);
      if (!h || h->space_id != CLAP_CORE_EVENT_SPACE_ID) continue;
      if (h->type == CLAP_EVENT_PARAM_VALUE) {
        const auto *ev = reinterpret_cast<const clap_event_param_value *>(h);
        if (ev->param_id < kParamCount) {
          values_[ev->param_id].store(std::clamp(ev->value, kParams[ev->param_id].min, kParams[ev->param_id].max));
          if (ev->param_id == kTight || ev->param_id == kGrowl || ev->param_id == kPresence || ev->param_id == kCab)
            filterDirty = true;
        }
      }
    }
    if (filterDirty) updateFilters();
  }

  double shape(double x, double drive, double fuzz, double growl, Channel &c) {
    const double env = c.envelope.lowpass(std::abs(x));
    const double pre = 1.0 + drive * 0.42;
    const double asym = 0.025 * growl + 0.06 * fuzz * std::clamp(env * 4.0, 0.0, 1.0);
    const double biased = x * pre + asym;
    const double soft = std::tanh(biased);
    const double cubic = biased - 0.18 * biased * biased * biased;
    const double fuzzMix = fuzz / 10.0;
    double y = soft * (1.0 - fuzzMix * 0.58) + clamp1(cubic) * fuzzMix * 0.58;
    const double octaveLike = std::copysign(std::sqrt(std::abs(y) + 1.0e-12), y);
    y = y * (1.0 - 0.12 * fuzzMix) + octaveLike * (0.12 * fuzzMix);
    c.dc = 0.995 * c.dc + 0.005 * y;
    return y - c.dc;
  }

  clap_process_status process(const clap_process *pr) {
    handleEvents(pr->in_events);
    if (pr->audio_inputs_count < 1 || pr->audio_outputs_count < 1) return CLAP_PROCESS_CONTINUE;
    const auto &in = pr->audio_inputs[0];
    auto &out = pr->audio_outputs[0];
    const uint32_t channels = std::min<uint32_t>({in.channel_count, out.channel_count, 2});
    if (channels == 0) return CLAP_PROCESS_CONTINUE;

    const double inputGain = dbToGain(values_[kInput].load());
    const double outputGain = dbToGain(values_[kOutput].load());
    const double drive = values_[kDrive].load();
    const double fuzz = values_[kFuzz].load();
    const double growl = values_[kGrowl].load();
    const double mix = values_[kMix].load() / 100.0;
    const double threshold = dbToGain(values_[kGate].load());
    const double attack = 1.0 - std::exp(-1.0 / (sampleRate_ * 0.0015));
    const double release = 1.0 - std::exp(-1.0 / (sampleRate_ * 0.055));

    for (uint32_t c = 0; c < channels; ++c) {
      Channel &st = ch_[c];
      if (in.data32 && out.data32) {
        const float *src = in.data32[c]; float *dst = out.data32[c];
        for (uint32_t i = 0; i < pr->frames_count; ++i) {
          const double dry = src[i];
          const double target = std::abs(dry) >= threshold ? 1.0 : 0.0;
          st.gateGain += (target - st.gateGain) * (target > st.gateGain ? attack : release);
          double x = dry * inputGain * st.gateGain;
          x = st.tightHp.process(x);
          x = st.growlPeak.process(x);
          x = shape(x, drive, fuzz, growl, st);
          x = st.presencePeak.process(x);
          x = st.cabLp.process(x);
          dst[i] = static_cast<float>((dry * (1.0 - mix) + x * mix) * outputGain);
        }
      } else if (in.data64 && out.data64) {
        const double *src = in.data64[c]; double *dst = out.data64[c];
        for (uint32_t i = 0; i < pr->frames_count; ++i) {
          const double dry = src[i];
          const double target = std::abs(dry) >= threshold ? 1.0 : 0.0;
          st.gateGain += (target - st.gateGain) * (target > st.gateGain ? attack : release);
          double x = dry * inputGain * st.gateGain;
          x = st.tightHp.process(x);
          x = st.growlPeak.process(x);
          x = shape(x, drive, fuzz, growl, st);
          x = st.presencePeak.process(x);
          x = st.cabLp.process(x);
          dst[i] = (dry * (1.0 - mix) + x * mix) * outputGain;
        }
      }
    }
    return CLAP_PROCESS_CONTINUE;
  }

  static uint32_t audioPortsCount(const clap_plugin *, bool) { return 1; }
  static bool audioPortsGet(const clap_plugin *, uint32_t index, bool isInput, clap_audio_port_info *info) {
    if (index != 0 || !info) return false;
    info->id = isInput ? 0 : 1;
    std::snprintf(info->name, CLAP_NAME_SIZE, "%s", isInput ? "Input" : "Output");
    info->flags = CLAP_AUDIO_PORT_IS_MAIN;
    info->channel_count = 2;
    info->port_type = CLAP_PORT_STEREO;
    info->in_place_pair = isInput ? 1 : 0;
    return true;
  }

  static uint32_t paramsCount(const clap_plugin *) { return kParamCount; }
  static bool paramsGetInfo(const clap_plugin *, uint32_t index, clap_param_info *info) {
    if (!info || index >= kParamCount) return false;
    const auto &d = kParams[index];
    info->id = d.id;
    info->flags = CLAP_PARAM_IS_AUTOMATABLE;
    info->cookie = nullptr;
    std::snprintf(info->name, CLAP_NAME_SIZE, "%s", d.name);
    info->module[0] = '\0';
    info->min_value = d.min; info->max_value = d.max; info->default_value = d.def;
    return true;
  }
  static bool paramsGetValue(const clap_plugin *p, clap_id id, double *value) {
    if (!value || id >= kParamCount) return false;
    *value = self(p)->values_[id].load(); return true;
  }
  static bool paramsValueToText(const clap_plugin *, clap_id id, double value, char *display, uint32_t size) {
    if (!display || size == 0 || id >= kParamCount) return false;
    const auto &d = kParams[id];
    if (d.unit[0]) std::snprintf(display, size, "%.2f %s", value, d.unit);
    else std::snprintf(display, size, "%.2f", value);
    return true;
  }
  static bool paramsTextToValue(const clap_plugin *, clap_id id, const char *text, double *value) {
    if (!text || !value || id >= kParamCount) return false;
    char *end = nullptr; const double v = std::strtod(text, &end);
    if (end == text) return false;
    *value = std::clamp(v, kParams[id].min, kParams[id].max); return true;
  }
  static void paramsFlush(const clap_plugin *p, const clap_input_events *in, const clap_output_events *) {
    self(p)->handleEvents(in);
  }

  static bool stateSave(const clap_plugin *p, const clap_ostream *stream) {
    if (!stream) return false;
    std::array<double, kParamCount> data{};
    for (size_t i = 0; i < data.size(); ++i) data[i] = self(p)->values_[i].load();
    const uint8_t *ptr = reinterpret_cast<const uint8_t *>(data.data());
    int64_t left = static_cast<int64_t>(sizeof(data));
    while (left > 0) { const int64_t n = stream->write(stream, ptr, static_cast<uint64_t>(left)); if (n <= 0) return false; ptr += n; left -= n; }
    return true;
  }
  static bool stateLoad(const clap_plugin *p, const clap_istream *stream) {
    if (!stream) return false;
    std::array<double, kParamCount> data{};
    uint8_t *ptr = reinterpret_cast<uint8_t *>(data.data());
    int64_t left = static_cast<int64_t>(sizeof(data));
    while (left > 0) { const int64_t n = stream->read(stream, ptr, static_cast<uint64_t>(left)); if (n <= 0) return false; ptr += n; left -= n; }
    for (size_t i = 0; i < data.size(); ++i) self(p)->values_[i].store(std::clamp(data[i], kParams[i].min, kParams[i].max));
    self(p)->updateFilters(); return true;
  }

  const void *getExtension(const char *id) {
    if (!std::strcmp(id, CLAP_EXT_AUDIO_PORTS)) return &audioPorts_;
    if (!std::strcmp(id, CLAP_EXT_PARAMS)) return &params_;
    if (!std::strcmp(id, CLAP_EXT_STATE)) return &state_;
    return nullptr;
  }

  const clap_host *host_ = nullptr;
  clap_plugin plugin_{};
  std::array<std::atomic<double>, kParamCount> values_{};
  std::array<Channel, 2> ch_{};
  double sampleRate_ = 48000.0;
  bool processing_ = false;

  static inline const char *features_[] = {CLAP_PLUGIN_FEATURE_AUDIO_EFFECT, CLAP_PLUGIN_FEATURE_DISTORTION, CLAP_PLUGIN_FEATURE_STEREO, nullptr};
  static inline const clap_plugin_descriptor descriptor_{CLAP_VERSION, kPluginId, "GrowlForge", "Tocsaine / OpenAI", "", "", "0.1.0", "Tight modern metal growl, controlled fuzz and cabinet filtering", features_};
  static inline const clap_plugin_audio_ports audioPorts_{audioPortsCount, audioPortsGet};
  static inline const clap_plugin_params params_{paramsCount, paramsGetInfo, paramsGetValue, paramsValueToText, paramsTextToValue, paramsFlush};
  static inline const clap_plugin_state state_{stateSave, stateLoad};
};

// Access descriptor through a local duplicate because descriptor_ is private.
static const char *factoryFeatures[] = {CLAP_PLUGIN_FEATURE_AUDIO_EFFECT, CLAP_PLUGIN_FEATURE_DISTORTION, CLAP_PLUGIN_FEATURE_STEREO, nullptr};
static const clap_plugin_descriptor factoryDescriptor{CLAP_VERSION, kPluginId, "GrowlForge", "Tocsaine / OpenAI", "", "", "0.1.0", "Tight modern metal growl, controlled fuzz and cabinet filtering", factoryFeatures};

uint32_t factoryCount(const clap_plugin_factory *) { return 1; }
const clap_plugin_descriptor *factoryDescriptorAt(const clap_plugin_factory *, uint32_t index) { return index == 0 ? &factoryDescriptor : nullptr; }
const clap_plugin *factoryCreate(const clap_plugin_factory *, const clap_host *host, const char *pluginId) {
  if (!host || !pluginId || std::strcmp(pluginId, kPluginId) != 0 || !clap_version_is_compatible(host->clap_version)) return nullptr;
  auto *instance = new (std::nothrow) GrowlForge(host);
  return instance ? instance->plugin() : nullptr;
}

const clap_plugin_factory factory{factoryCount, factoryDescriptorAt, factoryCreate};

bool entryInit(const char *) { return true; }
void entryDeinit() {}
const void *entryGetFactory(const char *factoryId) {
  return factoryId && !std::strcmp(factoryId, CLAP_PLUGIN_FACTORY_ID) ? &factory : nullptr;
}

} // namespace

extern "C" {
#ifdef _WIN32
__declspec(dllexport)
#else
__attribute__((visibility("default")))
#endif
const clap_plugin_entry clap_entry{CLAP_VERSION, entryInit, entryDeinit, entryGetFactory};
}
