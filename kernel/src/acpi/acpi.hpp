
#include <stddef.h>
#include <stdint.h>

namespace toy {

class ACPI final {
 public:
  ACPI() = delete;
  ~ACPI() = delete;

  static void Initialize();
};

}  // namespace toy