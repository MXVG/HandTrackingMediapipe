cc_library(
  name = "Vertex",
  hdrs = ["Vertex.h"],
  deps = ["@glm//:glm"],
)

cc_library(
  name = "ShapeData",
  hdrs = ["ShapeData.h"],
  deps = [
    ":Vertex",
    "//mediapipe/calculators/HandGestureDemo/glad:glad"
  ],
)

cc_library(
  name = "ShapeGen",
  srcs = ["ShapeGen.cc"],
  hdrs = ["ShapeGen.h"],
  deps = [
    "@glm//:glm",
    ":ShapeData",
  ],

)

cc_library(
  name = "shader_s",
  srcs = ["shader_s.cc"],
  hdrs = ["shader_s.h"],
  deps = [
    "//mediapipe/calculators/HandGestureDemo/glad:glad",
  ],
)

cc_binary(
  name = "hello_triangle",
  srcs = ["hello_triangle.cc"],
  deps = [
    "@glfw//:glfw",
    "@glm//:glm",
    "//mediapipe/calculators/HandGestureDemo/glad:glad",
    ":ShapeGen",
    ":shader_s",
  ],
)
