
#include "mysystem.hh"
#include "myrapidjson.h"

#include <vector>
#include <map>
#include <iostream>

template<typename T>
  static void PrintJson(const T& o){
    rapidjson::StringBuffer sb;
    // rapidjson::PrettyWriter<rapidjson::StringBuffer> w(sb);
    rapidjson::Writer<rapidjson::StringBuffer> w(sb);
    o.Accept(w);
    rapidjson::PutN(sb, '\n', 1);
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

         
  uint64_t layer_pairs = 6+5+4+3+2+1;   
  std::vector<   std::vector<std::pair<uint64_t, uint64_t>>  > pix_corrX(layer_pairs);
  std::vector<   std::vector<std::pair<uint64_t, uint64_t>>  > pix_corrY(layer_pairs);
  std::vector<std::pair<double, double> >  corrX01;
  std::vector<std::pair<double, double> >  corrY01;
  
  uint64_t perfect_datapack_n = 0;
  uint64_t not_perfect_datapack_n = 0;
  while(ev_it != ev_it_end){
    const auto &evpack = *ev_it;
    ev_it++;
    
    // PrintJson(evpack);
    
    bool is_perfect_datapack = true;
    for(const auto& layer : evpack["layers"].GetArray()){
      uint64_t l_hit_n = layer["hit"].GetArray().Size();
      if(l_hit_n != 1){
        is_perfect_datapack = false;
        continue;
      }
    }
    if(!is_perfect_datapack){
      not_perfect_datapack_n ++;
      continue;
    }
    perfect_datapack_n ++;

    const auto &frames = evpack["layers"];
    if(frames.Size()!=7){
      std::fprintf(stderr, "frame size != 7 \n");
      throw;
    }


    
    
    corrX01.push_back({frames[0]["hit"][0]["pos"][0].GetDouble(),
                       frames[1]["hit"][0]["pos"][0].GetDouble()});
    
    corrY01.push_back({frames[0]["hit"][0]["pos"][1].GetDouble(),
                       frames[1]["hit"][0]["pos"][1].GetDouble()});
    
    // for(const auto& layer : evpack["layers"].GetArray()){
    //   for(const auto& cluster : layer["hit"].GetArray()){
    //       double x = cluster[0].GetDouble();
    //       double y = pixel[1].GetDouble();
    //     }        
    //   }

    PrintJson(frames[0]);
    PrintJson(frames[1]);
    
    processed_count ++;
    if(processed_count>wanted_count){
      std::fprintf(stdout, "this file has more than %lu datapack\n", wanted_count);
      break;
    }
  }

  uint64_t cluster_n  = corrX01.size();
  for(uint64_t n= 0; n< cluster_n; n++){
    std::printf("x01 [%.4f, %.4f]  y01 [%.4f, %.4f] \n", corrX01[n].first, corrX01[n].second, corrY01[n].first, corrY01[n].second);
  }
  
  
  std::fprintf(stdout, "processed datapack %lu \n", processed_count);
  std::fprintf(stdout, "analizesed prefect datapack %lu \n", perfect_datapack_n);  
  
  return 0;
}

