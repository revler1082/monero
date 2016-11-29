#pragma once

#include <string>

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

          trezor() : m_kernel(new core::kernel())
          {

          }

          bool initialize()
          {
            std::cout << "enumerating devices" << std::endl;
            for(auto kv : m_kernel->enumerate_devices())
            {
              std::cout << "vendor_id: " << kv.first.vendor_id << " , prod_id:" << kv.first.product_id << std::endl;
            }

            return true;
          }

      };

    }
  }
}
