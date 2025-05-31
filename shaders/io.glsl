#ifndef POSITION_ATTRIBUTE_LOCATION
#define POSITION_ATTRIBUTE_LOCATION 0
#endif

#ifndef TEXTURECOORDINATES_ATTRIBUTE_LOCATION
#define TEXTURECOORDINATES_ATTRIBUTE_LOCATION 1
#endif

#ifndef NORMAL_ATTRIBUTE_LOCATION
#define NORMAL_ATTRIBUTE_LOCATION 2
#endif

layout(location = POSITION_ATTRIBUTE_LOCATION) in vec3 a_position;
out vec3 v_position;

#ifdef HAS_NORMAL_VEC3
layout(location = NORMAL_ATTRIBUTE_LOCATION) in vec3 a_normal;
out vec3 v_normal;
#endif

#ifdef HAS_TEXCOORD_0_VEC2
layout(location = TEXTURECOORDINATES_ATTRIBUTE_LOCATION) in vec2 a_texcoord;
out vec2 v_texcoord;
#endif
