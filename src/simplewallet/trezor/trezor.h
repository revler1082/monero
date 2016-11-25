#pragma once

#include "core.hpp"

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
          
        public:
          
          bool initialize();
          
      };
      
    }
  }
}