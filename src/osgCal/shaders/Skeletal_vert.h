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
shaderText += "attribute vec4 weight;\n";
shaderText += "attribute vec4 index; /* ivec is not compatible with ATI cards and\n";
shaderText += "                       * strangely enough it doesn't work on 8600\n";
shaderText += "                       * (driver bug?), while working on 6600 */\n";
shaderText += "\n";
shaderText += "uniform mat3 rotationMatrices[31];\n";
shaderText += "uniform vec3 translationVectors[31];\n";
}
shaderText += "\n";
if ( NORMAL_MAPPING == 1 || BUMP_MAPPING == 1 ) {
shaderText += "attribute vec3 tangent;\n";
shaderText += "attribute vec3 binormal;\n";
shaderText += "varying mat3 eyeBasis; // in tangent space\n";
} else {
shaderText += "varying vec3 transformedNormal;\n";
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
    if ( !DONT_CALCULATE_VERTEX ) {
shaderText += "    vec3 totalTranslation = weight.x * translationVectors[int(index.x)];\n";
    }
shaderText += "\n";
if ( BONES_COUNT >= 2 ) {
shaderText += "    totalRotation += weight.y * rotationMatrices[int(index.y)];\n";
    if ( !DONT_CALCULATE_VERTEX ) {
shaderText += "    totalTranslation += weight.y * translationVectors[int(index.y)];\n";
    }
shaderText += "\n";
if ( BONES_COUNT >= 3 ) {
shaderText += "    totalRotation += weight.z * rotationMatrices[int(index.z)];\n";
    if ( !DONT_CALCULATE_VERTEX ) {
shaderText += "    totalTranslation += weight.z * translationVectors[int(index.z)];\n";
    }
shaderText += "\n";
if ( BONES_COUNT >= 4 ) {
shaderText += "    totalRotation += weight.w * rotationMatrices[int(index.w)];\n";
    if ( !DONT_CALCULATE_VERTEX ) {
shaderText += "    totalTranslation += weight.w * translationVectors[int(index.w)];\n";
    }
} // BONES_COUNT >= 4
} // BONES_COUNT >= 3
} // BONES_COUNT >= 2
shaderText += "\n";
  if ( !DONT_CALCULATE_VERTEX ) {
shaderText += "    vec3 transformedPosition = totalRotation * gl_Vertex.xyz + totalTranslation;\n";
  } else {
shaderText += "    vec3 transformedPosition = gl_Vertex.xyz;\n";
  }
shaderText += "    gl_Position = gl_ModelViewProjectionMatrix * vec4(transformedPosition, 1.0);\n";
shaderText += "    # ifdef __GLSL_CG_DATA_TYPES\n";
shaderText += "//    if ( clipPlanesUsed )\n";
shaderText += "    {\n";
shaderText += "        gl_ClipVertex = gl_ModelViewMatrix * vec4(transformedPosition, 1.0);\n";
shaderText += "    } \n";
shaderText += "    # endif\n";
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
shaderText += "// function clipping, when ftransform() is used. I.e. we can't get\n";
shaderText += "// correct clipping for our deformed meshes on ATI.\n";
shaderText += "//\n";
shaderText += "// R300/R400 ATI chips also doesn't support branching, so fragment\n";
shaderText += "// shader culling can be very slow?\n";
shaderText += "\n";
shaderText += "// Seems that we need separate shaders (with or w/o culling) and\n";
shaderText += "// switch culling shader only when it's necessary (since it also kills\n";
shaderText += "// speed-up at depth first pass). Also we can support only one\n";
shaderText += "// clipping plane for better performance.\n";
shaderText += "// To not make too many shaders when it not needed we may create\n";
shaderText += "// separate USE_CLIP_PLANES_CULLING flag.\n";
shaderText += "// `discard' in fragment shader is VERY slow. So for rigid meshes we\n";
shaderText += "// need turn on hardware culling, also we need two paths one for\n";
shaderText += "// NVidia with gl_ClipVertex used and one for ATI with discard.\n";
shaderText += "// Maybe not support clipping of dynamic meshes on ATI at all?\n";
shaderText += "\n";
if ( FOG ) {
shaderText += "    eyeVec = (gl_ModelViewMatrix * vec4(transformedPosition, 1.0)).xyz;\n";
} // no fog
shaderText += "\n";
if ( NORMAL_MAPPING == 1 || BUMP_MAPPING == 1 ) {
shaderText += "    vec3 t = gl_NormalMatrix * (totalRotation * tangent);\n";
shaderText += "    vec3 b = gl_NormalMatrix * (totalRotation * binormal);\n";
shaderText += "    vec3 n = gl_NormalMatrix * (totalRotation * gl_Normal);\n";
shaderText += "    // vec3 b = cross( n, t );\n";
shaderText += "    // ^ does'n work for some of our meshes (no handedness)\n";
shaderText += "    // TODO: maybe save handedness in tangent alpha and remove binormals?:\n";
shaderText += "    //    vec3 b = cross( n, t ) * tangent.a;\n";
shaderText += "\n";
shaderText += "    eyeBasis = mat3( t[0], b[0], n[0],\n";
shaderText += "                     t[1], b[1], n[1],\n";
shaderText += "                     t[2], b[2], n[2] );\n";
} else { // NORMAL_MAPPING == 1
shaderText += "    transformedNormal = gl_NormalMatrix * (totalRotation * gl_Normal);\n";
} // NORMAL_MAPPING == 1
shaderText += "\n";
} else { // no bones
shaderText += "\n";
shaderText += "    // dont touch anything when no bones influence mesh\n";
shaderText += "    gl_Position = ftransform();\n";
shaderText += "    # ifdef __GLSL_CG_DATA_TYPES\n";
shaderText += "//    if ( clipPlanesUsed )\n";
shaderText += "    {\n";
shaderText += "        gl_ClipVertex = gl_ModelViewMatrix * gl_Vertex;\n";
shaderText += "    }\n";
shaderText += "    # endif\n";
if ( FOG ) {
shaderText += "    eyeVec = (gl_ModelViewMatrix * gl_Vertex).xyz;\n";
} // no fog
shaderText += "\n";
if ( NORMAL_MAPPING == 1 || BUMP_MAPPING == 1 ) {
shaderText += "    vec3 t = gl_NormalMatrix * tangent;\n";
shaderText += "    vec3 b = gl_NormalMatrix * binormal;\n";
shaderText += "    vec3 n = gl_NormalMatrix * gl_Normal;\n";
shaderText += "\n";
shaderText += "    eyeBasis = mat3( t[0], b[0], n[0],\n";
shaderText += "                     t[1], b[1], n[1],\n";
shaderText += "                     t[2], b[2], n[2] );\n";
} else { // NORMAL_MAPPING == 1
shaderText += "    transformedNormal = gl_NormalMatrix * gl_Normal;\n";
} // NORMAL_MAPPING == 1
shaderText += "\n";
} // BONES_COUNT >= 1
shaderText += "}\n";
