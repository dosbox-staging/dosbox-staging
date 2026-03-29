#include <streambuf>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weffc++"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"

class membuffer : public std::streambuf {
public:
  membuffer(char *begin, size_t size)
    : m_begin(begin), m_current(begin), m_size(size) {}

protected:
  virtual pos_type seekoff(off_type off, std::ios_base::seekdir dir,
                           std::ios_base::openmode which) override;
  virtual pos_type seekpos(pos_type pos,
                           std::ios_base::openmode which) override;
  virtual int_type underflow() override;
  virtual int_type uflow() override;
  virtual std::streamsize showmanyc() override;

private:
  char *m_begin;
  char *m_current;
  size_t m_size;
};

#pragma GCC diagnostic pop
#pragma clang diagnostic pop
