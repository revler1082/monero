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
          
          std::unique_ptr<core::kernel> m_kernel;
          std::string m_session_id;
          bool handle_call(const wire_codec::pbuf_type& pbuf, const wire::message& wire_in, wire::message& wire_out);          
          
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