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
      case sf::Event::KeyPressed:
        if(ev_it == ev_it_end){
          std::cout<< "end of events"<<std::endl;
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
          break;
        }
        telgl.clearHit();
        for (auto& subev : ev_it->GetArray()){
          for (auto& hit : subev["hit_xyz_array"].GetArray()){
            telgl.addHit(hit[0].GetInt(), hit[1].GetInt(), hit[2].GetInt());
          }
        }
        std::cout<< "flush new frame"<<std::endl;
        telgl.clearFrame();
        telgl.drawTel();
        telgl.drawHit();
        telgl.flushFrame();
        ++ev_it;
        break;
        
        // we don't process other types of events
      default:
        break;
      }  
    }
  }
}
