shaderText += "// -*-c++-*-\n";
shaderText += "\n";
shaderText += "attribute vec3 position;\n";
shaderText += "attribute vec3 normal;\n";
shaderText += "\n";
if ( TEXTURING == 1 || NORMAL_MAPPING == 1 ) {
shaderText += "attribute vec2 texCoord; \n";
}
shaderText += "\n";
if ( BONES_COUNT >= 1 ) {
shaderText += "attribute vec4 weight;\n";
shaderText += "attribute vec4 index; /* ivec gives small speedup 17.7 vs 17.9 msec\n";
shaderText += "                         (but gl_FrontFacing is not used with 17.9)\n";
shaderText += "                         but it's not compatible with ATI cards */\n";
shaderText += "\n";
shaderText += "uniform mat3 rotationMatrices[31];\n";
shaderText += "uniform vec3 translationVectors[31];\n";
}
shaderText += "\n";
if ( NORMAL_MAPPING == 1 ) {
shaderText += "attribute vec3 tangent;\n";
shaderText += "attribute vec3 binormal;\n";
}
shaderText += "\n";
if ( TEXTURING == 1 || NORMAL_MAPPING == 1 ) {
shaderText += "varying vec2 texUV;\n";
}
shaderText += "\n";
if (!( NORMAL_MAPPING )) {
shaderText += "varying vec3 transformedNormal;\n";
}
shaderText += "\n";
if ( NORMAL_MAPPING == 1 ) {
shaderText += "varying vec3 lightVec;\n";
if ( SHINING ) {
shaderText += "varying vec3 halfVec;//blinn\n";
}
shaderText += "//varying mat3 eyeBasis; // in tangent space\n";
}
shaderText += "\n";
if ( SHINING ) {
shaderText += "//varying vec3 eyeVec;//phong\n";
}
shaderText += "\n";
shaderText += "void main()\n";
shaderText += "{\n";
if ( TEXTURING == 1 || NORMAL_MAPPING == 1 ) {
shaderText += "    texUV = texCoord; // export texCoord to fragment shader\n";
}
shaderText += "\n";
if ( NORMAL_MAPPING == 1 ) {
shaderText += "    vec3 transformedNormal; // its not used in fragment shader\n";
shaderText += "                            // so define it locally\n";
}
shaderText += "\n";
if ( BONES_COUNT >= 1 ) {
shaderText += "    vec3 transformedPosition;\n";
shaderText += "    \n";
shaderText += "    transformedPosition =  //remark '=' instead of '+='\n";
shaderText += "        weight.x * (rotationMatrices[int(index.x)] * position + \n";
shaderText += "                       translationVectors[int(index.x)]);\n";
shaderText += "    transformedNormal =  // remark '=' instead of '+='\n";
shaderText += "        weight.x * (rotationMatrices[int(index.x)] * normal);\n";
 if ( NORMAL_MAPPING == 1 ) {
shaderText += "    vec3 transformedTangent;\n";
shaderText += "    vec3 transformedBinormal;\n";
shaderText += "\n";
shaderText += "    transformedTangent =  // remark '=' instead of '+='\n";
shaderText += "        weight.x * (rotationMatrices[int(index.x)] * tangent);\n";
shaderText += "    transformedBinormal =  // remark '=' instead of '+='\n";
shaderText += "        weight.x * (rotationMatrices[int(index.x)] * binormal);\n";
 } // NORMAL_MAPPING == 1
if ( BONES_COUNT >= 2 ) {
shaderText += "    transformedPosition += \n";
shaderText += "        weight.y * (rotationMatrices[int(index.y)] * position + \n";
shaderText += "                       translationVectors[int(index.y)]);\n";
shaderText += "    transformedNormal += \n";
shaderText += "        weight.y * (rotationMatrices[int(index.y)] * normal);\n";
 if ( NORMAL_MAPPING == 1 ) {
shaderText += "    transformedTangent += \n";
shaderText += "        weight.y * (rotationMatrices[int(index.y)] * tangent);\n";
shaderText += "    transformedBinormal += \n";
shaderText += "        weight.y * (rotationMatrices[int(index.y)] * binormal);\n";
 } // NORMAL_MAPPING == 1
if ( BONES_COUNT >= 3 ) {
shaderText += "    transformedPosition += \n";
shaderText += "        weight.z * (rotationMatrices[int(index.z)] * position + \n";
shaderText += "                       translationVectors[int(index.z)]);\n";
shaderText += "    transformedNormal += \n";
shaderText += "        weight.z * (rotationMatrices[int(index.z)] * normal);\n";
 if ( NORMAL_MAPPING == 1 ) {
shaderText += "    transformedTangent += \n";
shaderText += "        weight.z * (rotationMatrices[int(index.z)] * tangent);\n";
shaderText += "    transformedBinormal += \n";
shaderText += "        weight.z * (rotationMatrices[int(index.z)] * binormal);\n";
 } // NORMAL_MAPPING == 1
if ( BONES_COUNT >= 4 ) {
shaderText += "    transformedPosition += \n";
shaderText += "        weight.w * (rotationMatrices[int(index.w)] * position + \n";
shaderText += "                       translationVectors[int(index.w)]);\n";
shaderText += "    transformedNormal += \n";
shaderText += "        weight.w * (rotationMatrices[int(index.w)] * normal);\n";
 if ( NORMAL_MAPPING == 1 ) {
shaderText += "    transformedTangent += \n";
shaderText += "        weight.w * (rotationMatrices[int(index.w)] * tangent);\n";
shaderText += "    transformedBinormal += \n";
shaderText += "        weight.w * (rotationMatrices[int(index.w)] * binormal);\n";
 } // NORMAL_MAPPING == 1
} // BONES_COUNT >= 4
} // BONES_COUNT >= 3
} // BONES_COUNT >= 2
shaderText += "\n";
shaderText += "    transformedNormal = gl_NormalMatrix * transformedNormal;\n";
shaderText += "    gl_Position = gl_ModelViewProjectionMatrix * vec4(transformedPosition, 1.0);\n";
if ( FOG && !SHINING ) {
shaderText += "    //vec3 eyeVec = (gl_ModelViewMatrix * vec4(transformedPosition, 1.0)).xyz;\n";
} else if ( SHINING ) {
shaderText += "    //eyeVec = (gl_ModelViewMatrix * vec4(transformedPosition, 1.0)).xyz;\n";
} 
if ( FOG ) {
shaderText += "    vec3 eyeVec = (gl_ModelViewMatrix * vec4(transformedPosition, 1.0)).xyz;\n";
shaderText += "    gl_FogFragCoord = length( eyeVec );\n";
} // no fog
shaderText += "\n";
if ( NORMAL_MAPPING == 1 ) {
shaderText += "    mat3 tangentBasis = mat3( gl_NormalMatrix * transformedTangent,\n";
shaderText += "                              gl_NormalMatrix * transformedBinormal,\n";
shaderText += "                              transformedNormal );\n";
shaderText += "\n";
shaderText += "//     eyeBasis = mat3( vec3(1,0,0) * tangentBasis,\n";
shaderText += "//                      vec3(0,1,0) * tangentBasis,\n";
shaderText += "//                      vec3(0,0,1) * tangentBasis );\n";
shaderText += "\n";
shaderText += "    lightVec = gl_LightSource[0].position.xyz * tangentBasis;\n";
 if ( SHINING ) {
shaderText += "    halfVec = gl_LightSource[0].halfVector.xyz * tangentBasis;\n";
shaderText += "    //eyeVec *= tangentBasis;\n";
 } // no shining
} // NORMAL_MAPPING == 1
shaderText += "\n";
} else { // no bones
shaderText += "\n";
shaderText += "    // dont touch anything when no bones influence mesh\n";
shaderText += "    transformedNormal = gl_NormalMatrix * normal;\n";
shaderText += "    gl_Position = gl_ModelViewProjectionMatrix * vec4(position, 1.0);\n";
if ( FOG && !SHINING ) {
shaderText += "    //vec3 eyeVec = (gl_ModelViewMatrix * vec4(position, 1.0)).xyz;\n";
} else if ( SHINING ) {
shaderText += "    //eyeVec = (gl_ModelViewMatrix * vec4(position, 1.0)).xyz;\n";
} 
if ( FOG ) {
shaderText += "    vec3 eyeVec = (gl_ModelViewMatrix * vec4(position, 1.0)).xyz;\n";
shaderText += "    gl_FogFragCoord = length( eyeVec );\n";
} // no fog
shaderText += "\n";
if ( NORMAL_MAPPING == 1 ) {
shaderText += "    mat3 tangentBasis = mat3( gl_NormalMatrix * tangent,\n";
shaderText += "                              gl_NormalMatrix * binormal,\n";
shaderText += "                              transformedNormal );\n";
shaderText += "\n";
shaderText += "//     eyeBasis = mat3( vec3(1,0,0) * tangentBasis,\n";
shaderText += "//                      vec3(0,1,0) * tangentBasis,\n";
shaderText += "//                      vec3(0,0,1) * tangentBasis );\n";
shaderText += "\n";
shaderText += "    lightVec = gl_LightSource[0].position.xyz * tangentBasis;\n";
 if ( SHINING ) {
shaderText += "    halfVec = gl_LightSource[0].halfVector.xyz * tangentBasis;\n";
shaderText += "    //eyeVec *= tangentBasis;\n";
 } // no shining
} // NORMAL_MAPPING == 1
shaderText += "\n";
} // BONES_COUNT >= 1
shaderText += "\n";
if ( SHINING ) {
shaderText += "    //eyeVec = normalize( eyeVec );\n";
} // no shining
  if ( FOG ) {
shaderText += "    //distance = -gl_Position.z; <- junk with z smaller than in eye vec\n";
shaderText += "    //distance = -eyeVec.z; // plane parallel to screen\n";
shaderText += "    //distance = clamp( length( eyeVec ), 0.0, 2000.0 );\n";
shaderText += "    // clamping in vertex shader is not correct (especially for large triangles)\n";
shaderText += "    //gl_FogCoord - when glFogCoord() used for per-vertex fog coord,\n";
shaderText += "    //i.e. volumetric fog\n";
  } // no fog
shaderText += "}\n";
