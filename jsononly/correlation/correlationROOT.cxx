// g++ -I../../common/rapidjson -I../../common correlationROOT.cxx  `root-config --cflags --glibs`
// ./a.out tests.json 1000
// eog correlationROOT_xxxx.png

#include "mysystem.hh"
#include "myrapidjson.h"

#include <vector>
#include <map>
#include <iostream>


#include <TROOT.h>
#include <TGraph.h>
#include <TH2.h>
#include <TImage.h>
#include <TCanvas.h>

template<typename T>
  static void PrintJson(const T& o){
    rapidjson::StringBuffer sb;
    // rapidjson::PrettyWriter<rapidjson::StringBuffer> w(sb);
    rapidjson::Writer<rapidjson::StringBuffer> w(sb);
    o.Accept(w);
    rapidjson::PutN(sb, '\n', 1);
    std::fwrite(sb.GetString(), 1, sb.GetSize(), stdout);
    
}


template<typename ... Args>
static std::string FormatString( const std::string& format, Args ... args ){
  std::size_t size = snprintf( nullptr, 0, format.c_str(), args ... ) + 1;
  std::unique_ptr<char[]> buf( new char[ size ] ); 
  std::snprintf( buf.get(), size, format.c_str(), args ... );
  return std::string( buf.get(), buf.get() + size - 1 );
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
  if(ev_it == ev_it_end){
    std::fprintf(stderr, "empty array\n");
    throw;
  }
  
  
  std::map<std::pair<uint64_t, uint64_t>,  std::shared_ptr<TH2D> > corrX_col;
  std::map<std::pair<uint64_t, uint64_t>,  std::shared_ptr<TH2D> > corrY_col;
  
  std::set<uint64_t> index_layer{0, 1, 2, 3, 4, 5, 6}; // put the layer number 
  std::map<uint64_t, uint64_t> index_map;
  for(auto &i: index_layer){
    uint64_t pos_in_set = 0;
    for(auto it = index_layer.begin(); it!=index_layer.end(); it++){
      if(*it == i){
        index_map[i]=pos_in_set;
      }
      pos_in_set++;
    }
  }
  
  uint64_t index_layer_size  = index_layer.size();
  uint64_t max_index  = *max_element(index_layer.begin(), index_layer.end());  
  
  for(auto &left: index_layer){
    for(auto &right: index_layer){
      std::string corrX_name = FormatString("corr_%c%u%u_h", 'X', left, right);
      std::string corrX_title = FormatString("corr_%c%u%u; layer_%u_%c mm;layer_%u_%c mm", 'X', left, right, left, 'X', right, 'X');
      corrX_col[{left, right}].reset(new TH2D(corrX_name.c_str(),corrX_title.c_str(),1000,0,30,1000,0,30));


      std::string corrY_name = FormatString("corr_%c%u%u_h", 'Y', left, right);
      std::string corrY_title = FormatString("corr_%c%u%u; layer_%u_%c mm;layer_%u_%c mm", 'Y', left, right, left, 'Y', right, 'Y');      
      corrY_col[{left, right}].reset(new TH2D(corrY_name.c_str(),corrY_title.c_str(),1000,0,30,1000,0,30));
    }
  }
    
  uint64_t processed_count=0;         
  uint64_t perfect_datapack_n = 0;
  uint64_t not_perfect_datapack_n = 0;
  while(ev_it != ev_it_end){
    const auto &evpack = *ev_it;
    ev_it++;
    processed_count ++;
    
    // PrintJson(evpack);
    const auto &frames = evpack["layers"];
    if(frames.Size()< max_index){
      std::fprintf(stderr, "frame size != max_index \n");
      throw;
    }
    
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

    for(auto &corr: corrX_col){
      uint64_t left = corr.first.first;
      uint64_t right = corr.first.second;
      corr.second->Fill(frames[left]["hit"][0]["pos"][0].GetDouble(),
                         frames[right]["hit"][0]["pos"][0].GetDouble());      
    }

    for(auto &corr: corrY_col){
      uint64_t left = corr.first.first;
      uint64_t right = corr.first.second;
      corr.second->Fill(frames[left]["hit"][0]["pos"][1].GetDouble(),
                         frames[right]["hit"][0]["pos"][1].GetDouble());      
    }
    
    if(processed_count>=wanted_count){
      std::fprintf(stdout, "this file has more than %lu datapack\n", wanted_count);
      break;
    }
  }
  
  std::fprintf(stdout, "processed datapack %lu \n", processed_count);
  std::fprintf(stdout, "analyzed perfect datapack %lu \n", perfect_datapack_n);

  TCanvas cv("c", "c", 696* index_layer.size(), 472 * index_layer.size());
  cv.Divide(index_layer.size(),index_layer.size());
  std::cout<< "divide "<< index_layer.size() << index_layer.size()<< std::endl;
  
  for(auto &corr: corrX_col){
      uint64_t left = corr.first.first;
      uint64_t right = corr.first.second;
      uint64_t pad_x = index_map[left];
      uint64_t pad_y = index_map[right];
      cv.cd(1+ pad_x+ index_layer.size() * pad_y);
      if(pad_x > pad_y)
        corr.second->Draw();
  }

  for(auto &corr: corrY_col){
      uint64_t left = corr.first.first;
      uint64_t right = corr.first.second;
      uint64_t pad_x = index_map[left];
      uint64_t pad_y = index_map[right];
      cv.cd(1+ pad_x+ index_layer.size() * pad_y);
      if(pad_x < pad_y)
        corr.second->Draw();
  }

  
  TImage *img = TImage::Create();
  img->FromPad(&cv);

  std::string str_index;
  for(auto &i: index_layer){
    str_index = str_index + "_" + std::to_string(i);
  }
  
  std::string image_name = FormatString("correlationROOT%s.png", str_index.c_str());
  std::fprintf(stdout, "save canvas image into %s\n", image_name.c_str());  
  img->WriteImage(image_name.c_str());
  delete img;
  
  return 0;
}
