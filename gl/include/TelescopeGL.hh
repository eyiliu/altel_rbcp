#ifndef TELESCOPE_GL_HH
#define TELESCOPE_GL_HH

#include <vector>
#include <memory>

#define GLEW_STATIC
#include <GL/glew.h>
#include <glm/glm.hpp>

namespace sf{
  class Window;
}

struct UniformLayer{
  GLfloat pos[4]  {0,0,0,0}; //vec3, pad
  GLfloat color[4]{0,1,0,0}; //vec3, pad
  GLfloat pitch[4]{0.028, 0.026, 1, 0}; //pitch x,y, thick z, pad
  GLint   npixel[4]{1024, 512,   1, 0};//pixe number x, y, z/1, pad
  GLfloat miss_alignment[16]{1, 0, 0, 0,
                             0, 1, 0, 0,
                             0, 0, 1, 0,
                             0, 0, 0, 1 }; //mat4
};

class TelescopeGL{
public:
  GLfloat m_win_width{1200};
  GLfloat m_win_high{400};

  glm::mat4 m_model;
  glm::mat4 m_view;
  glm::mat4 m_proj;
  std::unique_ptr<sf::Window> m_window;
  
  std::vector<UniformLayer> m_uniLayers;
  
  static const GLchar* vertexShaderSrc;
  static const GLchar* geometryShaderSrc;
  static const GLchar* fragmentShaderSrc;
  GLuint m_vertexShader{0};
  GLuint m_geometryShader{0};
  GLuint m_fragmentShader{0};
  GLuint m_shaderProgram{0};
  GLuint m_vao{0};
  GLuint m_vbo_tel{0};
  GLint m_uniModel{0};
  GLint m_uniView{0};
  GLint m_uniProj{0};
  std::vector<GLuint> m_uboLayers;
  std::vector<GLint> m_points_tel;
  
  static const GLchar* vertexShaderSrc_hit;
  static const GLchar* geometryShaderSrc_hit;
  // static const GLchar* fragmentShaderSrc_hit;
  GLuint m_vertexShader_hit{0};
  GLuint m_geometryShader_hit{0};
  GLuint m_fragmentShader_hit{0};
  GLuint m_shaderProgram_hit{0};
  GLuint m_vao_hit{0};
  GLuint m_vbo_hit{0};
  GLint m_uniModel_hit{0};
  GLint m_uniView_hit{0};
  GLint m_uniProj_hit{0}; 
  std::vector<GLint> m_points_hit;
  std::vector<GLuint> m_uboLayers_hit;

  static const GLchar* vertexShaderSrc_track;
  static const GLchar* geometryShaderSrc_track;
  // static const GLchar* fragmentShaderSrc_track;
  GLuint m_vertexShader_track{0};
  GLuint m_geometryShader_track{0};
  GLuint m_fragmentShader_track{0};
  GLuint m_shaderProgram_track{0};
  GLuint m_vao_track{0};
  GLuint m_vbo_track{0};
  GLint m_uniModel_track{0};
  GLint m_uniView_track{0};
  GLint m_uniProj_track{0}; 
  std::vector<GLfloat> m_points_track;
  
  TelescopeGL();
  ~TelescopeGL();
  
  void initializeGL();
  void terminateGL();
  void lookAt(float cameraX, float cameraY, float cameraZ,
              float centerX, float centerY, float centerZ,
              float upvectX, float upvectY, float upvectZ);
  void perspective(float povHoriz, float nearDist, float farDist);
  
  
  void addTelLayer(float    posx,     float posy,     float posz, 
                   float    colorr,   float colorg,   float colorb, 
                   float    pitchx,   float pitchy,   float thickz,
                   uint32_t pixelx,   uint32_t pixely);  
  void buildProgramTel();
  void drawTel();

  void buildProgramHit();
  void drawHit();
  void addHit(int32_t px, int32_t py, int32_t pz);
  void clearHit();

  
  void buildProgramTrack();
  void drawTrack();
  void addTrack(float px, float py, float pz,
                float dx, float dy, float dz); // P+t*D
  void clearTrack();


  void clearFrame();
  void flushFrame();
  static GLuint createShader(GLenum type, const GLchar* src);  
};

#endif
