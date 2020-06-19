#ifndef JADEPIX_JADEDATAFRAME_WS
#define JADEPIX_JADEDATAFRAME_WS

#include <string>
#include <vector>
#include <memory>

#include "mysystem.hh"

class JadeDataFrame;
using JadeDataFrameSP = std::shared_ptr<JadeDataFrame>;
  
class JadeDataFrame {
public:

  JadeDataFrame(std::string& data);
  JadeDataFrame(std::string&& data);
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
  
  const std::vector<uint16_t>& Data_X() const {return m_data_x; }
  const std::vector<uint16_t>& Data_Y() const {return m_data_y; }
  const std::vector<uint16_t>& Data_D() const {return m_data_d; }
  
  void Print(std::ostream& os, size_t ws = 0) const;

  template <typename W>
  void Deserialize(W& w) const {
    
  }
  
  template <typename W>
  void Serialize(W& w) const {
    w.StartObject();
    {
      w.String("det");
      w.String("alpide");

      w.String("geo");
      w.StartArray();
      {
        w.StartArray();
        {
          w.Uint(m_n_x);
          w.Double(29.24);
        }
        w.EndArray();
        w.StartArray();
        {
          w.Uint(m_n_y);
          w.Double(26.88);
        }
        w.EndArray();        
      }
      w.EndArray();
      
      w.String("tri");
      w.Uint(m_counter);

      w.String("ext");
      w.Uint(m_extension);
      
      w.String("hit_xyz_array");
      w.StartArray();
      {
        auto it_x = m_data_x.begin();
        auto it_y = m_data_y.begin();
        auto it_z = m_data_d.begin();
        while(it_x!=m_data_x.end()){
          w.StartArray();
          {
            w.Uint(*it_x);
            w.Uint(*it_y);
            //w.Uint(*it_z);
            w.Uint(m_extension);
          }
          w.EndArray();
          it_x++;
          it_y++;
          it_z++;
        }
      }
      w.EndArray();
    }
    w.EndObject();
  }
  
private:
  std::string m_data_raw;
  uint16_t m_level_decode{0};
  uint64_t m_counter{0};
  uint64_t m_extension{0};
  uint64_t m_trigger{0};
  
  uint16_t m_n_x{0};
  uint16_t m_n_y{1};
  uint16_t m_n_d{1}; //Z

  std::vector<uint16_t> m_data_x;
  std::vector<uint16_t> m_data_y;
  std::vector<uint16_t> m_data_d;  
};

#endif
