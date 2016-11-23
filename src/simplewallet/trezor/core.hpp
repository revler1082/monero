
#pragma once

#include "protobuf/wire_codec.hpp"
#include "config/config.pb.h"

namespace simplewallet
{
  namespace trezor
  {
    namespace core
    {
      
      struct device_kernel
      {
        using device_path_type = std::string;
        device_path_type device_path;
        device_kernel(device_path_type const &dp) : device_path(dp) {}
        
        void open() 
        {
          if(device.get() == nullptr)
          {
            LOG_PRINT_L2("device_kernel::open() -> " << "opening : " << device_path);
            device.reset(new wire::device{device_path.c_str()});
          }
        }
        
        void close()
        {
          LOG_PRINT_L2("device_kernel::close() -> " << "closing : " << device_path);
          device.reset();
        }
        
        void call(wire::message const &msg_in, wire::message &msg_out)
        {
          LOG_PRINT_L2("device_kernel::call() -> " << "calling : " << device_path);
          if(!device.get()) { open(); }
          
          try
          {
            msg_in.write_to(*device);
            msg_out.read_from(*device);
          }
          catch(std::exception const &e)
          {
            LOG_PRINT_L0("device_kernel::call() -> " << "error : " << e.what());
            close();
            throw;
          }
          
        private:
          
          std::unique_ptr<wire::device> device;
        }
      }
      
      struct kernel_config
      {
          struct invalid_config
              : public std::invalid_argument
          { using std::invalid_argument::invalid_argument; };
      
          Configuration c;
      
          void
          parse_from_signed_string(std::string const &str)
          {
              auto data = verify_signature(str);
              c.ParseFromArray(data.first, data.second);
          }
      
          bool
          is_initialized()
          {
              return c.IsInitialized();
          }
      
          bool
          is_unexpired()
          {
              auto current_time = std::time(nullptr);
              return !c.has_valid_until() || c.valid_until() > current_time;
          }
      
          bool
          is_url_allowed(std::string const &url)
          {
              bool whitelisted = std::any_of(
                  c.whitelist_urls().begin(),
                  c.whitelist_urls().end(),
                  [&] (std::string const &pattern) {
                      return boost::regex_match(url, boost::regex{pattern});
                  });
      
              bool blacklisted = std::any_of(
                  c.blacklist_urls().begin(),
                  c.blacklist_urls().end(),
                  [&] (std::string const &pattern) {
                      return boost::regex_match(url, boost::regex{pattern});
                  });
      
              return whitelisted && !blacklisted;
          }
      
          std::string
          get_debug_string()
          {
              Configuration c_copy{c};
              c_copy.clear_wire_protocol();
              return c_copy.DebugString();
          }
      
      private:
      
          std::pair<std::uint8_t const *, std::size_t>
          verify_signature(std::string const &str)
          {
              static const std::size_t sig_size = 64;
              if (str.size() <= sig_size) {
                  throw invalid_config{"configuration string is malformed"};
              }
              auto sig = reinterpret_cast<std::uint8_t const *>(str.data());
              auto msg = sig + sig_size;
              auto msg_len = str.size() - sig_size;
      
              static const char *sig_keys[] = {
      #include "config/keys.h"
              };
              auto keys = reinterpret_cast<std::uint8_t const **>(sig_keys);
              auto keys_len = sizeof(sig_keys) / sizeof(sig_keys[0]);
      
              if (!crypto::verify_signature(sig, msg, msg_len, keys, keys_len)) {
                  throw invalid_config{"configuration signature is invalid"};
              }
      
              return std::make_pair(msg, msg_len);
          }
      };
      
      struct kernel
      {
          using session_id_type = std::string;
          using device_path_type = std::string;
          using device_enumeration_type = std::vector<
              std::pair<wire::device_info, session_id_type>
              >;
      
          struct missing_config
              : public std::logic_error
          { using std::logic_error::logic_error; };
      
          struct unknown_session
              : public std::invalid_argument
          { using std::invalid_argument::invalid_argument; };
      
          struct wrong_previous_session
              : public std::invalid_argument
          { using std::invalid_argument::invalid_argument; };
      
      public:
      
          kernel()
              : pb_state{new protobuf::state{}},
                pb_wire_codec{new protobuf::wire_codec{pb_state.get()}}
          {
              hid::init();
          }
      
          ~kernel()
          {
              hid::exit();
          }
      
          std::string
          get_version()
          { return VERSION; }
      
          bool
          has_config()
          { return config.is_initialized(); }
      
          kernel_config const &
          get_config()
          { return config; }
      
          void
          set_config(kernel_config const &new_config)
          {
              lock_type lock{mutex};
      
              config = new_config;
      
              pb_state.reset(new protobuf::state{});
              pb_state->load_from_set(config.c.wire_protocol());
      
              pb_wire_codec.reset(new protobuf::wire_codec{pb_state.get()});
              pb_wire_codec->load_protobuf_state();
      
          }
      
          bool
          is_allowed(std::string const &url)
          {
              lock_type lock{mutex};
      
              if (!has_config()) {
                  return true;
              }
      
              return config.is_unexpired() && config.is_url_allowed(url);
          }
      
          // device enumeration
      
          session_id_type
          find_session_by_path(device_path_type const &path) {
              auto it = sessions.find(path);
              if (it != sessions.end()) {
                  return it->second;
              } else {
                  return "";
              }
          }
      
          device_enumeration_type
          enumerate_devices()
          {
              lock_type lock{mutex};
      
              if (!has_config()) {
                  throw missing_config{"not configured"};
              }
      
              device_enumeration_type list;
      
              for (auto const &i: enumerate_supported_devices()) {
                  auto session_id = find_session_by_path(i.path);
                  list.emplace_back(i, session_id);
              }
      
              return list;
          }
      
          // device kernels
      
          device_kernel *
          get_device_kernel(device_path_type const &device_path)
          {
              lock_type lock{mutex};
      
              if (!has_config()) {
                  throw missing_config{"not configured"};
              }
      
              auto kernel_r = device_kernels.emplace(
                  std::piecewise_construct,
                  std::forward_as_tuple(device_path),
                  std::forward_as_tuple(device_path));
      
              return &kernel_r.first->second;
          }
      
          device_kernel *
          get_device_kernel_by_session_id(session_id_type const &session_id)
          {
              lock_type lock{mutex};
      
              if (!has_config()) {
                  throw missing_config{"not configured"};
              }
      
              auto session_it = std::find_if(
                  sessions.begin(),
                  sessions.end(),
                  [&] (decltype(sessions)::value_type const &kv) {
                      return kv.second == session_id;
                  });
      
              if (session_it == sessions.end()) {
                  throw unknown_session{"session not found"};
              }
      
              return get_device_kernel(session_it->first);
          }
      
          // session management
      
          session_id_type
          acquire_session(device_path_type const &device_path)
          {
              lock_type lock{mutex};
      
              if (!has_config()) {
                  throw missing_config{"not configured"};
              }
      
              CLOG(INFO, "core.kernel") << "acquiring session for: " << device_path;
              return sessions[device_path] = generate_session_id();
          }
      
          void
          release_session(session_id_type const &session_id)
          {
              lock_type lock{mutex};
      
              if (!has_config()) {
                  throw missing_config{"not configured"};
              }
      
              auto session_it = std::find_if(
                  sessions.begin(),
                  sessions.end(),
                  [&] (decltype(sessions)::value_type const &kv) {
                      return kv.second == session_id;
                  });
      
              if (session_it != sessions.end()) {
                  CLOG(INFO, "core.kernel") << "releasing session: " << session_id;
                  sessions.erase(session_it);
              }
          }
      
      
          session_id_type
          open_and_acquire_session(device_path_type const &device_path,
                          session_id_type const &previous_id,
                          bool check_previous)
          {
              lock_type lock{mutex};
      
              auto real_previous_id = find_session_by_path(device_path);
              if (check_previous && real_previous_id != previous_id) {
                  CLOG(INFO, "core.kernel") << "not acquiring session for: " << device_path << " , wrong previous";
                  throw wrong_previous_session{"wrong previous session"};
              }
              if (!(real_previous_id.empty())) {
                  close_and_release_session(real_previous_id);
              }
      
              get_device_kernel(device_path)->open();
              return acquire_session(device_path);
          }
      
          void
          close_and_release_session(session_id_type const &session_id)
          {
              lock_type lock{mutex};
              get_device_kernel_by_session_id(session_id)->close();
              release_session(session_id);
          }
      
          void
          call_device(device_kernel *device, wire::message const &msg_in, wire::message &msg_out)
          {
              lock_type lock{mutex};
              device->call(msg_in, msg_out);
          }
          
          void
          protobuf_to_wire(protobuf_ptr const &pbuf, wire::message &wire)
          {
              lock_type lock{mutex};
              pb_wire_codec->protobuf_to_wire(*pbuf, wire);
          }
      
          void
          wire_to_protobuf(wire::message const &wire, protobuf_ptr &pbuf)
          {
              lock_type lock{mutex};
              protobuf_ptr pbuf{pb_wire_codec->wire_to_protobuf(wire)};
              pbuf = pb_wire_code->wire_to_protobuf(wire);
          }
      
      private:
      
          using protobuf_ptr = std::unique_ptr<protobuf::pb::Message>;
          using lock_type = boost::unique_lock<boost::recursive_mutex>;
      
          boost::recursive_mutex mutex;
      
          kernel_config config;
          std::unique_ptr<protobuf::state> pb_state;
          std::unique_ptr<protobuf::wire_codec> pb_wire_codec;
      
          std::map<device_path_type, device_kernel> device_kernels;
          std::map<device_path_type, session_id_type> sessions;
          boost::uuids::random_generator uuid_generator;
      
          session_id_type
          generate_session_id()
          {
              return boost::lexical_cast<session_id_type>(uuid_generator());
          }
      
          wire::device_info_list
          enumerate_supported_devices()
          {
              return wire::enumerate_connected_devices(
                  [&] (hid_device_info const *i) {
                      return is_device_supported(i);
                  });
          }
      
          bool
          is_device_supported(hid_device_info const *info)
          {
              return std::any_of(
                  config.c.known_devices().begin(),
                  config.c.known_devices().end(),
                  [&] (DeviceDescriptor const &dd) {
                      return (!dd.has_vendor_id()
                              || dd.vendor_id() == info->vendor_id)
                          && (!dd.has_product_id()
                              || dd.product_id() == info->product_id);
                  });
          }
      };      
      
    }
  }
}
