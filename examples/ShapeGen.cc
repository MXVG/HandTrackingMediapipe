#include "ShapeGen.h"
#include "Vertex.h"
#include <glm/glm.hpp>

#include <cstring>

ShapeData ShapeGenerator::Triangle(){

    ShapeData toReturn;

    Vertex tri[] =
    {
        glm::vec3(-0.6f, -0.4f, 0.0f),
        glm::vec3(1.0f, 0.0f, 0.0f),

        glm::vec3(0.6f, -0.4f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f),

        glm::vec3(0.0f, 0.6f, 0.0f),
        glm::vec3(0.0f, 0.0f, 1.0f),
    };

    toReturn.numVertices = sizeof(tri) / sizeof(*tri);
    toReturn.vertices = new Vertex[toReturn.numVertices];
    memcpy(toReturn.vertices, tri, sizeof(tri));
    toReturn.vertices = tri;

    GLushort indices[] {0, 1, 2};
    toReturn.numIndices = sizeof(indices) / sizeof(*indices);
    toReturn.indices = new GLushort[toReturn.numIndices];
    memcpy(toReturn.indices, indices, sizeof(indices));


    return toReturn;

}