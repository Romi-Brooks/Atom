#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 0) out vec3 vertexColor;

layout(set = 1, binding = 0, std140) uniform CameraData {
    mat4 modelViewProjection;
};

void main() {
    vertexColor = inColor;
    gl_Position = modelViewProjection * vec4(inPosition, 1.0);
}
