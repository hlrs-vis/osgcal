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

#ifndef NORMAL_MAPPING
varying vec3 transformedNormal;
#endif

#if NORMAL_MAPPING == 1
varying mat3 eyeBasis; // in tangent space
#endif

#if SHINING
//varying vec3 eyeVec;//phong
#endif

void main()
{
#if TEXTURING == 1 || NORMAL_MAPPING == 1
    gl_TexCoord[0].st = texCoord; // export texCoord to fragment shader
#endif

#if BONES_COUNT >= 1
    mat3 totalRotation = weight.x * rotationMatrices[int(index.x)];
    vec3 totalTranslation = weight.x * translationVectors[int(index.x)];

#if BONES_COUNT >= 2
    totalRotation += weight.y * rotationMatrices[int(index.y)];
    totalTranslation += weight.y * translationVectors[int(index.y)];

#if BONES_COUNT >= 3
    totalRotation += weight.z * rotationMatrices[int(index.z)];
    totalTranslation += weight.z * translationVectors[int(index.z)];

#if BONES_COUNT >= 4
    totalRotation += weight.w * rotationMatrices[int(index.w)];
    totalTranslation += weight.w * translationVectors[int(index.w)];
#endif // BONES_COUNT >= 4
#endif // BONES_COUNT >= 3
#endif // BONES_COUNT >= 2

    vec3 transformedPosition = totalRotation * position + totalTranslation;
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
    mat3 tangentBasis =
        gl_NormalMatrix * totalRotation * mat3( tangent, binormal, normal );

    eyeBasis = mat3( tangentBasis[0][0], tangentBasis[1][0], tangentBasis[2][0],
                     tangentBasis[0][1], tangentBasis[1][1], tangentBasis[2][1],
                     tangentBasis[0][2], tangentBasis[1][2], tangentBasis[2][2] );
//     eyeBasis = mat3( vec3(1.0, 0.0, 0.0) * tangentBasis,
//                      vec3(0.0, 1.0, 0.0) * tangentBasis,
//                      vec3(0.0, 0.0, 1.0) * tangentBasis );
//     ^ this is simply matrix transposition

 #if SHINING
    //eyeVec *= tangentBasis;
 #endif // no shining
#else // NORMAL_MAPPING == 1
    transformedNormal = gl_NormalMatrix * (totalRotation * normal);
#endif // NORMAL_MAPPING == 1

#else // no bones

    // dont touch anything when no bones influence mesh
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
//     mat3 tangentBasis =
//         gl_NormalMatrix * mat3( tangent, binormal, normal );
//  ^ why this give us incorrect results?
    mat4 tangentBasis =
        gl_ModelViewMatrix * mat4( vec4( tangent, 0.0 ),
                                   vec4( binormal, 0.0 ),
                                   vec4( normal, 0.0 ),
                                   vec4( 0.0, 0.0, 0.0, 1.0 ) );

    eyeBasis = mat3( tangentBasis[0][0], tangentBasis[1][0], tangentBasis[2][0],
                     tangentBasis[0][1], tangentBasis[1][1], tangentBasis[2][1],
                     tangentBasis[0][2], tangentBasis[1][2], tangentBasis[2][2] );

 #if SHINING
    //eyeVec *= tangentBasis;
 #endif // no shining
#else // NORMAL_MAPPING == 1
    transformedNormal = gl_NormalMatrix * normal;
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
