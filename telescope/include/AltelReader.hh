#ifndef _ALPIDE_READER_WS_
#define _ALPIDE_READER_WS_

#include <string>
#include <vector>
#include <chrono>

#include "mysystem.hh"
#include "JadeDataFrame.hh"

class AltelReader{
public:
  ~AltelReader();
  AltelReader(const std::string &json_string);
  JadeDataFrameSP Read(const std::chrono::milliseconds &timeout);
  std::vector<JadeDataFrameSP> Read(size_t size_max_pkg,
                                    const std::chrono::milliseconds &timeout_idel,
                                    const std::chrono::milliseconds &timeout_total);
  void Open();
  void Close();
  static std::string LoadFileToString(const std::string& path);    
private:
  int m_fd{0};
  std::string m_tcp_ip;
  uint16_t m_tcp_port{24};
  std::string m_file_path;
  bool m_file_terminate_eof{false};
  bool m_flag_file{false};
};

#endif
