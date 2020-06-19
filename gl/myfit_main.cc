#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <csignal>

#include <iostream>

#include "Mathematics/ApprOrthogonalLine3.h"

static sig_atomic_t g_done = 0;
int main(int argc, char **argv){
  // signal(SIGINT, [](int){g_done+=1;});
  gte::ApprOrthogonalLine3<double> linefit;

  std::vector<gte::Vector3<double>> hits;
  for(int i= 0; i<6; i++){
    // hits.push_back(gte::Vector3<double>(i,i,2*i));
    double x=i;
    hits.push_back({x,x,2*x});
  }
  linefit.Fit(hits);
  gte::Line3<double> line = linefit.GetParameters();

  // line.origin;
  // line.direction;

  std::printf("origin: %f, %f, %f; direction:%f, %f, %f \n",
              line.origin[0],line.origin[1],line.origin[2],
              line.direction[0],line.direction[1],line.direction[2]
              );
    
  std::cout<< "yes, compiled, run"<<std::endl;
  
  return 0;
  
}
