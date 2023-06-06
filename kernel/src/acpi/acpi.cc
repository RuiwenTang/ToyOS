#include "acpi/acpi.hpp"

#include <lai/core.h>
#include <lai/helpers/pc-bios.h>
#include <lai/helpers/sci.h>

#include "kprintf.h"

namespace toy {

void ACPI::Initialize() {
  lai_state_t state{};

  lai_init_state(&state);

  lai_create_namespace();

  auto r = lai_enable_acpi(1);

  if (r != 0) {
    lai_finalize_state(&state);
    return;
  }
}

}  // namespace toy