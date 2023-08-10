#ifndef MCARVE_CONFIG_H_
#define MCARVE_CONFIG_H_

#include <cstdint>
#include <fstream>
#include <memory>

namespace mcarve {

class Pass1Config {
  public:
    Pass1Config(int argc, char *argv[]);
    std::string file_name() { return m_filename; }
    uint32_t start_time() { return m_start_time; }
    uint32_t stop_time() { return m_stop_time; }
    bool verbose() { return m_verbose; }

  private:
    std::string m_filename;
    uint32_t m_start_time;
    uint32_t m_stop_time;
    bool m_verbose;
};

} // namespace mcarve

#endif // MCARVE_CONFIG_H_
