#include <cstdio>
#include <cctype>
#include <cstdlib>
#include <cstring>

#include <iostream>
#include <fstream>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "myrapidjson.h"
#include "AltelReader.hh"

#ifdef _WIN32
#pragma comment(lib, "Ws2_32.lib")
#endif

#define HEADER_BYTE  (0x5a)
#define FOOTER_BYTE  (0xa5)

AltelReader::~AltelReader(){
  Close();
}


AltelReader::AltelReader(const std::string& json_str)
{
  rapidjson::Document js_doc;
  js_doc.Parse(json_str.c_str());
  if(js_doc.HasParseError()){
    fprintf(stderr, "JSON parse error: %s (at string positon %u)",
            rapidjson::GetParseError_En(js_doc.GetParseError()), js_doc.GetErrorOffset());
    throw;
  }

  m_fd = 0;
  m_flag_file = false;
  auto& js_proto = js_doc["protocol"];
  auto& js_opt = js_doc["options"];
  if(js_proto == "tcp"){
    m_tcp_ip = js_opt["ip"].GetString();
    m_tcp_port = js_opt["port"].GetUint();
  }
  else if(js_proto == "file"){
    m_file_path = js_opt["path"].GetString();
    m_file_terminate_eof = js_opt["terminate_eof"].GetBool();
    m_flag_file = true;
  }
  else{
    fprintf(stderr, "Unknown reader protocol: %s", js_proto.GetString());
    throw;
  }  
}

void AltelReader::Open(){
  if(m_flag_file){
    // std::string time_str= JadeUtils::GetNowStr("%y%m%d%H%M%S");
#ifdef _WIN32
    m_fd = _open(m_file_path.c_str(), _O_RDONLY | _O_BINARY);
#else
    m_fd = open(m_file_path.c_str(), O_RDONLY);
#endif
  }
  else{
    m_fd = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in tcpaddr;
    tcpaddr.sin_family = AF_INET;
    tcpaddr.sin_port = htons(m_tcp_port);
    tcpaddr.sin_addr.s_addr = inet_addr(m_tcp_ip.c_str());
    if(connect(m_fd, (struct sockaddr *)&tcpaddr, sizeof(tcpaddr)) < 0){
      //when connect fails, go to follow lines
      if(errno != EINPROGRESS) fprintf(stderr,"TCP connection\n");
      if(errno == 29)
        fprintf(stderr,"reader open timeout\n");
      fd_set fds;
      FD_ZERO(&fds);
      FD_SET(m_fd, &fds);
      timeval tv_timeout;
      tv_timeout.tv_sec = 0;
      tv_timeout.tv_usec = 10000;
      int rc = select(m_fd+1, &fds, NULL, NULL, &tv_timeout);
      if(rc<=0)
        fprintf(stderr,"connect-select error\n");
    }
  }
}

void AltelReader::Close(){
  if(!m_fd)
    return;
  
  if(m_flag_file){
#ifdef _WIN32
    _close(m_fd);
#else
    close(m_fd);
#endif
  }
  else{
    close(m_fd);
  }
  m_fd = 0;
}



std::vector<JadeDataFrameSP> AltelReader::Read(size_t size_max_pkg,
                                               const std::chrono::milliseconds &timeout_idel,
                                               const std::chrono::milliseconds &timeout_total){
  std::chrono::system_clock::time_point tp_timeout_total = std::chrono::system_clock::now() + timeout_total;
  std::vector<JadeDataFrameSP> pkg_v;
  while(1){
    JadeDataFrameSP pkg = Read(timeout_idel);
    if(pkg){
      pkg_v.push_back(pkg);
      if(pkg_v.size()>=size_max_pkg){
        break;
      }
    }
    else{
      break; 
    }
    if(std::chrono::system_clock::now() > tp_timeout_total){
      break;
    }
  }
  return pkg_v;
}

JadeDataFrameSP AltelReader::Read(const std::chrono::milliseconds &timeout_idel){ //timeout_read_interval
  size_t size_buf_min = 8;
  size_t size_buf = size_buf_min;
  std::string buf(size_buf, 0);
  size_t size_filled = 0;
  std::chrono::system_clock::time_point tp_timeout_idel;
  bool can_time_out = false;
  int read_len_real = 0;
  while(size_filled < size_buf){
    if(m_flag_file){
#ifdef _WIN32
      read_len_real = _read(m_fd, &buf[size_filled], (unsigned int)(size_buf-size_filled));
#else
      read_len_real = read(m_fd, &buf[size_filled], size_buf-size_filled);
#endif
      if(read_len_real < 0){
        std::cerr<<"JadeRead: reading error\n";
        throw;
      }
      // std::cout<<"recv len "<<read_len_real<<std::endl;

      if(read_len_real== 0){
        if(!can_time_out){
          can_time_out = true;
          tp_timeout_idel = std::chrono::system_clock::now() + timeout_idel;
        }
        else{
          if(std::chrono::system_clock::now() > tp_timeout_idel){
            // std::cerr<<"JadeRead: reading timeout\n";
            if(size_filled == 0){
              if(m_file_terminate_eof)
                return nullptr;
              else{
                std::cerr<<"JadeRead: no data at all. ("<<m_tcp_ip <<")\n";
              }
              return nullptr;
            }
            //TODO: keep remain data, nothrow
            throw;
          }
        }
        continue;
      }
    }
    else{
      fd_set fds;
      timeval tv_timeout;
      FD_ZERO(&fds);
      FD_SET(m_fd, &fds);
      FD_SET(0, &fds);
      tv_timeout.tv_sec = 0;
      tv_timeout.tv_usec = 10;
      if( !select(m_fd+1, &fds, NULL, NULL, &tv_timeout) || !FD_ISSET(m_fd, &fds) ){
        // std::cout<<"reader select timeout"<<std::endl;
        if(!can_time_out){
          can_time_out = true;
          tp_timeout_idel = std::chrono::system_clock::now() + timeout_idel;
        }
        else{
          if(std::chrono::system_clock::now() > tp_timeout_idel){
            //std::cerr<<"JadeRead: reading timeout\n";
            if(size_filled == 0){
              std::cerr<<"JadeRead: no data at all. ("<<m_tcp_ip <<")\n";
              return nullptr;
            }
            std::cerr<<"JadeRead: remaining data\n";
            std::cerr<<"replace here!! hextring\n";
            // std::cerr<<JadeUtils::ToHexString(buf.data(), size_filled)<<"\n";
            //TODO: keep remain data, nothrow, ? try a again?
            throw;
          }
        }
        continue;
      }
      read_len_real = recv(m_fd, &buf[size_filled], (unsigned int)(size_buf-size_filled), MSG_WAITALL);
      if( read_len_real == 0) continue;
      if(read_len_real <= 0){
        std::cerr<<"JadeRead: reading error\n";
        throw;
      }
      // std::cout<<"recv len "<<read_len_real<<std::endl;
    }
    
    size_filled += read_len_real;
    can_time_out = false;
    // std::cout<<" size_buf size_buf_min  size_filled<< size_buf << " "<< size_buf_min<<" " << size_filled<<std::endl;    
    if(size_buf == size_buf_min  && size_filled >= size_buf_min){
      uint8_t header_byte =  buf.front();
      uint32_t w1 = BE32TOH(*reinterpret_cast<const uint32_t*>(buf.data()+1));
      uint8_t rsv = (w1>>20) & 0xf;
      uint32_t size_payload = (w1 & 0xfffff);
      // std::cout<<" size_payload "<< size_payload<<std::endl;      
      if(header_byte != HEADER_BYTE){
        std::cerr<<"wrong header\n";
        //TODO: skip brocken data
        throw;
      }
      size_buf = size_buf_min + size_payload;
      buf.resize(size_buf);
    }
  }
  uint8_t footer_byte =  buf.back();
  if(footer_byte != FOOTER_BYTE){
    std::cerr<<"wrong footer\n";
    std::cerr<<"\n";
    std::cerr<<"replace here!! hextring\n";
    // std::cout<<JadeUtils::ToHexString(buf)<<std::endl;
    std::cerr<<"\n";
    //TODO: skip brocken data
    throw;
  }
  
  // std::cout<<JadeUtils::ToHexString(buf)<<std::endl;
  return std::make_shared<JadeDataFrame>(std::move(buf));
}

std::string AltelReader::LoadFileToString(const std::string& path){
  std::ifstream ifs(path);
  if(!ifs.good()){
    std::cerr<<"LoadFileToString:: ERROR, unable to load file<"<<path<<">\n";
    throw;
  }
  std::string str;
  str.assign((std::istreambuf_iterator<char>(ifs) ),
             (std::istreambuf_iterator<char>()));
  return str;
}
