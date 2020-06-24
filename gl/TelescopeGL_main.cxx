#include <cstdio>
#include <csignal>

#include <string>
#include <fstream>
#include <sstream>
#include <memory>
#include <chrono>
#include <thread>
#include <regex>

#include <unistd.h>
#include <getopt.h>

#include "TelescopeGL.hh"
#include <SFML/Window.hpp>

#include <chrono>
#include <thread>
#include <iostream>

#include "Telescope.hh"
#include "ClusterPool.hh"

#include "linenoise.h"
#include "myrapidjson.h"

template<typename T>
static void PrintJson(const T& o){
    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> w(sb);
    o.Accept(w);
    rapidjson::PutN(sb, '\n', 1);
    std::fwrite(sb.GetString(), 1, sb.GetSize(), stdout);
}

  
static const std::string help_usage = R"(
Usage:
  -help              help message
  -verbose           verbose flag
  -file [jsonfile]   path to data json file
)";


int main(int argc, char **argv){
  int do_help = false;
  int do_verbose = false;
  struct option longopts[] =
    {
     { "help",       no_argument,       &do_help,      1  },
     { "verbose",    no_argument,       &do_verbose,   1  },
     { "file",     required_argument, NULL,           'f' },
     { 0, 0, 0, 0 }};
  
  std::string datafile_name;

  int c;
  opterr = 1;
  while ((c = getopt_long_only(argc, argv, "", longopts, NULL))!= -1) {
    switch (c) {
    case 'h':
      do_help = 1;
      break;
    case 'f':
      datafile_name = optarg;
      break;
    /////generic part below///////////
    case 0: /* getopt_long() set a variable, just keep going */
      break;
    case 1:
      fprintf(stderr,"case 1\n");
      exit(1);
      break;
    case ':':
      fprintf(stderr,"case :\n");
      exit(1);
      break;
    case '?':
      fprintf(stderr,"case ?\n");
      exit(1);
      break;
    default:
      fprintf(stderr, "case default, missing branch in switch-case\n");
      exit(1);
      break;
    }
  }

  if(do_help){
    std::cout<<help_usage<<std::endl;
    exit(0);
  }

  std::FILE* fp = std::fopen(datafile_name.c_str(), "r");
  if(!fp) {
    std::perror("File opening failed");
  }

  char readBuffer[65536];
  rapidjson::FileReadStream is(fp, readBuffer, sizeof(readBuffer));
  rapidjson::Document data;
  data.ParseStream(is);
  if(!data.IsArray()){
    std::cout<< "no, it is not data array"<<std::endl;
    exit(2);
  }
  
  TelescopeGL telgl;
  telgl.addTelLayer(0, 0, 0,   1, 0, 0, 0.028, 0.026, 1.0, 1024, 512);
  telgl.addTelLayer(0, 0, 30,  0, 1, 0, 0.028, 0.026, 1.0, 1024, 512);
  telgl.addTelLayer(0, 0, 60,  0, 0, 1, 0.028, 0.026, 1.0, 1024, 512);
  telgl.addTelLayer(0, 0, 120, 0.5, 0.5, 0, 0.028, 0.026, 1.0, 1024, 512);
  telgl.addTelLayer(0, 0, 150, 0, 0.5, 0.5, 0.028, 0.026, 1.0, 1024, 512);
  telgl.addTelLayer(0, 0, 180, 1, 0, 1, 0.028, 0.026, 1.0, 1024, 512);
  telgl.addTelLayer(0, 0, 250, 0.5, 0, 0.5, 0.028, 0.026, 1.0, 1024, 512);
  telgl.buildProgramTel();
  telgl.buildProgramHit();

  rapidjson::Value::ConstValueIterator ev_it = data.Begin();
  rapidjson::Value::ConstValueIterator ev_it_end = data.End();
  
  bool running = true;
  uint64_t n_gl_frame = 0;
  sf::Event windowEvent;
  while (running)
  {
    sf::Event windowEvent;
    while (telgl.m_window->pollEvent(windowEvent))
    {
      switch (windowEvent.type)
      {
      case sf::Event::Closed:
        running = false;
        break;
        
        // key pressed
      case sf::Event::KeyPressed:{
        if(ev_it == ev_it_end){
          std::cout<< "end of events"<<std::endl;
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
          break;
        }
        telgl.clearHit();
        
        const auto &js_ev = ev_it->GetObject();
        
        const auto &js_layers = js_ev["layers"].GetArray();        
        std::cout<<"\n  "<<std::endl;
        for(const auto& js_layer : js_layers){
          JadeDataFrame df(js_layer);
          PrintJson(js_layer);
          std::printf("recreated native cluster: ");
          for(auto &c : df.m_clusters){
            telgl.addHit( c.x()/0.02924+0.1, c.y()/0.02688+0.1, c.z() );
            std::printf(" [%f, %f, %u] ", c.x(), c.y(), c.z());
          }
          std::printf(" \n");
        }
        
        telgl.clearFrame();
        telgl.drawTel();
        telgl.drawHit();
        telgl.flushFrame();
        ++ev_it;
        break;
      }
        // we don't process other types of events
      default:
        break;
      }  
    }
  }
}




class TelescopeViewer{

public:
  std::vector<JadeDataFrameSP> m_vec_ring_ev;
  JadeDataFrameSP m_ring_end; // ring end is nullptr,therefore real data as nullptr should not go into the ring.  

  uint64_t m_size_ring{200000};
  std::atomic_uint64_t m_count_ring_write;
  std::atomic_uint64_t m_count_ring_read;
  std::atomic_uint64_t m_hot_p_read;
  bool m_is_async_thread_running{false};  
  std::future<uint64_t> m_fut_async_thread;
  
  virtual ~TelescopeViewer(){
    stopAsyncThread();
  }
  
  void clearBufferEvent(){ // by write thread
    uint64_t count_ring_read = m_count_ring_read;
    m_count_ring_write = count_ring_read;
  }
  
  void pushBufferEvent(JadeDataFrameSP df){ //by write thread
    uint64_t next_p_ring_write = m_count_ring_write % m_size_ring;
    if(next_p_ring_write == m_hot_p_read){
      std::fprintf(stderr, "buffer full, unable to write into buffer, monitor data lose\n");
      return;
    }
    m_vec_ring_ev[next_p_ring_write] = df;
    m_count_ring_write ++;
  }
  
  JadeDataFrameSP& frontBufferEvent(){ //by read thread
    if(m_count_ring_write > m_count_ring_read) {
      uint64_t next_p_ring_read = m_count_ring_read % m_size_ring;
      m_hot_p_read = next_p_ring_read;
      // keep hot read to prevent write-overlapping
      return m_vec_ring_ev[next_p_ring_read];
    }
    else{
      return m_ring_end;
    }
  }

  void popFrontBufferEvent(){ //by read thread
    if(m_count_ring_write > m_count_ring_read) {
      uint64_t next_p_ring_read = m_count_ring_read % m_size_ring;
      m_hot_p_read = next_p_ring_read;
      // keep hot read to prevent write-overlapping
      m_vec_ring_ev[next_p_ring_read].reset();
      m_count_ring_read ++;
    }
  }

  void stopAsyncThread(){
    m_is_async_thread_running = false;
    if(m_fut_async_thread.valid())
      m_fut_async_thread.get();
  }

  void startAsyncThread(){
    if(m_is_async_thread_running){
      std::fprintf(stderr, "unable to start up new async thread.  already running\n");
    }
    m_vec_ring_ev.clear();
    m_vec_ring_ev.resize(m_size_ring);
    m_count_ring_write = 0;
    m_count_ring_read = 0;
    m_hot_p_read = m_size_ring -1; // tail
    
    m_fut_async_thread = std::async(std::launch::async, &TelescopeViewer::async_gl_loop, this);
  }
  
  uint64_t async_gl_loop(){
    bool &flag_running = m_is_async_thread_running;
    flag_running = true;

    TelescopeGL telgl;
    telgl.addTelLayer(0, 0, 0,   1, 0, 0,      0.028, 0.026, 1.0, 1024, 512);
    telgl.addTelLayer(0, 0, 30,  0, 1, 0,      0.028, 0.026, 1.0, 1024, 512);
    telgl.addTelLayer(0, 0, 60,  0, 0, 1,      0.028, 0.026, 1.0, 1024, 512);
    telgl.addTelLayer(0, 0, 120, 0.5, 0.5, 0,  0.028, 0.026, 1.0, 1024, 512);
    telgl.addTelLayer(0, 0, 150, 0, 1, 1,      0.028, 0.026, 1.0, 1024, 512);
    telgl.addTelLayer(0, 0, 180, 1, 0, 1,      0.028, 0.026, 1.0, 1024, 512);
    telgl.addTelLayer(0, 0, 240, 0.5, 0, 0.5,  0.028, 0.026, 1.0, 1024, 512);
    telgl.buildProgramTel();
    telgl.buildProgramHit();
    
    uint64_t n_gl_frame = 0;
    bool flag_pausing = false;
    sf::Event windowEvent;
    while ( flag_running ){ 
      while (telgl.m_window->pollEvent(windowEvent)){
        switch (windowEvent.type){
        case sf::Event::Closed:
          flag_running = false;
          break;
        case sf::Event::KeyPressed:
          flag_pausing = true;
          break;
        case sf::Event::KeyReleased:
          flag_pausing = false;
          break;
        }
      }
      if(flag_pausing){
        continue;
      }
    
      telgl.clearHit();
      
      auto &dfev_ref = frontBufferEvent(); //ref only,  no copy, no move
      if(dfev_ref){// not nullptr/ring_end
        auto dfev = std::move(dfev_ref); //moved
        popFrontBufferEvent();
        for(auto &chit : dfev->m_clusters){
          //only get first pixel hit from each cluster hit 
          telgl.addHit(chit.pixelHits[0].x(),
                       chit.pixelHits[0].y(),
                       chit.pixelHits[0].z());
        }
      } 
      telgl.clearFrame();
      telgl.drawTel();
      telgl.drawHit();
      telgl.flushFrame();
      n_gl_frame ++;
    }
    flag_running = false;
    return n_gl_frame;
  }
};
