#include <stdio.h>

#include <functional>

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

int main(int argc, const char** argv) {
  Base* b = new SubBase();

  std::function<void()> f = [b]() { b->say(); };

  f();

  delete b;

  return 0;
}
