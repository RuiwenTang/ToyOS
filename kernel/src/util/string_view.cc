#ifdef TOY_HOST_TEST
#include "string_view.hpp"
#else
#include "util/string_view.hpp"
#endif

#include <string.h>

#ifdef __toyos__

#include "mmu/heap.h"

#define malloc kmalloc
#define realloc krealloc
#define free kfree

#else

#include <cstdlib>

#endif

namespace util {

StringView::StringView(const char* str) {
  if (str) {
    auto len = strlen(str);

    m_size = len;
    m_capacity = m_size + 1;
    m_str = reinterpret_cast<char*>(malloc(m_capacity));

    memcpy(m_str, str, len);

    m_str[len] = '\0';
  }
}

StringView::StringView(const StringView& str) {
  if (str.size()) {
    m_size = str.size();
    m_capacity = str.m_capacity;

    m_str = reinterpret_cast<char*>(malloc(m_capacity));

    memcpy(m_str, str.c_str(), m_size);

    m_str[m_size] = '\0';
  }
}

StringView::~StringView() {
  if (m_str) {
    free(m_str);
  }
}

StringView& StringView::operator=(const StringView& str) {
  if (str.size() == 0) {
    m_size = 0;

    return *this;
  }

  if (m_capacity <= str.size()) {
    uint32_t new_capacity = 2 * m_capacity;

    if (new_capacity <= str.size()) {
      new_capacity = str.m_capacity;
    }

    free(m_str);

    m_str = reinterpret_cast<char*>(malloc(new_capacity));

    m_capacity = new_capacity;
  }

  memcpy(m_str, str.m_str, str.size());

  m_size = str.size();

  m_str[m_size] = '\0';

  return *this;
}

StringView StringView::operator+(const StringView& str) {
  StringView new_str{*this};

  new_str += str;

  return new_str;
}

StringView StringView::operator+(const char* str) {
  StringView new_str{*this};

  new_str += str;

  return new_str;
}

StringView& StringView::operator+=(const StringView& str) {
  if (str.size() == 0) {
    return *this;
  }

  if (m_size + str.size() >= m_capacity) {
    uint32_t new_capacity = 2 * m_capacity;

    if (new_capacity < m_size + str.size() + 1) {
      new_capacity = m_size + str.size() + 1;
    }

    m_str = reinterpret_cast<char*>(realloc(m_str, new_capacity));

    m_capacity = new_capacity;
  }

  memcpy(m_str + m_size, str.m_str, str.size());

  m_size += str.size();

  m_str[m_size] = '\0';

  return *this;
}

StringView& StringView::operator+=(const char* str) {
  size_t str_len = strlen(str);

  if (m_size + str_len >= m_capacity) {
    uint32_t new_capacity = 2 * m_capacity;

    if (new_capacity < m_size + str_len + 1) {
      new_capacity = m_size + str_len + 1;
    }

    m_str = reinterpret_cast<char*>(realloc(m_str, new_capacity));

    m_capacity = new_capacity;
  }

  memcpy(m_str + m_size, str, str_len);

  m_size += str_len;

  m_str[m_size] = '\0';

  return *this;
}

bool StringView::operator==(const StringView& str) const {
  if (m_size != str.size()) {
    return false;
  }

  if (m_size == 0 && str.size() == 0) {
    return true;
  }

  return strcmp(m_str, str.c_str()) == 0;
}

bool StringView::operator==(const char* str) const {
  size_t str_len = strlen(str);

  if (m_size != str_len) {
    return false;
  }

  if (m_size == 0 && str_len == 0) {
    return true;
  }

  return strcmp(m_str, str) == 0;
}

char StringView::operator[](uint32_t index) const {
  if (index >= m_size) {
    return '\0';
  }

  return m_str[index];
}

uint32_t StringView::size() const { return m_size; }

const char* StringView::c_str() const { return m_str; }

}  // namespace util
