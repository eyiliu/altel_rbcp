#include "JadeDataFrame.hh"
#include "mysystem.hh"
#include <iostream>


template void
JadeDataFrame::Serialize<rapidjson::PrettyWriter<rapidjson::StringBuffer>>
(rapidjson::PrettyWriter<rapidjson::StringBuffer>& ) const;

template void
JadeDataFrame::Serialize<rapidjson::Writer<rapidjson::StringBuffer>>
(rapidjson::Writer<rapidjson::StringBuffer>& ) const;

JadeDataFrame::JadeDataFrame(std::string&& data)
  : m_data_raw(std::move(data))
{
  Decode(4);
}

JadeDataFrame::JadeDataFrame(const std::string& data)
  : m_data_raw(data)
{
  Decode(4);
}

JadeDataFrame::JadeDataFrame(const rapidjson::Value &js){
  if(js["ver"].GetUint64()!=s_version){
    std::fprintf(stderr, "mismathed data writer/reader versions");
    throw;
  }
  
  m_trigger   = js["tri"].GetUint64();
  m_counter   = js["cnt"].GetUint64();
  m_extension = js["ext"].GetUint64();
    
  const auto &js_chs = js["hit"].GetArray();
  for(const auto &js_ch : js_chs){
    std::vector<PixelHit> pixelhits;
    const auto &js_phs = js_ch.GetArray();
    for(const auto &js_ph : js_phs){
      const auto &js_pos = js_ph.GetArray();
      pixelhits.emplace_back(js_pos[0].GetUint(),
                             js_pos[1].GetUint(),
                             js_pos[2].GetUint());
    }
    ClusterHit clusterhit(std::move(pixelhits));
    m_clusters.push_back(std::move(clusterhit));
  }
}


const std::string& JadeDataFrame::Raw() const
{
  return m_data_raw;
}

std::string& JadeDataFrame::Raw()
{
  return m_data_raw;
}

uint64_t JadeDataFrame::GetCounter()
{
  return m_counter;
}

uint64_t JadeDataFrame::GetExtension()
{
  return m_extension;
}

void JadeDataFrame::Decode(uint32_t level){
  if(level <= m_level_decode)
    return;
  const uint8_t* p_raw_beg = reinterpret_cast<const uint8_t *>(m_data_raw.data());
  const uint8_t* p_raw_end = p_raw_beg + m_data_raw.size();
  const uint8_t* p_raw = p_raw_beg;
  if(m_data_raw.size()<8){
    std::cerr << "JadeDataFrame: raw data length is less than 8\n";
    throw;
  }
  // std::cout << JadeUtils::ToHexString(m_data_raw)<<std::endl;
  if( *p_raw_beg!=0x5a || *(p_raw_end-1)!=0xa5){
    std::cerr << "JadeDataFrame: pkg header/trailer mismatch\n";
    //std::cerr << JadeUtils::ToHexString(m_data_raw)<<std::endl;
    std::cerr <<uint16_t((*p_raw_beg))<<std::endl;
    std::cerr <<uint16_t((*(p_raw_end-1)))<<std::endl;
    throw;
  }
  p_raw++;
  m_extension=*p_raw;
  uint32_t len_payload_data = BE32TOH(*reinterpret_cast<const uint32_t*>(p_raw)) & 0x000fffff;
  if (len_payload_data + 8 != m_data_raw.size()) {
    std::cerr << "JadeDataFrame: raw data length does not match\n";
    std::cerr << len_payload_data<<std::endl;
    std::cerr << m_data_raw.size()<<std::endl;
    throw;
  }
  p_raw += 4;
  m_counter = BE16TOH(*reinterpret_cast<const uint16_t*>(p_raw));
  m_trigger = m_counter;
  p_raw += 2;
  // m_n_x = 1024;
  // m_n_y = 512;

  if(level<2){
    m_level_decode = level;
    return;
  }

  ClusterPool pool;
  
  uint8_t l_frame_n = -1;
  uint8_t l_region_id = -1;
  while(p_raw < p_raw_end-1){
    char d = *p_raw;
    // std::cout << JadeUtils::ToHexString(&d, 1)<<std::endl;    
    if(d & 0b10000000){
      // std::cout<<"//NOT DATA 1"<<std::endl;
      if(d & 0b01000000){
        // std::cout<<"//empty or region header or busy_on/off 11"<<std::endl;
        if(d & 0b00100000){
          // std::cout<<"//emtpy or busy_on/off 111"<<std::endl;
          if(d & 0b00010000){
            // std::cout<<"//busy_on/off"<<std::endl;
            p_raw++;
            continue;
          }
          // std::cout<<"// empty 1110"<<std::endl;
          uint8_t chip_id = d & 0b00001111;
          l_frame_n++;
          p_raw++;
          d = *p_raw;
          uint8_t bunch_counter_h = d;
          p_raw++;
          continue;
        }
        // std::cout<<"// region header 110"<<std::endl;
        l_region_id = d & 0b00011111;
        p_raw++;
        continue;
      }
      // std::cout<<"//CHIP_HEADER/TRAILER or undefined 10"<<std::endl;
      if(d & 0b00100000){
        // std::cout<<"//CHIP_HEADER/TRAILER 101"<<std::endl;
        if(d & 0b00010000){
          // std::cout<<"//TRAILER 1011"<<std::endl;
          uint8_t readout_flag= d & 0b00001111;
          p_raw++;
          continue;
        }
        // std::cout<<"//HEADER 1010"<<std::endl;
        uint8_t chip_id = d & 0b00001111;
        l_frame_n++;
        p_raw++;
        d = *p_raw;
        uint8_t bunch_counter_h = d;
        p_raw++;
        continue;
      }
      std::cout<<"//undefined 100"<<std::endl;
      p_raw++;
      continue;
    }
    else{
      // std::cout<<"//DATA 0"<<std::endl;
      if(d & 0b01000000){
        // std::cout<<"//DATA SHORT 01"<<std::endl;
        if(level>2){
          uint8_t encoder_id = (d & 0b00111100)>> 2;
          uint16_t addr = (d & 0b00000011)<<8;
          p_raw++;
          d = *p_raw;
          addr += *p_raw;
          p_raw++;

          uint16_t y = addr>>1;
          uint16_t x = (l_region_id<<5)+(encoder_id<<1)+((addr&0b1)!=((addr>>1)&0b1));

          pool.addHit(x, y, m_extension);
          // m_data_x.push_back(x);
          // m_data_y.push_back(y);
          // m_data_d.push_back(m_extension);
        }
        else{
          p_raw++;
          p_raw++;
        }
        continue;
      }
      // std::cout<<"//DATA LONG 00"<<std::endl;
      if(level>2){
        uint8_t encoder_id = (d & 0b00111100)>> 2;
        uint16_t addr = (d & 0b00000011)<<8;
        p_raw++;
        d = *p_raw;
        addr += *p_raw;
        p_raw++;
        d = *p_raw;
        uint8_t hit_map = (d & 0b01111111);
        p_raw++;
        uint16_t y = addr>>1;
        uint16_t x = (l_region_id<<5)+(encoder_id<<1)+((addr&0b1)!=((addr>>1)&0b1));
 
        // m_data_x.push_back(x);
        // m_data_y.push_back(y);
        // m_data_d.push_back(m_extension);
        pool.addHit(x, y, m_extension);

        for(int i=1; i<=7; i++){
          if(hit_map & (1<<(i-1))){
	    uint16_t addr_l = addr + i;
            uint16_t y = addr_l>>1;
            uint16_t x = (l_region_id<<5)+(encoder_id<<1)+((addr_l&0b1)!=((addr_l>>1)&0b1));
            // m_data_x.push_back(x);
            // m_data_y.push_back(y);
            // m_data_d.push_back(m_extension);
            pool.addHit(x, y, m_extension);
          }
        }
      }
      else{
        p_raw++;
        p_raw++;
        p_raw++;
      }
      continue;
    }
  }
  
  pool.buildClusters();
  m_clusters = std::move(pool.m_clusters);
  
  // m_n_d = l_frame_n+1;
  m_level_decode = level;
  return;
}

void JadeDataFrame::Print(std::ostream& os, size_t ws) const
{
  rapidjson::OStreamWrapper js_osw(os);
  rapidjson::Writer<rapidjson::OStreamWrapper> js_writer(js_osw);
  Serialize(js_writer);
}


rapidjson::Value JadeDataFrame::JSON( rapidjson::MemoryPoolAllocator<> &a) const {
    rapidjson::Value js;
    js.SetObject();
    js.AddMember("det", "alptel", a);
    js.AddMember("ver",  rapidjson::Value(s_version).Move(), a);
    js.AddMember("tri",  rapidjson::Value(m_trigger).Move(), a);
    js.AddMember("cnt",  rapidjson::Value(m_counter).Move(), a);
    js.AddMember("ext",  rapidjson::Value(m_extension).Move(), a);

    rapidjson::Value js_cluster_hits;
    js_cluster_hits.SetArray();
    for(auto &ch : m_clusters){
      rapidjson::Value js_cluster_hit;
      js_cluster_hit.SetObject();

      rapidjson::Value js_cluster_hit_pos;
      js_cluster_hit_pos.SetArray();
      js_cluster_hit_pos.PushBack(rapidjson::Value(ch.x()).Move(), a);
      js_cluster_hit_pos.PushBack(rapidjson::Value(ch.y()).Move(), a);
      js_cluster_hit_pos.PushBack(rapidjson::Value(ch.z()).Move(), a);
      js_cluster_hit.AddMember("pos",  js_cluster_hit_pos, a);
      
      rapidjson::Value js_pixel_hits;
      js_pixel_hits.SetArray();
      for(auto &ph : ch.pixelHits){
        rapidjson::Value js_pixel_hit_pos;
        js_pixel_hit_pos.SetArray();
        js_pixel_hit_pos.PushBack(rapidjson::Value(ph.x()).Move(), a);
        js_pixel_hit_pos.PushBack(rapidjson::Value(ph.y()).Move(), a);
        js_pixel_hit_pos.PushBack(rapidjson::Value(ph.z()).Move(), a);
        js_pixel_hits.PushBack(js_pixel_hit_pos, a);
      }
      js_cluster_hit.AddMember("pix", js_pixel_hits, a);
      
      js_cluster_hits.PushBack(js_cluster_hit, a);
    }
    js.AddMember("hit", js_cluster_hits , a);
    
    //https://rapidjson.org/classrapidjson_1_1_generic_object.html
    //https://rapidjson.org/md_doc_tutorial.html#CreateModifyValues
    return js;
};
