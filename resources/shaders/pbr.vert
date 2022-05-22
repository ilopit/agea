#version 450
layout (location = 0) in vec3 vPosition;
layout (location = 1) in vec3 vNormal;
layout (location = 2) in vec3 vColor;
layout (location = 3) in vec2 vTexCoord;

layout (location = 0) out vec3 outWorldPos;
layout (location = 1) out vec3 outNormal;
layout (location = 2) out vec3 outColor;
layout (location = 3) out vec2 outTexCoord;

layout (set = 0, binding = 0) uniform CameraBuffer 
{
	mat4 projection;
	mat4 view;
	vec3 camPos;
} cameraData;

struct ObjectData{
	mat4 model;
	vec3 objPos;
}; 

//all object matrices
layout(std140, set = 1, binding = 0) readonly buffer ObjectBuffer{   

	ObjectData objects[];
} objectBuffer;

void main() 
{	
	mat4 modelMatrix = objectBuffer.objects[gl_InstanceIndex].model;
	vec3 objPos      = objectBuffer.objects[gl_InstanceIndex].objPos;

	vec3 locPos = vec3(modelMatrix * vec4(vPosition, 1.0));
	
	outWorldPos = locPos + objPos;
	outNormal   = mat3(modelMatrix) * vNormal;
	outColor    = vColor;
	outTexCoord = vTexCoord;

	gl_Position =  cameraData.projection * cameraData.view * vec4(outWorldPos, 1.0);

}
