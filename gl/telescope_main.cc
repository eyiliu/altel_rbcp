#include <chrono>
#include <thread>
#include <iostream>

#include "Telescope.hh"

#include "linenoise.h"

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

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <chrono>
#include <thread>

#include <signal.h>

#include "Telescope.hh"
#include "TelescopeGL.hh"
#include <SFML/Window.hpp>
#include "myrapidjson.h"


void fw_threshold(FirmwarePortal *m_fw, uint32_t thrshold){
  m_fw->SetAlpideRegister("ITHR", thrshold); //empty 0x32; 0x12 data, not full.
}


uint64_t async_gl_loop(Telescope *ptel, bool *pflag_gl_running);


static sig_atomic_t g_done = 0;
int main(int argc, char **argv){
  // signal(SIGINT, [](int){g_done+=1;});
  std::future<uint64_t> fut_gl_loop;
  bool flag_gl_running = false;
  
  const std::string help_usage("\n\
Usage:\n\
-c json_file: path to json file\n\
-h : print usage information, and then quit\n\
");
  
  std::string c_opt;
  std::string w_opt;
  bool w_opt_enable = false;
  int c;
  while ( (c = getopt(argc, argv, "c:w:h")) != -1) {
    switch (c) {
    case 'c':
      c_opt = optarg;
      break;
    case 'w':
      w_opt_enable = true;
      w_opt = optarg;
      break;
    case 'h':
      fprintf(stdout, "%s", help_usage.c_str());
      return 0;
      break;
    default:
      fprintf(stderr, "%s", help_usage.c_str());
      return 1;
    }
  }
  
  if (optind < argc) {
    fprintf(stderr, "\ninvalid options: ");
    while (optind < argc)
      fprintf(stderr, "%s\n", argv[optind++]);;
    return 1;
  }

  ////////////////////////
  //test if all opts
  if(c_opt.empty() ){
    fprintf(stderr, "\ninsufficient options.\n%s\n\n\n",help_usage.c_str());
    return 1;
  }
  ///////////////////////
  
  std::string json_file_path = c_opt;

  ///////////////////////
  std::string file_context = FirmwarePortal::LoadFileToString(json_file_path);
  
  Telescope tel(file_context);
  
  const char* linenoise_history_path = "/tmp/.alpide_cmd_history";
  linenoiseHistoryLoad(linenoise_history_path);
  linenoiseSetCompletionCallback([](const char* prefix, linenoiseCompletions* lc)
                                 {
                                   static const char* examples[] =
                                     {"help", "start", "stop", "init", "threshold", "window", "info",
                                      "connect","quit", "exit", "sensor", "firmware", "set", "get", "region",
                                      "broadcast", "pixel", "true", "reset","false", "ITHR", "mask", "command"
                                      "x", "y",
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
        if(fut_gl_loop.valid()){
          fut_gl_loop.get();
        }
      }
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
      tel.Start();
      // if(!flag_gl_running){
      //   fut_gl_loop = std::async(std::launch::async, &async_gl_loop, &tel, &flag_gl_running);
      // }
    }
    else if (std::regex_match(result, std::regex("\\s*(stop)\\s*"))){
      printf("stop \n");
      tel.Stop();
      flag_gl_running = false;
      if(fut_gl_loop.valid()){
        fut_gl_loop.get();
      }
    }
    else if (std::regex_match(result, std::regex("\\s*(init)\\s*"))){
      printf("init \n");
      tel.Init();
    }
    else if (std::regex_match(result, std::regex("\\s*(reset)\\s*"))){
      printf("reset \n");
      for(auto& l: tel.m_vec_layer){
        l->m_fw->SendFirmwareCommand("RESET");
      }
      
    }

    else if ( std::regex_match(result, std::regex("\\s*(sensor)\\s+(set)\\s+(\\w+)\\s+(?:(0[Xx])?([0-9]+))\\s*")) ){
      std::cmatch mt;
      std::regex_match(result, mt, std::regex("\\s*(sensor)\\s+(set)\\s+(\\w+)\\s+(?:(0[Xx])?([0-9]+))\\s*"));
      std::string name = mt[3].str();
      uint64_t value = std::stoull(mt[5].str(), 0, mt[4].str().empty()?10:16);
      for(auto& l: tel.m_vec_layer){
        auto &fw = l->m_fw;
        fw->SetAlpideRegister(name, value);
      }
    }
    else if ( std::regex_match(result, std::regex("\\s*(region)\\s+(?:(0[Xx])?([0-9]+))\\s+(set)\\s+(\\w+)\\s+(?:(0[Xx])?([0-9]+))\\s*")) ){
      std::cmatch mt;
      std::regex_match(result, mt, std::regex("\\s*(region)\\s+(?:(0[Xx])?([0-9]+))\\s+(set)\\s+(\\w+)\\s+(?:(0[Xx])?([0-9]+))\\s*"));
      
      uint64_t region = std::stoull(mt[3].str(), 0, mt[2].str().empty()?10:16);      
      std::string name = mt[5].str();
      uint64_t value = std::stoull(mt[7].str(), 0, mt[6].str().empty()?10:16);
      
      for(auto& l: tel.m_vec_layer){
        auto &fw = l->m_fw;
        fw->SetRegionRegister(region, name, value);
      }
    }
    else if ( std::regex_match(result, std::regex("\\s*(region)\\s+(set)\\s+(\\w+)\\s+(?:(0[Xx])?([0-9]+))\\s*")) ){
      std::cmatch mt;
      std::regex_match(result, mt, std::regex("\\s*(region)\\s+(set)\\s+(\\w+)\\s+(?:(0[Xx])?([0-9]+))\\s*"));
      
      std::string name = mt[3].str();
      uint64_t value = std::stoull(mt[5].str(), 0, mt[4].str().empty()?10:16);
      
      for(auto& l: tel.m_vec_layer){
        auto &fw = l->m_fw;
        fw->BroadcastRegionRegister(name, value);
      }
    }
    else if ( std::regex_match(result, std::regex("\\s*(region)\\s+(?:(0[Xx])?([0-9]+))\\s+(get)\\s+(\\w+)\\s*")) ){
      std::cmatch mt;
      std::regex_match(result, mt, std::regex("\\s*(region)\\s+(?:(0[Xx])?([0-9]+))\\s+(get)\\s+(\\w+)\\s*"));
      
      uint64_t region = std::stoull(mt[3].str(), 0, mt[2].str().empty()?10:16);      
      std::string name = mt[5].str();
      for(auto& l: tel.m_vec_layer){
        auto &fw = l->m_fw;
        uint64_t value = fw->GetRegionRegister(region, name);
        fprintf(stderr, "%s @r%u  = %u, %#x\n", name.c_str(), region, value, value);
      }
    }
    else if ( std::regex_match(result, std::regex("\\s*(sensor)\\s+(get)\\s+(\\w+)\\s*")) ){
      std::cmatch mt;
      std::regex_match(result, mt, std::regex("\\s*(sensor)\\s+(get)\\s+(\\w+)\\s*"));
      std::string name = mt[3].str();
      for(auto& l: tel.m_vec_layer){
        auto &fw = l->m_fw;
        uint64_t value = fw->GetAlpideRegister(name);
        fprintf(stderr, "%s = %u, %#x\n", name.c_str(), value, value);
      }
    }    
    else if ( std::regex_match(result, std::regex("\\s*(firmware)\\s+(set)\\s+(\\w+)\\s+(?:(0[Xx])?([0-9]+))\\s*")) ){
      std::cmatch mt;
      std::regex_match(result, mt, std::regex("\\s*(firmware)\\s+(set)\\s+(\\w+)\\s+(?:(0[Xx])?([0-9]+))\\s*"));
      std::string name = mt[3].str();
      uint64_t value = std::stoull(mt[5].str(), 0, mt[4].str().empty()?10:16);
      for(auto& l: tel.m_vec_layer){
        auto &fw = l->m_fw;
        fw->SetFirmwareRegister(name, value);
      }
    }
    else if ( std::regex_match(result, std::regex("\\s*(firmware)\\s+(get)\\s+(\\w+)\\s*")) ){
      std::cmatch mt;
      std::regex_match(result, mt, std::regex("\\s*(firmware)\\s+(get)\\s+(\\w+)\\s*"));
      std::string name = mt[3].str();
      for(auto& l: tel.m_vec_layer){
        auto &fw = l->m_fw;
        uint64_t value = fw->GetFirmwareRegister(name);
        fprintf(stderr, "%s = %u, %#x\n", name.c_str(), value, value);
      }
    }
    else if ( std::regex_match(result, std::regex("\\s*(firmware)\\s+(command)\\s+(\\w+)\\s*")) ){
      std::cmatch mt;
      std::regex_match(result, mt, std::regex("\\s*(firmware)\\s+(command)\\s+(\\w+)\\s*"));
      std::string name = mt[3].str();
      for(auto& l: tel.m_vec_layer){
        auto &fw = l->m_fw;
        fw->SendFirmwareCommand(name);
      }
    }

    else if ( std::regex_match(result, std::regex("\\s*(pixel)\\s+(mask)\\s+(?:(0[Xx])?([0-9]+))\\s+(?:(0[Xx])?([0-9]+))\\s+(true|false)\\s*")) ){
      std::cmatch mt;
      std::regex_match(result, mt, std::regex("\\s*(pixel)\\s+(mask)\\s+(?:(0[Xx])?([0-9]+))\\s+(?:(0[Xx])?([0-9]+))\\s+(true|false)\\s*"));
      uint64_t x = std::stoull(mt[4].str(), 0, mt[3].str().empty()?10:16);
      uint64_t y = std::stoull(mt[6].str(), 0, mt[5].str().empty()?10:16);
      bool value =  (mt[7].str()=="true")?true:false;
      for(auto& l: tel.m_vec_layer){
        auto &fw = l->m_fw;
        fw->SetPixelRegister(x, y, "MASK_EN", value);
      }
    }
    else if ( std::regex_match(result, std::regex("\\s*(test)\\s+(mask)\\s+(true|false)\\s*")) ){
      std::cmatch mt;
      std::regex_match(result, mt, std::regex("\\s*(test)\\s+(mask)\\s+(true|false)\\s*"));
      bool value =  (mt[3].str()=="true")?true:false;
      std::cout<< "mask " << value<<std::endl;

      tel.m_vec_layer[2]->m_fw->SetPixelRegisterFullChip("MASK_EN", 1);
      tel.m_vec_layer[4]->m_fw->SetPixelRegisterFullChip("PULSE_EN", 1);
      
    }
    else if ( std::regex_match(result, std::regex("\\s*(test)\\s+(delay)\\s+(?:(0[Xx])?([0-9]+))\\s+(?:(0[Xx])?([0-9]+))\\s*")) ){
      std::cmatch mt;
      std::regex_match(result, mt, std::regex("\\s*(test)\\s+(delay)\\s+(?:(0[Xx])?([0-9]+))\\s+(?:(0[Xx])?([0-9]+))\\s*"));

      uint64_t s = std::stoull(mt[4].str(), 0, mt[3].str().empty()?10:16);
      uint64_t d = std::stoull(mt[6].str(), 0, mt[5].str().empty()?10:16);
      if(s>=tel.m_vec_layer.size()){
        std::fprintf(stderr, "Layer %u does not exist. Do nothing. \n", s);
        continue;
      }      
      tel.m_vec_layer[s]->m_fw->SetFirmwareRegister("TRIG_DELAY", d);
      std::fprintf(stdout, "TRIG_DELAY @l%u  = %u \n", s, d);
      
    }
    else if( std::regex_match(result, std::regex("\\s*(threshold)\\s+(?:(0[Xx])?([0-9]+))\\s*")) ){
      std::cmatch mt;
      std::regex_match(result, mt, std::regex("\\s*(threshold)\\s+(?:(0[Xx])?([0-9]+))\\s*"));
      uint16_t base = mt[2].str().empty()?10:16;
      uint64_t ithr = std::stoull(mt[3].str(), 0, base);
      for(auto& l: tel.m_vec_layer){
        auto &fw = l->m_fw;
        fw->SetAlpideRegister("ITHR", ithr);
      }
    }
    else if (!strncmp(result, "info", 5)){
      std::cout<<"layer number: "<< tel.m_vec_layer.size()<<std::endl;
      for(auto& l: tel.m_vec_layer){
        auto &fw = l->m_fw;
        uint32_t ip0 = fw->GetFirmwareRegister("IP0");
        uint32_t ip1 = fw->GetFirmwareRegister("IP1");
        uint32_t ip2 = fw->GetFirmwareRegister("IP2");
        uint32_t ip3 = fw->GetFirmwareRegister("IP3");
        std::cout<<"\n\ncurrent ip  " <<ip0<<":"<<ip1<<":"<<ip2<<":"<<ip3<<"\n\n"<<std::endl;
      }
    }
    linenoiseHistoryAdd(result);
    free(result);
  }

  linenoiseHistorySave(linenoise_history_path);
  linenoiseHistoryFree();
}


uint64_t async_gl_loop(Telescope *ptel, bool *pflag_gl_running){
  Telescope &tel = *ptel;
  bool &flag_gl_running = *pflag_gl_running;
  flag_gl_running = true;
  
  TelescopeGL telgl;
  telgl.addTelLayer(0, 0, 0,   1, 0, 0, 0.028, 0.026, 1.0, 1024, 512);
  telgl.addTelLayer(0, 0, 30,  0, 1, 0, 0.028, 0.026, 1.0, 1024, 512);
  telgl.addTelLayer(0, 0, 60,  0, 0, 1, 0.028, 0.026, 1.0, 1024, 512);
  telgl.addTelLayer(0, 0, 120, 0.5, 0.5, 0, 0.028, 0.026, 1.0, 1024, 512);
  telgl.addTelLayer(0, 0, 150, 0, 1, 1, 0.028, 0.026, 1.0, 1024, 512);
  telgl.addTelLayer(0, 0, 180, 1, 0, 1, 0.028, 0.026, 1.0, 1024, 512);
  telgl.buildProgramTel();
  telgl.buildProgramHit();

  uint64_t n_gl_frame = 0;
  bool flag_pausing = false;
  sf::Event windowEvent;
  while ( flag_gl_running ){
    while (telgl.m_window->pollEvent(windowEvent)){
      switch (windowEvent.type){
      case sf::Event::Closed:
        flag_gl_running = false;
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
    
    auto ev_col =tel.ReadEvent_Lastcopy();
    if(ev_col.empty())
      continue;
        
    for(auto& e: ev_col){
      auto it_x = e->Data_X().begin();
      auto it_y = e->Data_Y().begin();
      auto it_z = e->Data_D().begin();
      auto it_x_end = e->Data_X().end();
      while(it_x!=it_x_end){
        telgl.addHit(*it_x, *it_y, *it_z);
        it_x ++;
        it_y ++;
        it_z ++;
      }
    }
    
    telgl.clearFrame();
    telgl.drawTel();
    telgl.drawHit();
    telgl.flushFrame();
    n_gl_frame ++;
  }

  flag_gl_running = false;

  return n_gl_frame;
}
