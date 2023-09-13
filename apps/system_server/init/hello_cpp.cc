#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <functional>
#include <vector>

class Base {
 public:
  Base() = default;
  virtual ~Base() = default;

  virtual void say() = 0;
};

class SubBase : public Base {
 public:
  SubBase() = default;
  ~SubBase() override = default;

  void say() override { printf("Hello World\n"); }
};

void inner_func() {
  std::vector<int> arrs{};

  arrs.emplace_back(1);
  arrs.emplace_back(2);

  printf("arrs.size = %ld\n", arrs.size());
}

int main(int argc, const char** argv) {
  Base* b = new SubBase();

  std::function<void()> f = [b]() { b->say(); };

  f();
  f();

  delete b;

  inner_func();

  return 0;
}
