#ifndef _ALTEL_CLUSTERPOOL_HH
#define _ALTEL_CLUSTERPOOL_HH

#include "mysystem.hh"

#include <vector>

struct PixelHit{
  union {
    uint64_t index;
    uint16_t loc[4];
  } data{0};

  PixelHit(uint64_t h)
    :data{ .index = h }{};
  PixelHit(uint16_t x, uint16_t y, uint16_t z)
    :data{ .loc = {x, y, z, 0} }{};
  
  inline bool operator==(const PixelHit &rh){
    return data.index == rh.data.index;
  }

  inline bool operator<(const PixelHit &rh){
    return data.index < rh.data.index;
  }
  
  inline uint64_t& index() {return data.index;}
  inline uint16_t& x() {return data.loc[0];}
  inline uint16_t& y() {return data.loc[1];}
  inline uint16_t& z() {return data.loc[2];}
};

struct ClusterHit{
  double   centerX{0};
  double   centerY{0};
  uint32_t surfIndex{0};
  uint32_t pixelSize{0};
  std::vector<PixelHit> pixelHits;

  ClusterHit(std::vector<PixelHit> &&hits)
    :pixelHits(std::move(hits))
  {
  };

  ClusterHit(const std::vector<PixelHit> &hits)
    :pixelHits(hits)
  {
  };
  
  void buildClusterCenter(){
    if(pixelSize)
      return;//already build;
    pixelSize = pixelHits.size();
    if(!pixelSize)
      return;//nothing to build;
    for(auto &ph : pixelHits){
      centerX+= ph.x();
      centerY+= ph.y();
      surfIndex = ph.z();
    }
    centerX /= pixelSize;
    centerY /= pixelSize;
  }
};

class ClusterPool{
public:
  inline void addHit(uint16_t x, uint16_t y, uint16_t z){
    m_hit_col.emplace_back(x, y, z);
  }  
  void buildClusters();  
  std::vector<PixelHit> m_hit_col;
  std::vector<ClusterHit> m_cluster_col;
};


#endif

