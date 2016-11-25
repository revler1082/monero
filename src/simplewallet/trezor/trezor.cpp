
#include <string>

#include "core.hpp"
#include "trezor.h"

namespace simplewallet
{
  namespace trezor
  {
    namespace api
    {
      
      bool trezor::initialize()
      {
        for(auto kv : m_kernel->enumerate_devices())
        {
          std::cout << "vendor_id: " << kv.first.vendor_id << " , prod_id:" << kv.first.product_id << std::endl;
        }
        
        return true;
      }
      
    }
  }
}
