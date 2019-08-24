#version 410 core
uniform samplerCube tex_cubemap;

out VS_OUT{
	vec3    tc;
} vs_out; 
uniform mat4 view;
layout (location = 0) out vec4 color; 

void main(void){
    vec3[4] vertices = vec3[4](vec3(-1.0, -1.0, 1.0),
							vec3( 1.0, -1.0, 1.0), 
							vec3(-1.0,  1.0, 1.0), 
							vec3( 1.0,  1.0, 1.0));    
                                                              
    vs_out.tc =  vertices[gl_VertexID] * mat3(view);
                                                              
    gl_Position = vec4(vertices[gl_VertexID], 1.0);
}
