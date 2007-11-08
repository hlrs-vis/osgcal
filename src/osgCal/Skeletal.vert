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
# define weight gl_MultiTexCoord1
# define index  gl_MultiTexCoord2

uniform mat3 rotationMatrices[31];
uniform vec3 translationVectors[31];
#endif

#if NORMAL_MAPPING == 1 || BUMP_MAPPING == 1
# define tangent  gl_MultiTexCoord3.xyz
# define binormal gl_MultiTexCoord4.xyz
varying mat3 eyeBasis; // in tangent space
#else
varying vec3 transformedNormal;
#endif

#if FOG
varying vec3 eyeVec;
#endif

//uniform bool clipPlanesUsed;

void main()
{
#if TEXTURING == 1 || NORMAL_MAPPING == 1 || BUMP_MAPPING == 1
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
    # ifdef __GLSL_CG_DATA_TYPES
//    if ( clipPlanesUsed )
    {
        gl_ClipVertex = gl_ModelViewMatrix * vec4(transformedPosition, 1.0);
    } 
    # endif
//  8.5 -- no clip planes
// 10.2 -- gl_ClipVertex always set (20% slowdown, on both 6600 and 8600)
// 10.8 -- if ( clipPlanesUsed /* == true */  ) gl_ClipVertex = ...
//  9.2 -- if ( clipPlanesUsed /* == false */ ) gl_ClipVertex = ...
// i.e.:
//    if ( clipPlanesUsed )  ~ 0.6-0.7 ms (uniform brancing is not optimized!)
//    gl_ClipVertex = ...    ~ 1.6 ms
// For not so hi-poly scenes gl_ClipVertex slowdown is 2-4%.
// gl_ClipVertex slowdown is independent on whether glClipPlane is
// used or not.
//
// For performance it's better to make separate shaders and determine
// at the draw time which to use. But we can get too many shaders
// (count of shaders is doubled with another option in vertex shader).
// And also shader with clip planes support must be also compiled at
// the GLObjectsVisitor stage (but can be not used).

// ATI doesn't support programmable clipping, gl_ClipVertex assignment
// in shaders throw it into software mode. ATI only support fixed
// function clipping, when ftransform() is used. I.e. we can't get
// correct clipping for our deformed meshes on ATI.
//
// R300/R400 ATI chips also doesn't support branching, so fragment
// shader culling can be very slow?

// Seems that we need separate shaders (with or w/o culling) and
// switch culling shader only when it's necessary (since it also kills
// speed-up at depth first pass). Also we can support only one
// clipping plane for better performance.
// To not make too many shaders when it not needed we may create
// separate USE_CLIP_PLANES_CULLING flag.
// `discard' in fragment shader is VERY slow. So for rigid meshes we
// need turn on hardware culling, also we need two paths one for
// NVidia with gl_ClipVertex used and one for ATI with discard.
// Maybe not support clipping of dynamic meshes on ATI at all?

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
    # ifdef __GLSL_CG_DATA_TYPES
//    if ( clipPlanesUsed )
    {
        gl_ClipVertex = gl_ModelViewMatrix * gl_Vertex;
    }
    # endif
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
