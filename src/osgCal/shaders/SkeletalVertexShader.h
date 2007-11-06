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
//shaderText += "     # ifdef __GLSL_CG_DATA_TYPES\n";
shaderText += "//     gl_ClipVertex = gl_ModelViewMatrix * vec4(transformedPosition, 1.0);\n";
//shaderText += "     # endif\n";
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
//shaderText += "     # ifdef __GLSL_CG_DATA_TYPES\n";
shaderText += "//     gl_ClipVertex = gl_ModelViewMatrix * gl_Vertex;\n";
//shaderText += "     # endif\n";
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
