// g++ -I../../common/rapidjson -I../../common efficency.cxx  `root-config --cflags --glibs`
// ./a.out tests.json 1000
// eog efficency_xxxx.png

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
#include <TEfficiency.h>

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
  
  
  std::set<uint64_t> index_layer{0, 1, 2, 3, 4, 5, 6}; // put the layer number 
  uint64_t max_index  = *max_element(index_layer.begin(), index_layer.end());  
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

  std::map<uint64_t ,  std::pair<std::shared_ptr<TEfficiency>, std::set<uint64_t>>> eff_col;
  for(auto &i: index_layer){
    auto ref_layers = index_layer;
    ref_layers.erase(i);

    std::string str_ref;
    for(auto &r: ref_layers){
      str_ref = str_ref + "_" + std::to_string(r);
    }
    
    std::string eff_name = FormatString("eff_XY_%u_ref_to%s_h", i, str_ref.c_str());
    std::string eff_title = FormatString("effXY layer%u  ref_to%s ; layer_%u  X mm;layer_%u  Y mm", i, str_ref.c_str(), i, i);      
    auto eff_th2 = std::make_shared<TEfficiency>(eff_name.c_str(), eff_title.c_str(),100,0,30,50,0,15);
    auto pair_th2_ref = std::make_pair<std::shared_ptr<TEfficiency>, std::set<uint64_t> >(std::move(eff_th2), std::move(ref_layers));
    eff_col[i]=std::move(pair_th2_ref);
    
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
    uint64_t hit_n = 0;
    for(const auto& layer : evpack["layers"].GetArray()){
      uint64_t l_hit_n = layer["hit"].GetArray().Size();
      if(l_hit_n > 1){
        is_perfect_datapack = false;
        break;
      }
      hit_n += l_hit_n;
    }
    if( hit_n< index_layer.size()-1){
      is_perfect_datapack = false;
    }
    
    if(!is_perfect_datapack){
      not_perfect_datapack_n ++;
      continue;
    }
    perfect_datapack_n ++;


    for(auto &eff_item: eff_col){
      auto &index = eff_item.first;
      auto &th2 = eff_item.second.first;
      auto &refs = eff_item.second.second;
      
      double refX = 0;
      double refY = 0;
      bool is_good_ref_datapack_for_this_layer = true; 
      for(auto &r : refs){
        if(!frames[r]["hit"].Size()){
          is_good_ref_datapack_for_this_layer = false;
          break;
        }
        refX += frames[r]["hit"][0]["pos"][0].GetDouble();
        refY += frames[r]["hit"][0]["pos"][1].GetDouble();
      }
      if(!is_good_ref_datapack_for_this_layer){
        continue;
      }
      
      refX /= refs.size();
      refY /= refs.size();

      //search if it exists
      bool found_candidate = false;
      if(frames[index]["hit"].Size()){
        double x = frames[index]["hit"][0]["pos"][0].GetDouble();
        double y = frames[index]["hit"][0]["pos"][1].GetDouble();
        if(  (x-refX)*(x-refX) + (y-refY)*(y-refY) < 18 ){
          found_candidate = true;
        }
      }
      
      if(found_candidate){
        th2->Fill(1, refX,refY);
      }
      else{
        th2->Fill(0, refX,refY);
      }
    }
    
    
    if(processed_count>=wanted_count){
      std::fprintf(stdout, "this file has more than %lu datapack\n", wanted_count);
      break;
    }
  }
  
  std::fprintf(stdout, "processed datapack %lu \n", processed_count);
  std::fprintf(stdout, "analyzed perfect datapack %lu \n", perfect_datapack_n);

  TCanvas cv("c", "c", 696, 472 * index_layer.size());
  cv.Divide(1,index_layer.size());
  
  for(auto &eff_item: eff_col){
    auto &index = eff_item.first;
    auto &th2 = eff_item.second.first;
    uint64_t pad_y = index_map[index];    
    cv.cd(1+ pad_y);
    th2->Draw("COLZ");
  }

  
  TImage *img = TImage::Create();
  img->FromPad(&cv);

  std::string str_index;
  for(auto &i: index_layer){
    str_index = str_index + "_" + std::to_string(i);
  }
  
  std::string image_name = FormatString("effciency%s.png", str_index.c_str());
  std::fprintf(stdout, "save canvas image into %s\n", image_name.c_str());  
  img->WriteImage(image_name.c_str());
  delete img;
    
  
  return 0;
}
