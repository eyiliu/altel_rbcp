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


class ClusterPool{
public:  
  void addHit(uint64_t x, uint64_t y, uint64_t z){
    uint64_t index = x + (y<<16) + (z<<32);
    // std::fprintf(stdout, "push cluster hit: %#016lx [%u, %u, %u] \n", index, x, y, z);    
    m_hit_col.push_back(index);
  }
  
  void buildClusters(){
    std::vector<uint64_t> hit_col_remain= m_hit_col;

    while(!hit_col_remain.empty()){
      std::vector<uint64_t> hit_col_this_cluster;
      std::vector<uint64_t> hit_col_this_cluster_edge;
      
      // get first edge seed hit
      // from un-identifed hit to edge hit
      hit_col_this_cluster_edge.push_back(hit_col_remain[0]);
      hit_col_remain.erase(hit_col_remain.begin());
      
      while(!hit_col_this_cluster_edge.empty()){
        uint64_t e = hit_col_this_cluster_edge[0];
        uint64_t c  = 0x00000001; //LSB column  x
        uint64_t r  = 0x00010000; //LSB row     y
        
        //  8 sorround hits search, 
        std::vector<uint64_t> sorround_col
          {e-c+r, e+r, e+c+r,
           e-c  ,      e+c,
           e-c-r, e-r, e+c-r
          };
        
        for(auto& sr: sorround_col){
          // only search on un-identifed hits
          auto sr_found_it = std::find(hit_col_remain.begin(), hit_col_remain.end(), sr);
          if(sr_found_it != hit_col_remain.end()){
            // move the found sorround hit
            // from un-identifed hit to an edge hit
            hit_col_this_cluster_edge.push_back(sr);
            hit_col_remain.erase(sr_found_it);
          }
        }

        // after sorround search  
        // move from edge hit to cluster hit
        hit_col_this_cluster.push_back(e);
        hit_col_this_cluster_edge.erase(hit_col_this_cluster_edge.begin());

        // {
        //   uint64_t x_tmp = (e & 0xffff);
        //   uint64_t y_tmp = (e & 0xffff0000) >> 16;
        //   uint64_t z_tmp = (e & 0xffff00000000) >> 32;
        //   std::fprintf(stdout, "push cluster hit: %#016lx [%u, %u, %u] into cluster %u \n", e, x_tmp, y_tmp, z_tmp, m_cluster_col.size());
        // }
  
      }

      double   cx = 0;
      double   cy = 0;
      double   cz = 0;
      for(auto &hit : hit_col_this_cluster){
        cx+= (hit & 0xffff);
        cy+= (hit & 0xffff0000) >> 16;
        cz+= (hit & 0xffff00000000) >> 32;
      }
      cx /= hit_col_this_cluster.size();
      cy /= hit_col_this_cluster.size();
      cz /= hit_col_this_cluster.size();
     
      m_ccenter_col.push_back(std::vector<double>{cx, cy, cz});
      m_cluster_col.push_back(std::move(hit_col_this_cluster));
    }
  }
  
  std::vector<uint64_t> m_hit_col;
  std::vector<std::vector<uint64_t>> m_cluster_col;
  std::vector<std::vector<double>>   m_ccenter_col;
};

  
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
        ClusterPool cpool;
        telgl.clearHit();
        for (auto& subev : ev_it->GetArray()){
          PrintJson(subev);
          for (auto& hit : subev["hit_xyz_array"].GetArray()){
            telgl.addHit(hit[0].GetInt(), hit[1].GetInt(), hit[2].GetInt());            
            cpool.addHit(hit[0].GetInt(), hit[1].GetInt(), hit[2].GetInt());
          }
        }
        std::cout<<std::endl;

        cpool.buildClusters();

        uint64_t nc = cpool.m_ccenter_col.size();
        for(uint64_t i = 0; i< nc; i++){
          auto &ccenter = cpool.m_ccenter_col[i];
          auto &cluster = cpool.m_cluster_col[i];
          std::fprintf(stdout, "\n cluster@%u: [%f, %f, %f] \n", i, ccenter[0], ccenter[1], ccenter[2]);
          for(auto& h: cluster){
            uint64_t hx = (h & 0xffff);
            uint64_t hy = (h & 0xffff0000) >> 16;
            uint64_t hz = (h & 0xffff00000000) >> 32;
            std::fprintf(stdout, "   [%u, %u, %u] ", hx, hy, hz);
          }
          std::fprintf(stdout, "\n");
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
