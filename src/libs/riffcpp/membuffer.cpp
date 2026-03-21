#include "membuffer.hpp"

std::streambuf::pos_type membuffer::seekoff(off_type off,
                                            std::ios_base::seekdir dir,
                                            std::ios_base::openmode which) {
  if (dir == std::ios_base::beg) {
    m_current = m_begin + off;
  } else if (dir == std::ios_base::cur) {
    m_current += off;
  } else if (dir == std::ios_base::end) {
    m_current = (m_begin + m_size) + off;
  }

  if (m_current > m_begin + m_size || m_current < m_begin) {
    return -1;
  }

  return m_current - m_begin;
}

std::streambuf::pos_type membuffer::seekpos(pos_type pos,
                                            std::ios_base::openmode which) {
  if (pos > m_size) {
    return -1;
  }
  m_current = m_begin + static_cast<off_type>(pos);
  return m_current - m_begin;
}

std::streambuf::int_type membuffer::underflow() {
  if (m_current == m_begin + m_size) {
    return traits_type::eof();
  }

  return *m_current;
}

std::streambuf::int_type membuffer::uflow() {
  if (m_current == m_begin + m_size) {
    return traits_type::eof();
  }

  return *m_current++;
}

std::streamsize membuffer::showmanyc() {
  return (m_begin + m_size) - m_current;
}