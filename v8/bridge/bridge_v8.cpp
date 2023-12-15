#include "bridge_v8.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <memory>
#include <algorithm>
#include <chrono>
#include <optional>
#include <ctime>
#include <dirent.h>
#include <sys/stat.h>
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/fmt/bundled/color.h"
#define PATH_MAX_LEN 2048

namespace util
{
  inline std::string &ltrim(std::string &str, const std::string &chars = "\t\n\v\f\r ")
  {
    str.erase(0, str.find_first_not_of(chars));
    return str;
  }

  inline std::string &rtrim(std::string &str, const std::string &chars = "\t\n\v\f\r ")
  {
    str.erase(str.find_last_not_of(chars) + 1);
    return str;
  }

  inline std::string &trim(std::string &str, const std::string &chars = "\t\n\v\f\r ")
  {
    return ltrim(rtrim(str, chars), chars);
  }

  inline std::string &find_and_replace(std::string &&str, const char *find, const char *replace)
  {
    size_t f_len = strlen(find), r_len = strlen(replace);
    for (std::string::size_type i = 0; (i = str.find(find, i)) != std::string::npos;)
    {
      str.replace(i, f_len, replace);
      i += r_len;
    }
    return str;
  }

  inline std::string &find_and_replace(std::string &str, const char *find, const char *replace)
  {
    return find_and_replace(std::move(str), find, replace);
  }

  inline bool ends_with(const std::string &str, const std::string &match)
  {
    if (str.size() >= match.size() &&
        str.compare(str.size() - match.size(), match.size(), match) == 0)
    {
      return true;
    }
    else
    {
      return false;
    }
  }

  inline void base_name(const std::string &input,
                        std::string &base_path,
                        std::string &file_name)
  {
    if (input.find("/") != std::string::npos)
    {
      base_path = input.substr(0, input.find_last_of("/"));
      file_name = input.substr(input.find_last_of("/") + 1);
    }
    else
    {
      base_path = ".";
      file_name = input;
    }
  }

  size_t file_get_contents(const char *filename,
                           std::vector<char> &v,
                           spdlog::logger *log,
                           int &error)
  {
    ::FILE *fp = ::fopen(filename, "rb");
    if (fp == nullptr)
    {
      if (log)
      {
        log->error("{}:{}", filename, strerror(errno));
      }
      error = 1;
      return 0;
    }
    ::fseek(fp, 0, SEEK_END);
    long sz = ::ftell(fp);
    v.resize(sz);
    if (sz)
    {
      ::rewind(fp);
      ::fread(&(v)[0], 1, v.size(), fp);
    }
    ::fclose(fp);
    return v.size();
  }

}

namespace bridge_v8
{
  std::unique_ptr<v8::Platform> isolate::platform;

  bool isolate::init_V8(int argc, const char *argv[])
  {
    // Initialize V8.
    v8::V8::InitializeICUDefaultLocation(argv[0]);
    v8::V8::InitializeExternalStartupData(argv[0]);
    platform = v8::platform::NewDefaultPlatform();
    v8::V8::InitializePlatform(platform.get());
    return v8::V8::Initialize();
  }

  void isolate::stop_V8()
  {
    // Destroy V8.
    v8::V8::Dispose();
  }

  int isolate::load_scripts()
  {
    int res = 0;
    DIR *dir;
    struct dirent *ent;

    if ((dir = opendir(cfg_.in_path.c_str())) != nullptr)
    {
      while ((ent = readdir(dir)) != nullptr)
      {
        if (strcmp(ent->d_name, ".") && strcmp(ent->d_name, ".."))
        {
          struct stat info;
          std::ostringstream fpath;
          fpath << cfg_.in_path << "/" << ent->d_name;
          if (processed_scripts_.find(fpath.str()) == processed_scripts_.end())
          {
            processed_scripts_.insert(fpath.str());
          }
          else
          {
            continue;
          }

          stat(fpath.str().c_str(), &info);
          if (!S_ISDIR(info.st_mode))
          {
            if (!util::ends_with(ent->d_name, ".js"))
            {
              continue;
            }
            event_log_->trace("load_scripts:{}", ent->d_name);

            // read the script's source
            v8::Local<v8::String> source;
            if (!read_script_file(fpath.str().c_str()).ToLocal(&source))
            {
              res = 1;
              break;
            }

            // compile the script
            std::string error;
            v8::Local<v8::Script> script;
            if (!compile_script(source, script, error))
            {
              event_log_->error("error compiling script file:{} {}", ent->d_name, error);
              res = 1;
              break;
            }

            v8::Local<v8::Value> result;
            if (!run_script(script, result, error))
            {
              event_log_->error("error running script: {}", error);
              res = 1;
              break;
            }
          }
        }
      }

      if (closedir(dir))
      {
        event_log_->error("{}", strerror(errno));
      }
    }
    else
    {
      event_log_->error("{}", strerror(errno));
      res = 1;
    }

    return res;
  }

  // Reads a script into a v8 string.
  v8::MaybeLocal<v8::String> isolate::read_script_file(const std::string &name)
  {
    std::vector<char> content;
    int error = 0;
    if (!util::file_get_contents(name.c_str(), content, event_log_.get(), error))
    {
      return v8::MaybeLocal<v8::String>();
    }

    v8::MaybeLocal<v8::String> result = v8::String::NewFromUtf8(isolate_,
                                                                content.data(),
                                                                v8::NewStringType::kNormal,
                                                                static_cast<int>(content.size()));

    return result;
  }

  bool isolate::compile_script(const v8::Local<v8::String> &script_src,
                               v8::Local<v8::Script> &script,
                               std::string &error)
  {
    v8::Local<v8::Context> context = scenario_context_.Get(isolate_);
    v8::Context::Scope context_scope(context);
    v8::TryCatch try_catch(isolate_);

    if (!v8::Script::Compile(context, script_src).ToLocal(&script))
    {
      v8::String::Utf8Value utf8err(isolate_, try_catch.Exception());
      error = *utf8err;
      return false;
    }
    return true;
  }

  bool isolate::run_script(const v8::Local<v8::Script> &script,
                           v8::Local<v8::Value> &result,
                           std::string &error)
  {
    v8::Local<v8::Context> context = scenario_context_.Get(isolate_);
    v8::Context::Scope context_scope(context);
    v8::TryCatch try_catch(isolate_);

    if (!script->Run(scenario_context_.Get(isolate_)).ToLocal(&result))
    {
      v8::String::Utf8Value utf8err(isolate_, try_catch.Exception());
      error = *utf8err;
      return false;
    }
    return true;
  }

}
