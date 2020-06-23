#ifndef JADEPIX_JADEDATAFRAME_WS
#define JADEPIX_JADEDATAFRAME_WS

#include <string>
#include <vector>
#include <memory>

#include "mysystem.hh"
#include "myrapidjson.h"

#include "ClusterPool.hh"

class JadeDataFrame;
using JadeDataFrameSP = std::shared_ptr<JadeDataFrame>;

class JadeDataFrame {
public:
  JadeDataFrame(const std::string& raw);
  JadeDataFrame(std::string&& raw);
  JadeDataFrame(const rapidjson::Value &js);
  JadeDataFrame() = delete;  
  
  void Decode(uint32_t level);

  //const version
  const std::string& Raw() const;

  //none const version
  std::string& Raw();
  
  uint64_t GetTrigger(){return m_trigger;};
  void SetTrigger(uint64_t v){m_trigger = v;};
  
  uint32_t GetMatrixDepth() const;
  uint32_t GetMatrixSizeX() const; //x row, y column
  uint32_t GetMatrixSizeY() const;
  uint64_t GetCounter();
  uint64_t GetExtension();

  template <typename Allocator>
  rapidjson::GenericValue<rapidjson::UTF8<>, Allocator> JSON(Allocator &a) const{
    rapidjson::GenericValue<rapidjson::UTF8<>, Allocator> js;
    js.SetObject();
    js.AddMember("det", "alptel", a);
    js.AddMember("ver",  rapidjson::GenericValue<rapidjson::UTF8<>, Allocator>(s_version), a);
    js.AddMember("tri",  rapidjson::GenericValue<rapidjson::UTF8<>, Allocator>(m_trigger), a);
    js.AddMember("cnt",  rapidjson::GenericValue<rapidjson::UTF8<>, Allocator>(m_counter), a);
    js.AddMember("ext",  rapidjson::GenericValue<rapidjson::UTF8<>, Allocator>(m_extension), a);

    rapidjson::GenericValue<rapidjson::UTF8<>, Allocator> js_cluster_hits;
    js_cluster_hits.SetArray();
    for(auto &ch : m_clusters){
      rapidjson::GenericValue<rapidjson::UTF8<>, Allocator> js_cluster_hit;
      js_cluster_hit.SetObject();

      rapidjson::GenericValue<rapidjson::UTF8<>, Allocator> js_cluster_hit_pos;
      js_cluster_hit_pos.SetArray();
      js_cluster_hit_pos.PushBack(rapidjson::GenericValue<rapidjson::UTF8<>, Allocator>(ch.x()), a);
      js_cluster_hit_pos.PushBack(rapidjson::GenericValue<rapidjson::UTF8<>, Allocator>(ch.y()), a);
      js_cluster_hit_pos.PushBack(rapidjson::GenericValue<rapidjson::UTF8<>, Allocator>(ch.z()), a);
      js_cluster_hit.AddMember("pos", std::move(js_cluster_hit_pos), a);
      
      rapidjson::GenericValue<rapidjson::UTF8<>, Allocator> js_pixel_hits;
      js_pixel_hits.SetArray();
      for(auto &ph : ch.pixelHits){
        rapidjson::GenericValue<rapidjson::UTF8<>, Allocator> js_pixel_hit_pos;
        js_pixel_hit_pos.SetArray();
        js_pixel_hit_pos.PushBack(rapidjson::GenericValue<rapidjson::UTF8<>, Allocator>(ph.x()), a);
        js_pixel_hit_pos.PushBack(rapidjson::GenericValue<rapidjson::UTF8<>, Allocator>(ph.y()), a);
        js_pixel_hit_pos.PushBack(rapidjson::GenericValue<rapidjson::UTF8<>, Allocator>(ph.z()), a);
        js_pixel_hits.PushBack(std::move(js_pixel_hit_pos), a);
      }
      js_cluster_hit.AddMember("pix", std::move(js_pixel_hits), a);
      
      js_cluster_hits.PushBack(std::move(js_cluster_hit), a);
    }
    js.AddMember("hit", std::move(js_cluster_hits) , a);
    
    //https://rapidjson.org/classrapidjson_1_1_generic_object.html
    //https://rapidjson.org/md_doc_tutorial.html#CreateModifyValues
    return js;
  };
  
  
  void Print(std::ostream& os, size_t ws = 0) const;
  
  template <typename W>
  void Serialize(W& w) const {
    w.StartObject();
    {
      w.String("det");
      w.String("altel");
      
      w.String("ver");
      w.Uint(s_version);
      
      w.String("tri");
      w.Uint(m_trigger);
      
      w.String("cnt");
      w.Uint(m_counter);
      
      w.String("ext");
      w.Uint(m_extension);
      
      w.String("hit");
      w.StartArray();
      for(auto &ch : m_clusters){
        w.StartObject();
        {
          w.String("pos");
          w.StartArray();
          {
            w.Double(ch.x());
            w.Double(ch.y());
            w.Uint(ch.z());
          }
          w.EndArray();
        
          w.String("pix");
          w.StartArray();
          for(auto &ph : ch.pixelHits){
            w.StartArray();
            {
              w.Uint(ph.x());
              w.Uint(ph.y());
              w.Uint(ph.z());
            }
            w.EndArray();
          }
          w.EndArray();
        }
        w.EndObject();
      }
      w.EndArray();
    }
    w.EndObject();
  }  

  static const uint16_t s_version{3};
  std::string m_data_raw;
  uint16_t m_level_decode{0};
  uint64_t m_counter{0};
  uint64_t m_extension{0};
  uint64_t m_trigger{0};
  std::vector<ClusterHit> m_clusters;
};

#endif
