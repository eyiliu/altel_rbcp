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

#include "TelescopeViewer.hh"

#include "Mathematics/ApprOrthogonalLine3.h"

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

  bool flag_gl_running = false;
  altel::TelescopeViewer tel_viewer;

  
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
  

  rapidjson::Value::ConstValueIterator ev_it = data.Begin();
  rapidjson::Value::ConstValueIterator ev_it_end = data.End();  


  
  const char* linenoise_history_path = "/tmp/.alpide_viewer_history";
  linenoiseHistoryLoad(linenoise_history_path);
  linenoiseSetCompletionCallback([](const char* prefix, linenoiseCompletions* lc)
                                 {
                                   static const char* examples[] =
                                     {"help", "start", "stop", "init", "threshold", "window", "info",
                                      NULL};
                                   size_t i;
                                   for (i = 0;  examples[i] != NULL; ++i) {
                                     if (strncmp(prefix, examples[i], strlen(prefix)) == 0) {
                                       linenoiseAddCompletion(lc, examples[i]);
                                     }
                                   }
                                 } );
  
  const char* prompt = "\x1b[1;32malpide\x1b[0m> ";
  while (1) {
    char* result = linenoise(prompt);
    if (result == NULL) {
      if(linenoiseKeyType() == 1 ){ //ctrl-c
        flag_gl_running = false;
        // if(fut_gl_loop.valid()){
        //   fut_gl_loop.get();
        // }
      }
      std::cout<< " where am i"<< std::endl;
      break;
    }    
    if ( std::regex_match(result, std::regex("\\s*(quit|exit)\\s*")) ){
      printf("quiting \n");
      linenoiseHistoryAdd(result);
      free(result);
      break;
    }
    else if (std::regex_match(result, std::regex("\\s*(start)\\s*"))){
      printf("starting \n");
      tel_viewer.startAsyncThread();      
    }
    else if (std::regex_match(result, std::regex("\\s*(stop)\\s*"))){
      printf("stopping \n");
      tel_viewer.stopAsyncThread();      
    }
    else{
      if(strlen(result)){
        std::fprintf(stderr, "unknown command pattern\n");
      }
      else{
        /////////////////////////////////////////
        const auto &js_ev = ev_it->GetObject();     
        const auto &js_layers = js_ev["layers"].GetArray();        
        std::cout<<"\n  "<<std::endl;

        std::vector<gte::Vector3<double>> hits;

        DataPackSP dp(new DataPack);
        for(const auto& js_layer : js_layers){
          DataFrameSP df(new DataFrame(js_layer));
          // PrintJson(js_layer);
          std::printf("recreated native cluster: ");
          for(auto &c : df->m_clusters){
            std::printf(" [%f, %f, %u] ", c.x(), c.y(), c.z());
            double x = c.x();
            double y = c.y();
            double z;
            if(c.z()<6){
              z = c.z() * 38;
            }
            else{
              z = c.z() * 5 + 300;
            }
            hits.push_back({x, y, z});
          }
          dp->m_frames.push_back(std::move(df));
          std::printf(" \n");
        }
        
        gte::ApprOrthogonalLine3<double> linefit;  
        linefit.Fit(hits);
        gte::Line3<double> line = linefit.GetParameters();
          
        // line.origin;
        // line.direction;

        std::printf("origin: %f, %f, %f; direction:%f, %f, %f \n",
                    line.origin[0],line.origin[1],line.origin[2],
                    line.direction[0],line.direction[1],line.direction[2]
                    );

        
        tel_viewer.pushBufferEvent(dp);
        ++ev_it;

        //////////////////////////////
      }
    }

    
    linenoiseHistoryAdd(result);
    free(result);
  }
  linenoiseHistorySave(linenoise_history_path);
  linenoiseHistoryFree();
}



