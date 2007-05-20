// -*-c++-*-

attribute vec3 position;
attribute vec3 normal;

#if TEXTURING == 1 || NORMAL_MAPPING == 1
attribute vec2 texCoord; 
#endif

#if BONES_COUNT >= 1
attribute vec4 weight;
attribute vec4 index; /* ivec gives small speedup 17.7 vs 17.9 msec
                         (but gl_FrontFacing is not used with 17.9)
                         but it's not compatible with ATI cards */

uniform mat3 rotationMatrices[31];
uniform vec3 translationVectors[31];
#endif

#if NORMAL_MAPPING == 1
attribute vec3 tangent;
attribute vec3 binormal;
#endif

#if TEXTURING == 1 || NORMAL_MAPPING == 1
varying vec2 texUV;
#endif

#ifndef NORMAL_MAPPING
varying vec3 transformedNormal;
#endif

#if NORMAL_MAPPING == 1
varying vec3 lightVec;
#if SHINING
varying vec3 halfVec;//blinn
#endif
//varying mat3 eyeBasis; // in tangent space
#endif

#if SHINING
//varying vec3 eyeVec;//phong
#endif

void main()
{
#if TEXTURING == 1 || NORMAL_MAPPING == 1
    texUV = texCoord; // export texCoord to fragment shader
#endif

#if NORMAL_MAPPING == 1
    vec3 transformedNormal; // its not used in fragment shader
                            // so define it locally
#endif

#if BONES_COUNT >= 1
    vec3 transformedPosition;
    
    transformedPosition =  //remark '=' instead of '+='
        weight.x * (rotationMatrices[int(index.x)] * position + 
                       translationVectors[int(index.x)]);
    transformedNormal =  // remark '=' instead of '+='
        weight.x * (rotationMatrices[int(index.x)] * normal);
 #if NORMAL_MAPPING == 1
    vec3 transformedTangent;
    vec3 transformedBinormal;

    transformedTangent =  // remark '=' instead of '+='
        weight.x * (rotationMatrices[int(index.x)] * tangent);
    transformedBinormal =  // remark '=' instead of '+='
        weight.x * (rotationMatrices[int(index.x)] * binormal);
 #endif // NORMAL_MAPPING == 1
#if BONES_COUNT >= 2
    transformedPosition += 
        weight.y * (rotationMatrices[int(index.y)] * position + 
                       translationVectors[int(index.y)]);
    transformedNormal += 
        weight.y * (rotationMatrices[int(index.y)] * normal);
 #if NORMAL_MAPPING == 1
    transformedTangent += 
        weight.y * (rotationMatrices[int(index.y)] * tangent);
    transformedBinormal += 
        weight.y * (rotationMatrices[int(index.y)] * binormal);
 #endif // NORMAL_MAPPING == 1
#if BONES_COUNT >= 3
    transformedPosition += 
        weight.z * (rotationMatrices[int(index.z)] * position + 
                       translationVectors[int(index.z)]);
    transformedNormal += 
        weight.z * (rotationMatrices[int(index.z)] * normal);
 #if NORMAL_MAPPING == 1
    transformedTangent += 
        weight.z * (rotationMatrices[int(index.z)] * tangent);
    transformedBinormal += 
        weight.z * (rotationMatrices[int(index.z)] * binormal);
 #endif // NORMAL_MAPPING == 1
#if BONES_COUNT >= 4
    transformedPosition += 
        weight.w * (rotationMatrices[int(index.w)] * position + 
                       translationVectors[int(index.w)]);
    transformedNormal += 
        weight.w * (rotationMatrices[int(index.w)] * normal);
 #if NORMAL_MAPPING == 1
    transformedTangent += 
        weight.w * (rotationMatrices[int(index.w)] * tangent);
    transformedBinormal += 
        weight.w * (rotationMatrices[int(index.w)] * binormal);
 #endif // NORMAL_MAPPING == 1
#endif // BONES_COUNT >= 4
#endif // BONES_COUNT >= 3
#endif // BONES_COUNT >= 2

    transformedNormal = gl_NormalMatrix * transformedNormal;
    gl_Position = gl_ModelViewProjectionMatrix * vec4(transformedPosition, 1.0);
#if FOG && !SHINING
    //vec3 eyeVec = (gl_ModelViewMatrix * vec4(transformedPosition, 1.0)).xyz;
#elseif SHINING
    //eyeVec = (gl_ModelViewMatrix * vec4(transformedPosition, 1.0)).xyz;
#endif 
#if FOG
    vec3 eyeVec = (gl_ModelViewMatrix * vec4(transformedPosition, 1.0)).xyz;
    gl_FogFragCoord = length( eyeVec );
#endif // no fog

#if NORMAL_MAPPING == 1
    mat3 tangentBasis = mat3( gl_NormalMatrix * transformedTangent,
                              gl_NormalMatrix * transformedBinormal,
                              transformedNormal );

//     eyeBasis = mat3( vec3(1,0,0) * tangentBasis,
//                      vec3(0,1,0) * tangentBasis,
//                      vec3(0,0,1) * tangentBasis );

    lightVec = gl_LightSource[0].position.xyz * tangentBasis;
 #if SHINING
    halfVec = gl_LightSource[0].halfVector.xyz * tangentBasis;
    //eyeVec *= tangentBasis;
 #endif // no shining
#endif // NORMAL_MAPPING == 1

#else // no bones

    // dont touch anything when no bones influence mesh
    transformedNormal = gl_NormalMatrix * normal;
    gl_Position = gl_ModelViewProjectionMatrix * vec4(position, 1.0);
#if FOG && !SHINING
    //vec3 eyeVec = (gl_ModelViewMatrix * vec4(position, 1.0)).xyz;
#elseif SHINING
    //eyeVec = (gl_ModelViewMatrix * vec4(position, 1.0)).xyz;
#endif 
#if FOG
    vec3 eyeVec = (gl_ModelViewMatrix * vec4(position, 1.0)).xyz;
    gl_FogFragCoord = length( eyeVec );
#endif // no fog

#if NORMAL_MAPPING == 1
    mat3 tangentBasis = mat3( gl_NormalMatrix * tangent,
                              gl_NormalMatrix * binormal,
                              transformedNormal );

//     eyeBasis = mat3( vec3(1,0,0) * tangentBasis,
//                      vec3(0,1,0) * tangentBasis,
//                      vec3(0,0,1) * tangentBasis );

    lightVec = gl_LightSource[0].position.xyz * tangentBasis;
 #if SHINING
    halfVec = gl_LightSource[0].halfVector.xyz * tangentBasis;
    //eyeVec *= tangentBasis;
 #endif // no shining
#endif // NORMAL_MAPPING == 1

#endif // BONES_COUNT >= 1

#if SHINING
    //eyeVec = normalize( eyeVec );
#endif // no shining
  #if FOG
    //distance = -gl_Position.z; <- junk with z smaller than in eye vec
    //distance = -eyeVec.z; // plane parallel to screen
    //distance = clamp( length( eyeVec ), 0.0, 2000.0 );
    // clamping in vertex shader is not correct (especially for large triangles)
    //gl_FogCoord - when glFogCoord() used for per-vertex fog coord,
    //i.e. volumetric fog
  #endif // no fog
}
