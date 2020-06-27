// g++ -I../../common/rapidjson -I../../common `root-config --cflags --glibs`  cluster.cxx 
// ./a.out tests.json 1000
// eog cluster_xxxx.png

#include "mysystem.hh"
#include "myrapidjson.h"

#include <vector>
#include <map>
#include <iostream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

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

std::string GetNowStr(const std::string &format){
  // "%Y%m%d%H%M%S"
  auto now = std::chrono::system_clock::now();
  auto now_c = std::chrono::system_clock::to_time_t(now);
  std::stringstream ss;
  ss<<std::put_time(std::localtime(&now_c), format.c_str());
  return ss.str();
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

  
  fseek(fp, 0, SEEK_END);
  size_t filesize = (size_t)ftell(fp);
  fseek(fp, 0, SEEK_SET);
  char* buffer = (char*)malloc(filesize + 1);
  size_t readLength = fread(buffer, 1, filesize, fp);
  buffer[readLength] = '\0';
  fclose(fp); 
  rapidjson::Document doc;
  doc.ParseInsitu(buffer);
  std::fprintf(stdout, "parsing finished\n");
  
  /*
  char readBuffer[1000000];
  rapidjson::FileReadStream is(fp, readBuffer, sizeof(readBuffer));
  // rapidjson::GenericDocument<rapidjson::UTF8<char>, rapidjson::CrtAllocator> doc;
  rapidjson::Document doc;
  doc.ParseStream(is);
  */

  if(!doc.IsArray()){
    std::fprintf(stderr, "no, it is not data array\n");
    throw;
  }
  
  auto ev_it = doc.Begin();
  auto ev_it_end = doc.End();  
  if(ev_it == ev_it_end){
    std::fprintf(stderr, "empty array\n");
    throw;
  }
  
  auto& jsa = doc.GetAllocator();
  rapidjson::Value js_telescope;
  rapidjson::Value js_testbeam;  

  // rapidjson::GenericValue<rapidjson::UTF8<char>, rapidjson::CrtAllocator> js_telescope;
  // rapidjson::GenericValue<rapidjson::UTF8<char>, rapidjson::CrtAllocator> js_testbeam;  
  
  for(auto ev_it = doc.Begin();
      ev_it != doc.End(); ev_it++){
    
    const auto &evpack = *ev_it;
    if(js_testbeam.IsNull()){
      if(evpack.HasMember("testbeam")){
        js_testbeam.CopyFrom(evpack["testbeam"], jsa);
      }
    }
    
    if(js_telescope.IsNull()){
      if(evpack.HasMember("telescope")){
        js_telescope.CopyFrom(evpack["telescope"], jsa);
      }
    }
    if(!js_telescope.IsNull() && !js_testbeam.IsNull()){        
      break;
    }
  }
  if(js_telescope.IsNull() || js_testbeam.IsNull()){
    std::fprintf(stderr, "unable to get config from data \n");
    throw;
  }
  double energy = js_testbeam["energy"].GetDouble();
  
  
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
    
  std::map<uint64_t, std::pair<std::shared_ptr<TH1F>, std::shared_ptr<TH1F>> > clusterPixelN_col;
  for(auto &i: index_layer){
    std::string th1_clusterN_name = FormatString("clusterN_%u_h", i);
    std::string th1_clusterN_title = FormatString("clusterN per readout frame, layer%u, %.1fGeV; cluster number, per frame; normalised", i, energy);

    std::string th1_pixelN_name = FormatString("pixelN_%u_h", i);
    std::string th1_PixelN_title = FormatString("pixelN per cluster, layer%u, %.1fGeV; pixel number, per cluster; normalised", i, energy);

    clusterPixelN_col[i].first.reset(new TH1F(th1_clusterN_name.c_str(), th1_clusterN_title.c_str(), 10, -0.5,  9.5));
    clusterPixelN_col[i].second.reset(new TH1F(th1_pixelN_name.c_str(), th1_PixelN_title.c_str(), 20, 0.5,  20.5));
    
  }
  
  std::shared_ptr<TH1F> th_clusterSum;
  std::string th_clusterSum_title = FormatString("clusterSum all layers per readout, %.1fGeV; cluster number, all layers per readout; normalised", energy);
  th_clusterSum.reset(new TH1F("clusterSum_h", th_clusterSum_title.c_str(), 50, -0.5, 49.5));  

  std::shared_ptr<TH1F> th_trackN;
  std::string th_trackN_title = FormatString("trackN (clusterSum/layerSum) per readout, %.1fGeV; track number; normalised", energy);
  th_trackN.reset(new TH1F("trackN_h", th_trackN_title.c_str(), 10, -0.3, 9.7));  
  
  uint64_t processed_count=0;         
  uint64_t perfect_datapack_n = 0;
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
    }
    
    if(is_perfect_datapack){
      perfect_datapack_n ++;
      // continue;
    }


    uint64_t clusterSum = 0;
    for(auto &clusterPixelN_item: clusterPixelN_col){
      auto &i = clusterPixelN_item.first;
      auto &th_clusterN = clusterPixelN_item.second.first;
      auto &th_pixelN = clusterPixelN_item.second.second;
      
      uint64_t clusterN = frames[i]["hit"].Size();
      th_clusterN->Fill(clusterN);
      clusterSum += clusterN;
      
      for(const auto& ch: frames[i]["hit"].GetArray()){
        uint64_t pixelN = ch["pix"].Size();
        th_pixelN->Fill(pixelN);          
      }
    }

    th_clusterSum->Fill(clusterSum);
    th_trackN->Fill(clusterSum/index_layer.size());
    
    if(processed_count>=wanted_count){
      std::fprintf(stdout, "this file has more than %lu datapack\n", wanted_count);
      break;
    }
  }
  
  std::fprintf(stdout, "processed datapack %lu \n", processed_count);
  // std::fprintf(stdout, "analyzed perfect datapack %lu \n", perfect_datapack_n);

  uint64_t pad_divX = 2;
  uint64_t pad_divY = index_layer.size()+1;
  
  TCanvas cv("c", "c", 696 * pad_divX, 472 * pad_divY);
  cv.Divide(pad_divX, pad_divY);
  
  for(auto &clusterPixelN_item: clusterPixelN_col){
      auto &i = clusterPixelN_item.first;
      auto &th_clusterN = clusterPixelN_item.second.first;
      auto &th_pixelN = clusterPixelN_item.second.second;
      
      uint64_t n = index_map[i];

      cv.cd(1+ n*2);
      th_clusterN->Scale(1./th_clusterN->Integral());  
      th_clusterN->SetAxisRange(0, 1.0 ,"Y");
      th_clusterN->DrawCopy("HIST", "_");
      
      cv.cd(1+ n*2 + 1);
      th_pixelN->Scale(1./th_pixelN->Integral());      
      th_pixelN->SetAxisRange(0, 0.6,"Y");
      th_pixelN->DrawCopy("HIST", "_");
  }  

  cv.cd(1+2*index_layer.size());
  th_clusterSum->Scale(1./th_clusterSum->Integral());      
  th_clusterSum->SetAxisRange(0, 1,"Y");
  th_clusterSum->DrawCopy("HIST", "_");


  cv.cd(1+2*index_layer.size()+1);
  th_trackN->Scale(1./th_trackN->Integral());      
  th_trackN->SetAxisRange(0, 1,"Y");
  th_trackN->DrawCopy("HIST", "_");

  
  std::shared_ptr<TImage> img(TImage::Create());
  img->FromPad(&cv);
  
  std::string str_index;
  for(auto &i: index_layer){
    str_index = str_index + "_" + std::to_string(i);
  }
  
  std::string image_name = FormatString("cluster_%.1fGeV_%s.png", energy, GetNowStr("%Y%m%d-%H%M%S").c_str());
  std::fprintf(stdout, "save canvas image into %s\n", image_name.c_str());
  
  img->WriteImage(image_name.c_str());
  
  return 0;
}
