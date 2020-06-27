
#include "mysystem.hh"
#include "myrapidjson.h"

#include <map>
#include <iostream>

template<typename T>
  static void PrintJson(const T& o){
    rapidjson::StringBuffer sb;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> w(sb);
    o.Accept(w);
    std::fwrite(sb.GetString(), 1, sb.GetSize(), stdout);
}


// static sig_atomic_t g_done = 0;
int main(int argc, char **argv){
  // signal(SIGINT, [](int){g_done+=1;});

  if(argc < 2){
    std::fprintf(stderr, "wrong command options \n");
    throw;
  }
  std::string datafile_name = argv[1];

  uint64_t wanted_count=100;
  if(argc == 3 ){
    wanted_count = std::stoul(argv[2]);
  }

  std::FILE* fp = std::fopen(datafile_name.c_str(), "r");
  if(!fp) {
    std::fprintf(stderr, "File opening failed: %s \n", datafile_name.c_str());
    throw;
  }

  char readBuffer[1000000];
  rapidjson::FileReadStream is(fp, readBuffer, sizeof(readBuffer));
  rapidjson::Document doc;
  doc.ParseStream(is);
  if(!doc.IsArray()){
    std::fprintf(stderr, "no, it is not data array\n");
    throw;
  }
  rapidjson::Value::ConstValueIterator ev_it = doc.Begin();
  rapidjson::Value::ConstValueIterator ev_it_end = doc.End();  

  uint64_t processed_count=0;

  std::map<uint64_t, uint64_t> hotmap;    
  std::map<uint64_t, std::map<std::pair<uint64_t, uint64_t>, uint64_t >  > allmap;

  while(ev_it != ev_it_end){
    const auto &evpack = *ev_it;
    // PrintJson(evpack);
    // std::cout<< ">>evpack"<<std::endl;
    for(const auto& layer : evpack["layers"].GetArray()){
      // PrintJson(layer);
      // std::cout<< ">>layer"<<std::endl;
      for(const auto& cluster : layer["hit"].GetArray()){
        for(const auto& pixel : cluster["pix"].GetArray()){
          uint64_t x = pixel[0].GetUint();
          uint64_t y = pixel[1].GetUint();
          uint64_t z = pixel[2].GetUint();
          uint64_t i = x + (y<<16) + (z<<32); 
          ++(hotmap[i]);
        }
      }
    }
    ev_it++;
    processed_count ++;
    if(processed_count>wanted_count){
      std::fprintf(stdout, "this file has more than %lu datapack\n", wanted_count);
      break;
    }
  }
  std::fprintf(stdout, "processed datapack %lu \n", processed_count);

  std::fprintf(stdout, "%lu hot pixels found\n", hotmap.size());
  uint64_t noise_shot = 0;
  for(auto& h: hotmap ){
    uint64_t i = h.first;
    uint64_t c = h.second;

    uint64_t x = i & 0xffff;
    uint64_t y = (i>>16) & 0xffff;
    uint64_t z = (i>>32) & 0xffff;
    
    noise_shot += c;
    
    allmap[z][{x, y}] = c;
    std::fprintf(stdout, " [%u %u] L%u %u \n", x, y, z ,c );
  }

  
  std::fprintf(stdout, "%lu hot pixels found\n", hotmap.size());
  std::fprintf(stdout, "%lu noise events found\n",noise_shot);
  std::fprintf(stdout, "(noise events)/(hot pixels) = %f \n",double(noise_shot)/double(hotmap.size()) );
  
  return 0;
}
