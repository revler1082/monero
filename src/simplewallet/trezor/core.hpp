
#pragma once

#include <boost/bind.hpp>
#include <boost/regex.hpp>
#include <boost/thread.hpp>
#include <boost/lexical_cast.hpp>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include "wire.hpp"

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
            device.reset(new wire::device{device_path.c_str()});
          }
        }

        void close()
        {
          device.reset();
        }

        void call(wire::message const &msg_in, wire::message &msg_out)
        {
          if(!device.get()) { open(); }

          try
          {
            msg_in.write_to(*device);
            msg_out.read_from(*device);
          }
          catch(std::exception const &e)
          {
            close();
            throw;
          }
        }

      private:

        std::unique_ptr<wire::device> device;
      };

      struct kernel
      {
        using session_id_type = std::string;
        using device_path_type = std::string;
        using device_enumeration_type = std::vector<std::pair<wire::device_info, session_id_type>>;

        struct unknown_session : public std::invalid_argument { using std::invalid_argument::invalid_argument; };
        struct wrong_previous_session : public std::invalid_argument { using std::invalid_argument::invalid_argument; };

      public:

        kernel()
        {
            hid::init();
        }

        ~kernel()
        {
            hid::exit();
        }

        // device enumeration

        session_id_type find_session_by_path(device_path_type const &path)
        {
          auto it = sessions.find(path);
          if (it != sessions.end()) return it->second;
          else return "";
        }

        device_enumeration_type enumerate_devices()
        {
          lock_type lock{mutex};

          device_enumeration_type list;
          std::cout << "enumerate_devices" << std::endl;
          for (auto const &i: enumerate_supported_devices())
          {
            auto session_id = find_session_by_path(i.path);
            list.emplace_back(i, session_id);
          }

          return list;
        }

        // device kernels

        device_kernel* get_device_kernel(device_path_type const &device_path)
        {
            lock_type lock{mutex};

            auto kernel_r = device_kernels.emplace(
                std::piecewise_construct,
                std::forward_as_tuple(device_path),
                std::forward_as_tuple(device_path));

            return &kernel_r.first->second;
        }

        device_kernel* get_device_kernel_by_session_id(session_id_type const &session_id)
        {
          lock_type lock{mutex};

          auto session_it = std::find_if(
            sessions.begin(),
            sessions.end(),
            [&] (decltype(sessions)::value_type const &kv) {
              return kv.second == session_id;
            });

          if (session_it == sessions.end())
          {
            throw unknown_session { "session not found" };
          }

          return get_device_kernel(session_it->first);
        }

        // session management

        session_id_type acquire_session(device_path_type const &device_path)
        {
          lock_type lock{mutex};
          return sessions[device_path] = generate_session_id();
        }

        void release_session(session_id_type const &session_id)
        {
          lock_type lock{mutex};

          auto session_it = std::find_if(
            sessions.begin(),
            sessions.end(),
            [&] (decltype(sessions)::value_type const &kv) {
              return kv.second == session_id;
            });

          if (session_it != sessions.end())
          {
            sessions.erase(session_it);
          }
        }

        session_id_type open_and_acquire_session(device_path_type const &device_path, session_id_type const &previous_id, bool check_previous)
        {
          lock_type lock{mutex};

          auto real_previous_id = find_session_by_path(device_path);
          if (check_previous && real_previous_id != previous_id)
          {
            throw wrong_previous_session{"wrong previous session"};
          }

          if (!(real_previous_id.empty()))
          {
            close_and_release_session(real_previous_id);
          }

          get_device_kernel(device_path)->open();
          return acquire_session(device_path);
        }

        void close_and_release_session(session_id_type const &session_id)
        {
          lock_type lock{mutex};
          get_device_kernel_by_session_id(session_id)->close();
          release_session(session_id);
        }

        void call_device(device_kernel *device, wire::message const &msg_in, wire::message &msg_out)
        {
            lock_type lock{mutex};
            device->call(msg_in, msg_out);
        }

      private:

        using lock_type = boost::unique_lock<boost::recursive_mutex>;
        boost::recursive_mutex mutex;
        std::map<device_path_type, device_kernel> device_kernels;
        std::map<device_path_type, session_id_type> sessions;
        boost::uuids::random_generator uuid_generator;

        session_id_type generate_session_id()
        {
          return boost::lexical_cast<session_id_type>(uuid_generator());
        }

        wire::device_info_list enumerate_supported_devices()
        {
          std::cout << "enumerate_supported_devices " << std::endl;
          return wire::enumerate_connected_devices
          (
            [&] (hid_device_info const *i)
            {
              return is_device_supported(i);
            }
          );
        }

        bool is_device_supported(hid_device_info const *info)
        {
          return true;
          //return 0x534c == info->vendor_id && 0x0001 == info->product_id;
        }
      };

    }
  }
}
