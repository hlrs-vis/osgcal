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

#if FOG
varying vec3 eyeVec;
#endif

void main()
{
#if TEXTURING == 1 || NORMAL_MAPPING == 1 || BUMP_MAPPING == 1
    gl_TexCoord[0].st = gl_MultiTexCoord0.st; // export texCoord to fragment shader
#endif

#if BONES_COUNT >= 1
    mat3 totalRotation = weight.x * rotationMatrices[int(index.x)];
    #if !DONT_CALCULATE_VERTEX
    vec3 totalTranslation = weight.x * translationVectors[int(index.x)];
    #endif

#if BONES_COUNT >= 2
    totalRotation += weight.y * rotationMatrices[int(index.y)];
    #if !DONT_CALCULATE_VERTEX
    totalTranslation += weight.y * translationVectors[int(index.y)];
    #endif

#if BONES_COUNT >= 3
    totalRotation += weight.z * rotationMatrices[int(index.z)];
    #if !DONT_CALCULATE_VERTEX
    totalTranslation += weight.z * translationVectors[int(index.z)];
    #endif

#if BONES_COUNT >= 4
    totalRotation += weight.w * rotationMatrices[int(index.w)];
    #if !DONT_CALCULATE_VERTEX
    totalTranslation += weight.w * translationVectors[int(index.w)];
    #endif
#endif // BONES_COUNT >= 4
#endif // BONES_COUNT >= 3
#endif // BONES_COUNT >= 2

  #if !DONT_CALCULATE_VERTEX
    vec3 transformedPosition = totalRotation * gl_Vertex.xyz + totalTranslation;
    gl_Position = gl_ModelViewProjectionMatrix * vec4(transformedPosition, 1.0);
  #else
    gl_Position = ftransform();
  #endif

#if FOG
    eyeVec = (gl_ModelViewMatrix * vec4(transformedPosition, 1.0)).xyz;
#endif // no fog

#if NORMAL_MAPPING == 1 || BUMP_MAPPING == 1
    vec3 t = gl_NormalMatrix * (totalRotation * tangent);
    vec3 b = gl_NormalMatrix * (totalRotation * binormal);
    vec3 n = gl_NormalMatrix * (totalRotation * gl_Normal);
    // vec3 b = cross( n, t );
    // ^ does'n work for some of our meshes (no handedness)
    // TODO: maybe save handedness in tangent alpha and remove binormals?:
    //    vec3 b = cross( n, t ) * tangent.a;

    eyeBasis = mat3( t[0], b[0], n[0],
                     t[1], b[1], n[1],
                     t[2], b[2], n[2] );
#else // NORMAL_MAPPING == 1
    transformedNormal = gl_NormalMatrix * (totalRotation * gl_Normal);
#endif // NORMAL_MAPPING == 1

#else // no bones

    // dont touch anything when no bones influence mesh
    gl_Position = ftransform();
#if FOG
    eyeVec = (gl_ModelViewMatrix * gl_Vertex).xyz;
#endif // no fog

#if NORMAL_MAPPING == 1 || BUMP_MAPPING == 1
    vec3 t = gl_NormalMatrix * tangent;
    vec3 b = gl_NormalMatrix * binormal;
    vec3 n = gl_NormalMatrix * gl_Normal;

    eyeBasis = mat3( t[0], b[0], n[0],
                     t[1], b[1], n[1],
                     t[2], b[2], n[2] );
#else // NORMAL_MAPPING == 1
    transformedNormal = gl_NormalMatrix * gl_Normal;
#endif // NORMAL_MAPPING == 1

#endif // BONES_COUNT >= 1
}
