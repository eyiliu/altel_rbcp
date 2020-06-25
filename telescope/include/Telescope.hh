#ifndef _TELESCOPE_HH_
#define _TELESCOPE_HH_

#include <mutex>
#include <future>

#include <cstdio>

#include "FirmwarePortal.hh"
#include "AltelReader.hh"

#include "myrapidjson.h"

class Layer{
public:
  std::unique_ptr<FirmwarePortal> m_fw;
  std::unique_ptr<AltelReader> m_rd;
  std::future<uint64_t> m_fut_async_rd;
  std::future<uint64_t> m_fut_async_watch;
  std::vector<JadeDataFrameSP> m_vec_ring_ev;
  JadeDataFrameSP m_ring_end;
  
  uint64_t m_size_ring{200000};
  std::atomic_uint64_t m_count_ring_write;
  std::atomic_uint64_t m_hot_p_read;
  uint64_t m_count_ring_read;
  bool m_is_async_reading{false};
  bool m_is_async_watching{false};

  uint64_t m_extension{0};
  
  //status variable:
  std::atomic_uint64_t m_st_n_tg_ev_now{0};
  std::atomic_uint64_t m_st_n_ev_input_now{0};
  std::atomic_uint64_t m_st_n_ev_output_now{0};
  std::atomic_uint64_t m_st_n_ev_bad_now{0};
  std::atomic_uint64_t m_st_n_ev_overflow_now{0};
  std::atomic_uint64_t m_st_n_tg_ev_begin{0};

  uint64_t m_st_n_tg_ev_old{0};
  uint64_t m_st_n_ev_input_old{0};
  //uint64_t m_st_n_ev_output_old{0};
  uint64_t m_st_n_ev_bad_old{0};
  uint64_t m_st_n_ev_overflow_old{0};
  std::chrono::system_clock::time_point m_tp_old;
  std::chrono::system_clock::time_point m_tp_run_begin;

  std::string m_st_string;
  std::mutex m_mtx_st;
public:
  void fw_start();
  void fw_stop();
  void fw_init();

  void rd_start();
  void rd_stop();

  uint64_t AsyncPushBack();
  JadeDataFrameSP GetNextCachedEvent();
  JadeDataFrameSP& Front();
  void PopFront();
  uint64_t Size();
  void ClearBuffer();
  
  std::string GetStatusString();
  uint64_t AsyncWatchDog();

  std::string m_name;
  rapidjson::CrtAllocator m_jsa;
  rapidjson::GenericValue<rapidjson::UTF8<char>, rapidjson::CrtAllocator> m_js_conf;
};

class Telescope{
public:
  std::vector<std::unique_ptr<Layer>> m_vec_layer;
  std::future<uint64_t> m_fut_async_rd;
  std::future<uint64_t> m_fut_async_watch;
  bool m_is_async_reading{false};
  bool m_is_async_watching{false};
  bool m_is_running{false};

  std::vector<JadeDataFrameSP> m_ev_last;
  std::vector<JadeDataFrameSP> m_ev_last_empty;
  std::atomic_uint64_t m_mon_ev_read{0};
  std::atomic_uint64_t m_mon_ev_write{0};
  std::vector<JadeDataFrameSP> ReadEvent_Lastcopy();
  
  std::atomic_uint64_t m_st_n_ev{0};
  
  ~Telescope();
  Telescope(const std::string& file_context);
  std::vector<JadeDataFrameSP> ReadEvent();

  void Init();
  void Start();
  void Stop();
  void Start_no_tel_reading();
  uint64_t AsyncRead();
  uint64_t AsyncWatchDog();  

  bool m_flag_next_event_add_conf{true};

  rapidjson::CrtAllocator m_jsa;
  rapidjson::GenericValue<rapidjson::UTF8<char>, rapidjson::CrtAllocator> m_js_testbeam;
  rapidjson::GenericValue<rapidjson::UTF8<char>, rapidjson::CrtAllocator> m_js_telescope;
  
  rapidjson::GenericValue<rapidjson::UTF8<char>, rapidjson::CrtAllocator>  m_js_status;
  std::atomic_uint64_t m_count_st_js_write{0};
  std::atomic_uint64_t m_count_st_js_read{0};
};

#endif
