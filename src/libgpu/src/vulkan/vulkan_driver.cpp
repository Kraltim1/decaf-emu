#include "vulkan_driver.h"
#include "gpu_event.h"
#include "gpu_ringbuffer.h"

namespace vulkan
{

void
Driver::run()
{
   mRunning = true;

   while (mRunning) {
      auto buffer = gpu::ringbuffer::waitForItem();

      if (buffer.numWords) {
         continue;
      }

      gpu::onRetire(buffer.context);
   }
}

void
Driver::stop()
{
   mRunning = false;
   gpu::ringbuffer::awaken();
}

float
Driver::getAverageFPS()
{
   return 0.0f;
}

void
Driver::notifyCpuFlush(void *ptr,
                           uint32_t size)
{
}

void
Driver::notifyGpuFlush(void *ptr,
                           uint32_t size)
{
}

} // namespace vulkan
