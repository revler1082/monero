#pragma once

#include "wire_code.hpp";

namespace simplewallet
{
  namespace trezor
  {
    namespace api
    {
    
      class trezor
      {
        private:
          
          using protobuf_ptr = std::unique_ptr<protobuf::pb::Message>;
          std::unique_ptr<core::kernel> m_kernel;
          std::string m_session_id;
          bool handle_call(const protobuf_ptr& pbuf_in, protobuf_ptr& pbuf_out);          
          
        public:
          
          trezor()
          {
            
          }
          
          ~trezor()
          {
            
          }
          
          bool initialize();
          
      }
      
    }
  }
}