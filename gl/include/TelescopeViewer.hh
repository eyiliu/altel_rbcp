#ifndef _ALTEL_TELESCOPE_VIEWER_HH_
#define _ALTEL_TELESCOPE_VIEWER_HH_

#include <cstdio>
#include <csignal>

#include <string>
#include <fstream>
#include <sstream>
#include <memory>
#include <chrono>
#include <thread>
#include <regex>

#include <iostream>

#include <unistd.h>

#include "JadeDataFrame.hh"

namespace altel{
  
  class TelescopeViewer{
  public:
    std::vector<DataPackSP> m_vec_ring_ev;
    DataPackSP m_ring_end; // ring end is nullptr,therefore real data as nullptr should not go into the ring.  

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
  
    void pushBufferEvent(DataPackSP dp){ //by write thread
      uint64_t next_p_ring_write = m_count_ring_write % m_size_ring;
      if(next_p_ring_write == m_hot_p_read){
        std::fprintf(stderr, "buffer full, unable to write into buffer, monitor data lose\n");
        return;
      }
      m_vec_ring_ev[next_p_ring_write] = dp;
      m_count_ring_write ++;
    }
  
    DataPackSP& frontBufferEvent(){ //by read thread
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
      telgl.addTelLayer(0, 0, 400, 0.5, 0, 0.5,  0.028, 0.026, 1.0, 1024, 512);
      telgl.buildProgramTel();
      telgl.buildProgramHit();
      telgl.clearFrame();
      telgl.drawTel();
      telgl.drawHit();
      telgl.flushFrame();
    
      uint64_t n_gl_frame = 0;
      bool flag_pausing = false;
      sf::Event windowEvent;
      while ( flag_running ){ 
        while (telgl.m_window->pollEvent(windowEvent)){
          switch (windowEvent.type){
          case sf::Event::Closed:{
            flag_running = false;
            break;
          }
          case sf::Event::KeyPressed:{
            // flag_pausing = true;
            break;
          }
          case sf::Event::KeyReleased:
            flag_pausing = false;
            break;
          }
        }
        if(flag_pausing){
          continue;
        }

        // continue; // debugging
        auto &dpev_ref = frontBufferEvent(); //ref only,  no copy, no move
        if(dpev_ref == m_ring_end){// not nullptr/ring_end
          continue;
        }
        telgl.clearHit();
        auto data_pack = std::move(dpev_ref); //moved
        popFrontBufferEvent();
        std::cout<< "pack size"<< data_pack->m_frames.size()<<std::endl;
        for(auto &data_frame: data_pack->m_frames){ 
          for(auto &chit : data_frame->m_clusters){
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

}

#endif
