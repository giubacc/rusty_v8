#pragma once
#include <unordered_map>
#include "libplatform/libplatform.h"
#include "v8.h"
#include "spdlog/spdlog.h"

namespace bridge_v8
{

  struct isolate
  {
    struct cfg
    {
      std::string in_path = "";
    };

    // global V8 platform
    static std::unique_ptr<v8::Platform> platform;

    static bool init_V8(int argc, const char *argv[]);
    static void stop_V8();

    int load_scripts();

    v8::MaybeLocal<v8::String> read_script_file(const std::string &name);

    bool compile_script(const v8::Local<v8::String> &script_src,
                        v8::Local<v8::Script> &script,
                        std::string &error);

    bool run_script(const v8::Local<v8::Script> &script,
                    v8::Local<v8::Value> &result,
                    std::string &error);

    cfg cfg_;

    // processed scripts
    std::unordered_set<std::string> processed_scripts_;

    // the parameters used to instantiate the Isolate.
    v8::Isolate::CreateParams params_;

    // the Isolate associated with this object.
    v8::Isolate *isolate_ = nullptr;

    // the context associated with this object,
    v8::Global<v8::Context> scenario_context_;

    // event logger
    std::string event_log_fmt_;
    std::shared_ptr<spdlog::logger> event_log_;
  };

}
