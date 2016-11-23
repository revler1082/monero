
#include <string>

#include "core.hpp"
#include "trezor.h"

namespace simplewallet
{
  namespace trezor
  {
    namespace api
    {
      
      trezor::trezor() :
      
      {
        
      }
      
      trezor::~trezor()
      {
        
      }
      
      bool handle_call(const protobuf_ptr& pbuf_in, protobuf_ptr& pbuf_out)
      {
        core::device_kernel *device;
        device = kernel->get_device_kernel_by_session_id(m_session_id); // throws
        wire::message wire_in;
        wire::message wire_out;        
        kernel->pbuf_to_wire(pbuf_in, wire_in);
        kernel->call_device(device, wire_in, wire_out);
        kernel->wire_to_pbuf(wire_out, pbuf_out);
      }
      
      bool trezor::initalize()
      {
        try 
        {
          //auto device_path = decode_device_path(request.url_params.str(1));
          
          // slight hack to keep the types correct
          auto check_previous = request.url_params.size() > 2;
          auto previous_or_null = check_previous ? request.url_params.str(2) : "null";
          auto previous = previous_or_null == "null" ? "" : previous_or_null;
        
          GetFeatures
        
          auto session_id = kernel->open_and_acquire_session(device_path, previous, check_previous);
          return json_response(200, {{"session", session_id}});
        }
      }
      
      bool trezor::other_func()
      {
        auto session_id = request.url_params.str(1);
        wire::message wire_in;
        wire::message wire_out;
        kernel->call_device(device, wire_in, wire_out);        
      }
      
    }
  }
}