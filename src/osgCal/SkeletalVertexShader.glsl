// -*-c++-*-

# ifndef __GLSL_CG_DATA_TYPES // the space after '#' is necessary to
                              // differentiate `sed' defines from GLSL one's
  // remove half float types on non-nVidia videocards
  # define half    float
  # define half2   vec2
  # define half3   vec3
  # define half4   vec4
  # define half3x3 mat3
  # define ivec4   vec4
# endif

#if BONES_COUNT >= 1
attribute vec4 weight;
attribute ivec4 index; /* ivec gives small speedup 17.7 vs 17.9 msec
                          (but gl_FrontFacing is not used with 17.9)
                          but it's not compatible with ATI cards */

uniform mat3 rotationMatrices[31];
uniform vec3 translationVectors[31];
#endif

#if NORMAL_MAPPING == 1 || BUMP_MAPPING == 1
attribute vec3 tangent;
attribute vec3 binormal;
varying mat3 eyeBasis; // in tangent space
#else
varying vec3 transformedNormal;
#endif

#if SHINING
//varying vec3 eyeVec;//phong
#endif

void main()
{
#if TEXTURING == 1 || NORMAL_MAPPING == 1
    gl_TexCoord[0].st = gl_MultiTexCoord0.st; // export texCoord to fragment shader
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

    vec3 transformedPosition = totalRotation * gl_Vertex.xyz + totalTranslation;
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

#if NORMAL_MAPPING == 1 || BUMP_MAPPING == 1
    mat3 tangentBasis =
        gl_NormalMatrix * totalRotation * mat3( tangent, binormal, gl_Normal );

//    eyeBasis = transpose( tangentBasis );
    eyeBasis = mat3( tangentBasis[0][0], tangentBasis[1][0], tangentBasis[2][0],
                     tangentBasis[0][1], tangentBasis[1][1], tangentBasis[2][1],
                     tangentBasis[0][2], tangentBasis[1][2], tangentBasis[2][2] );

 #if SHINING
    //eyeVec *= tangentBasis;
 #endif // no shining
#else // NORMAL_MAPPING == 1
    transformedNormal = half3(gl_NormalMatrix * (totalRotation * gl_Normal));
#endif // NORMAL_MAPPING == 1

#else // no bones

    // dont touch anything when no bones influence mesh
    gl_Position = ftransform();
#if FOG && !SHINING
    //vec3 eyeVec = (gl_ModelViewMatrix * gl_Vertex).xyz;
#elseif SHINING
    //eyeVec = (gl_ModelViewMatrix * gl_Vertex).xyz;
#endif 
#if FOG
    vec3 eyeVec = (gl_ModelViewMatrix * gl_Vertex).xyz;
    gl_FogFragCoord = length( eyeVec );
#endif // no fog

#if NORMAL_MAPPING == 1 || BUMP_MAPPING == 1
//     mat3 tangentBasis =
//         gl_NormalMatrix * mat3( tangent, binormal, gl_Normal );
//  ^ why this give us incorrect results?
    mat4 tangentBasis =
        gl_ModelViewMatrix * mat4( vec4( tangent, 0.0 ),
                                   vec4( binormal, 0.0 ),
                                   vec4( gl_Normal, 0.0 ),
                                   vec4( 0.0, 0.0, 0.0, 1.0 ) );

//    eyeBasis = transpose( tangentBasis );
    eyeBasis = mat3( tangentBasis[0][0], tangentBasis[1][0], tangentBasis[2][0],
                     tangentBasis[0][1], tangentBasis[1][1], tangentBasis[2][1],
                     tangentBasis[0][2], tangentBasis[1][2], tangentBasis[2][2] );

 #if SHINING
    //eyeVec *= tangentBasis;
 #endif // no shining
#else // NORMAL_MAPPING == 1
    transformedNormal = half3(gl_NormalMatrix * gl_Normal);
#endif // NORMAL_MAPPING == 1

#endif // BONES_COUNT >= 1

#if SHINING
    //eyeVec = normalize( eyeVec );
#endif // no shining
}
