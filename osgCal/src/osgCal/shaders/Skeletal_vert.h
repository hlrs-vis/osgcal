shaderText += "// -*-c++-*-\n";
shaderText += "\n";
shaderText += "# ifndef __GLSL_CG_DATA_TYPES // the space after '#' is necessary to\n";
shaderText += "                              // differentiate `sed' defines from GLSL one's\n";
shaderText += "  // remove half float types on non-nVidia videocards\n";
shaderText += "  # define half    float\n";
shaderText += "  # define half2   vec2\n";
shaderText += "  # define half3   vec3\n";
shaderText += "  # define half4   vec4\n";
shaderText += "  # define half3x3 mat3\n";
shaderText += "  # define ivec4   vec4\n";
shaderText += "# endif\n";
shaderText += "\n";
if ( BONES_COUNT >= 1 ) {
shaderText += "# define weight gl_MultiTexCoord2\n";
shaderText += "# define index  gl_MultiTexCoord3\n";
shaderText += "\n";
shaderText += "uniform mat3 rotationMatrices[31];\n";
shaderText += "uniform vec3 translationVectors[31];\n";
}
shaderText += "\n";
shaderText += "varying vec3 vNormal;\n";
shaderText += "\n";
if ( NORMAL_MAPPING == 1 || BUMP_MAPPING == 1 ) {
shaderText += "# define inputTangent     (gl_MultiTexCoord1.xyz /* / 32767.0 */)\n";
shaderText += "# define inputHandedness   gl_MultiTexCoord1.w\n";
shaderText += "varying vec3 tangent;\n";
shaderText += "varying vec3 binormal;\n";
}
shaderText += "\n";
if ( FOG ) {
shaderText += "varying vec3 eyeVec;\n";
}
shaderText += "\n";
shaderText += "//uniform bool clipPlanesUsed;\n";
shaderText += "\n";
shaderText += "void main()\n";
shaderText += "{\n";
if ( TEXTURING == 1 || NORMAL_MAPPING == 1 || BUMP_MAPPING == 1 ) {
shaderText += "    gl_TexCoord[0].st = gl_MultiTexCoord0.st; // export texCoord to fragment shader\n";
}
shaderText += "\n";
if ( BONES_COUNT >= 1 ) {
shaderText += "    mat3 totalRotation = weight.x * rotationMatrices[int(index.x)];\n";
shaderText += "    vec3 transformedPosition = weight.x * translationVectors[int(index.x)];\n";
shaderText += "\n";
if ( BONES_COUNT >= 2 ) {
shaderText += "    totalRotation += weight.y * rotationMatrices[int(index.y)];\n";
shaderText += "    transformedPosition += weight.y * translationVectors[int(index.y)];\n";
shaderText += "\n";
if ( BONES_COUNT >= 3 ) {
shaderText += "    totalRotation += weight.z * rotationMatrices[int(index.z)];\n";
shaderText += "    transformedPosition += weight.z * translationVectors[int(index.z)];\n";
shaderText += "\n";
if ( BONES_COUNT >= 4 ) {
shaderText += "    totalRotation += weight.w * rotationMatrices[int(index.w)];\n";
shaderText += "    transformedPosition += weight.w * translationVectors[int(index.w)];\n";
} // BONES_COUNT >= 4
} // BONES_COUNT >= 3
} // BONES_COUNT >= 2
shaderText += "\n";
shaderText += "    transformedPosition += totalRotation * gl_Vertex.xyz;\n";
shaderText += "    gl_Position = gl_ModelViewProjectionMatrix * vec4(transformedPosition, 1.0);\n";
shaderText += "    # ifdef __GLSL_CG_DATA_TYPES\n";
shaderText += "      gl_ClipVertex = gl_ModelViewMatrix * vec4(transformedPosition, 1.0);\n";
shaderText += "    # endif\n";
if ( 0 ) {
shaderText += "//  8.5 -- no clip planes\n";
shaderText += "// 10.2 -- gl_ClipVertex always set (20% slowdown, on both 6600 and 8600)\n";
shaderText += "// 10.8 -- if ( clipPlanesUsed /* == true */  ) gl_ClipVertex = ...\n";
shaderText += "//  9.2 -- if ( clipPlanesUsed /* == false */ ) gl_ClipVertex = ...\n";
shaderText += "// i.e.:\n";
shaderText += "//    if ( clipPlanesUsed )  ~ 0.6-0.7 ms (uniform brancing is not optimized!)\n";
shaderText += "//    gl_ClipVertex = ...    ~ 1.6 ms\n";
shaderText += "// For not so hi-poly scenes gl_ClipVertex slowdown is 2-4%.\n";
shaderText += "// gl_ClipVertex slowdown is independent on whether glClipPlane is\n";
shaderText += "// used or not.\n";
shaderText += "//\n";
shaderText += "// For performance it's better to make separate shaders and determine\n";
shaderText += "// at the draw time which to use. But we can get too many shaders\n";
shaderText += "// (count of shaders is doubled with another option in vertex shader).\n";
shaderText += "// And also shader with clip planes support must be also compiled at\n";
shaderText += "// the GLObjectsVisitor stage (but can be not used).\n";
shaderText += "\n";
shaderText += "// ATI doesn't support programmable clipping, gl_ClipVertex assignment\n";
shaderText += "// in shaders throw it into software mode. ATI only support fixed\n";
shaderText += "// function clipping, when ftransform() is used. \n";
shaderText += "// But people say clipping on ati works w/o any ftransform() or gl_ClipVertex\n";
}
shaderText += "\n";
if ( FOG ) {
shaderText += "    eyeVec = (gl_ModelViewMatrix * vec4(transformedPosition, 1.0)).xyz;\n";
} // no fog
shaderText += "\n";
shaderText += "    vNormal = gl_NormalMatrix * (totalRotation * gl_Normal);\n";
if ( NORMAL_MAPPING == 1 || BUMP_MAPPING == 1 ) {
shaderText += "    tangent = gl_NormalMatrix * (totalRotation * inputTangent);\n";
shaderText += "    binormal = cross( vNormal, tangent ) * inputHandedness;\n";
} // no tangent space
shaderText += "\n";
} else { // no bones
shaderText += "\n";
shaderText += "    // dont touch anything when no bones influence mesh\n";
shaderText += "    gl_Position = ftransform();\n";
shaderText += "    # ifdef __GLSL_CG_DATA_TYPES\n";
shaderText += "      gl_ClipVertex = gl_ModelViewMatrix * gl_Vertex;\n";
shaderText += "    # endif\n";
if ( FOG ) {
shaderText += "    eyeVec = (gl_ModelViewMatrix * gl_Vertex).xyz;\n";
} // no fog
shaderText += "\n";
shaderText += "    vNormal = gl_NormalMatrix * gl_Normal;\n";
if ( NORMAL_MAPPING == 1 || BUMP_MAPPING == 1 ) {
shaderText += "    tangent = gl_NormalMatrix * inputTangent;\n";
shaderText += "    binormal = cross( vNormal, tangent ) * inputHandedness;\n";
} // no tangent space
shaderText += "\n";
} // BONES_COUNT >= 1
shaderText += "}\n";
