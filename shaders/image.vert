#include <io.glsl>

void main() {
    v_position = a_position;
    gl_Position = vec4(a_position, 1);
}
