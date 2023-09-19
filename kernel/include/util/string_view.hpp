#ifndef TOY_KERNEL_UTIL_STRING_HPP
#define TOY_KERNEL_UTIL_STRING_HPP

#include <stdint.h>

namespace util {

class StringView {
 public:
  StringView(const char* str = nullptr);
  StringView(const StringView& str);

  ~StringView();

  StringView& operator=(const StringView& str);
  StringView operator+(const char* str);
  StringView operator+(const StringView& str);
  StringView& operator+=(const StringView& str);
  StringView& operator+=(const char* str);

  bool operator==(const StringView& str) const;
  bool operator==(const char* str) const;

  char operator[](uint32_t index) const;

  uint32_t size() const;

  const char* c_str() const;

 private:
  char* m_str = nullptr;
  uint32_t m_capacity = 0;
  uint32_t m_size = 0;
};

}  // namespace util

#endif  // TOY_KERNEL_UTIL_STRING_HPP
