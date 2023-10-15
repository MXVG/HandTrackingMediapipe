#include <array>
#include <iostream>
#include <ostream>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <pthread.h>

#include "glad/glad.h" // must go before glfw3.h
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>

//#include "ShapeGen.h"
#include "shader_s.h"

#define PORT 8080
#define NUM_FLOATS 60

#define  NUM_VERTICES_PER_TRI  3
#define  NUM_FLOATS_PER_VERTICE  6
//#define uint VERTEX_BYTE_SIZE  

int shouldExit = 0;
pthread_mutex_t dataMutex = PTHREAD_MUTEX_INITIALIZER;

float currLandmarkData[NUM_FLOATS];

void* serverThreadFunction(void* arg){

  int sockfd;
  struct sockaddr_in serverAddr;
  char buffer[1024];
  const char* message = "Hello, client! This is a UDP server.";
  
  //create a UDP socket
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if(sockfd < 0){
    perror("Socket creation failed");
    exit(EXIT_FAILURE);
  }

  memset(&serverAddr, 0, sizeof(serverAddr));

  serverAddr.sin_family = AF_INET;
  serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serverAddr.sin_port = htons(PORT);

  //Bind the socket to the server address
  if(bind(sockfd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0){
    perror("Binding failed");
    close(sockfd);
    exit(EXIT_FAILURE);
  }

  printf("UDP server listening on port %d...\n", PORT);

  while(!shouldExit){
    struct sockaddr_in clientAddr;
    socklen_t addrLen = sizeof(clientAddr);


    char buf[sizeof(float) * NUM_FLOATS];

    //recieve data from client
    ssize_t numBytesReceived = recvfrom(sockfd, buf, sizeof(buf), 0, (struct sockaddr *)&clientAddr, &addrLen);

    if (numBytesReceived < 0) {
        perror("Receive failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }


    memcpy(currLandmarkData, buf, sizeof(currLandmarkData));

    for(int i = 0; i < NUM_FLOATS; i++){
      printf("Received data from client: %f\n", currLandmarkData[i]);
    }
    printf("\n");

  }

  close(sockfd);
  return NULL;
}


void error_callback(int error, const char* description) {
  std::cerr << "error[" << error << "]:" << description << std::endl;
}


struct Vertex {
  glm::vec2 position;
  glm::vec3 color;
};

int main(int argc, char **argv) {



  pthread_t serverThread;
  if(pthread_create(&serverThread, NULL, serverThreadFunction, NULL) != 0){
    perror("Failed to create server thread");
    return EXIT_FAILURE;
  }


  Shader myShader("/home/mxvg/mediapipe/mediapipe/calculators/HandGestureDemo/examples/vertexShader.vert", 
  "/home/mxvg/mediapipe/mediapipe/calculators/HandGestureDemo/examples/fragmentShader.frag");

  GLFWwindow *window;

  glfwSetErrorCallback(error_callback);
  if (!glfwInit()) {
    exit(EXIT_FAILURE);
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  
  window = glfwCreateWindow(640, 480, "Hello Triangle", nullptr, nullptr);
  
  if (!window) {
    glfwTerminate();
    exit(EXIT_FAILURE);
  }

  glfwMakeContextCurrent(window);
  gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
  glfwSwapInterval(1);

  std::array<Vertex, 3> vertices {
    glm::vec2(-0.6f, -0.4f), glm::vec3(1.f, 0.f, 0.f),
    glm::vec2(0.6f, -0.4f), glm::vec3(0.f, 1.f, 0.f),
    glm::vec2(0.0f,   0.6f), glm::vec3(0.f, 0.f, 1.f)
  };

  for (const auto& vertex : vertices) {
    std::cout << "vertex.position: " << glm::to_string(vertex.position) << ",";
    std::cout << "vertex.color: " << glm::to_string(vertex.color) << std::endl;
  }

  GLuint vertex_buffer;
  glGenBuffers(1, &vertex_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices.data(), GL_STATIC_DRAW);



  GLint mvp_location = glGetUniformLocation(myShader.ID, "MVP");
  GLint vpos_location = glGetAttribLocation(myShader.ID, "vPos");
  GLint vcol_location = glGetAttribLocation(myShader.ID, "vCol");
  glEnableVertexAttribArray(vpos_location);
  glVertexAttribPointer(vpos_location, 2, GL_FLOAT, GL_FALSE,
                        sizeof(float) * 5, nullptr);
  glEnableVertexAttribArray(vcol_location);
  glVertexAttribPointer(vcol_location, 3, GL_FLOAT, GL_FALSE,
                        sizeof(float) * 5, (void *)(sizeof(float) * 2));


  while (!glfwWindowShouldClose(window)) {

    int width;
    int height;
    glfwGetFramebufferSize(window, &width, &height);

    float ratio = width / static_cast<float>(height);
    glViewport(0, 0, width, height);
    glClear(GL_COLOR_BUFFER_BIT);
    
    glm::mat4 trans = glm::mat4(1.0f);
    trans = glm::rotate(trans, (float)glfwGetTime(), glm::vec3(0.0f, 0.0f, 1.0f));
    
    glm::mat4 p = glm::ortho(-ratio, ratio, -1.f, 1.f, -1.f, 1.f);

    glm::mat4 mvp = p * trans;
    myShader.use();
    glUniformMatrix4fv(mvp_location, 1, GL_FALSE,  glm::value_ptr(mvp));
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  shouldExit = 1;

  pthread_join(serverThread, NULL);

  glfwDestroyWindow(window);
  glfwTerminate();
  exit(EXIT_SUCCESS);
}



