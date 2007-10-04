// -*-c++-*-

# ifndef __GLSL_CG_DATA_TYPES // the space after '#' is necessary to
                              // differentiate `sed' defines from GLSL one's
  // remove half float types on non-nVidia videocards
  # define half    float
  # define half2   vec2
  # define half3   vec3
  # define half4   vec4
  # define half3x3 mat3
# endif

#if TEXTURING == 1
uniform sampler2D decalMap;
#endif

#if NORMAL_MAPPING == 1
uniform sampler2D normalMap;
#endif

#if BUMP_MAPPING == 1
uniform sampler2D bumpMap;
uniform half      bumpMapAmount;
#endif

#if SHINING
//varying vec3 eyeVec;//phong
#endif

#if NORMAL_MAPPING == 1 || BUMP_MAPPING == 1
varying half3x3 eyeBasis; // in tangent space
#else
varying half3 transformedNormal;
#endif

uniform half face;
uniform half glossiness;

void main()
{
    // -- Calculate normal --
//    face = gl_FrontFacing ? 1.0 : -1.0;
    // two-sided lighting
    // ATI doesn't know about gl_FrontFacing ???
    // it says that it unsupported language element
    // and shader will run in software
    // GeForce < 6.x also doesn't know about this.
#if NORMAL_MAPPING == 1 || BUMP_MAPPING == 1
    half2 ag = half2(0.0);
    #if NORMAL_MAPPING == 1
      ag += half(2.0)*(half2(texture2D(normalMap, gl_TexCoord[0].st).ag) - half(0.5));
    #endif
    #if BUMP_MAPPING == 1
      ag += bumpMapAmount * half(2.0)*(half2(texture2D(bumpMap, gl_TexCoord[0].st).rg) - half(0.5));
    #endif
    half3 normal = face*half3(ag, sqrt(half(1.0) - dot( ag, ag )));
//    vec3 normal = face*normalize(2.0 * (texture2D(normalMap, gl_TexCoord[0].st).rgb - 0.5));
    normal = normalize( normal * eyeBasis );
///    normal = normalize( vec3( eyeBasis[0][2], eyeBasis[1][2], eyeBasis[2][2] ) );
//     normal = normalize( normal * mat3( normalize( eyeBasis[0] ),
//                                        normalize( eyeBasis[1] ),
//                                        normalize( eyeBasis[2] ) ) );
    // ^ not much difference
#else        
    half3 normal = face*normalize(transformedNormal);
#endif

    // -- Calculate decal (texture) color --
// #if RGBA == 1
//     #define vec3 vec4 <- this is slower than decalColor4...
//     #define rgb  rgba <-
// #endif
    
#if TEXTURING == 1
  #if RGBA == 1
    half4 decalColor4 = half4(texture2D(decalMap, gl_TexCoord[0].st).rgba);
    half3 decalColor = decalColor4.rgb;
  #else
    half3 decalColor = half3(texture2D(decalMap, gl_TexCoord[0].st).rgb);
  #endif
#endif

    // -- Global Ambient --
    half3 globalAmbient = half3(gl_FrontMaterial.ambient.rgb * gl_LightModel.ambient.rgb);

    half3 color = half3(globalAmbient);

    // -- Lights ambient --
    half3 ambient0 = half3(gl_FrontMaterial.ambient.rgb * gl_LightSource[0].ambient.rgb);
    color += ambient0;

//     vec3 ambient1 = gl_FrontMaterial.ambient.rgb * gl_LightSource[1].ambient.rgb;
//     color += ambient1;

    // -- Lights diffuse --
    half3 lightDir0 = half3(gl_LightSource[0].position.xyz);
    half  NdotL0 = max( half(0.0), dot( normal, lightDir0 ) );
    //NdotL0 = NdotL0 > half(0.4) ? half(0.8) : half(0.5);
       //0.2 * floor( NdotL0 * 4.0 ) / 4.0 + 0.8 * NdotL0; // cartoon, need play with coeffs
    half3 diffuse0 = half3(gl_FrontMaterial.diffuse.rgb * gl_LightSource[0].diffuse.rgb);
    color += NdotL0 * diffuse0;

//     vec3 lightDir1 = gl_LightSource[1].position.xyz;
//     float NdotL1 = max(0.0, dot( normal, lightDir1 ) );
//     vec3 diffuse1 = gl_FrontMaterial.diffuse.rgb * gl_LightSource[1].diffuse.rgb;
//     color += NdotL1 * diffuse1;

    // -- Apply decal --
#if TEXTURING == 1
    color *= decalColor;
#endif

    // -- Specular --
#if SHINING == 1
//         vec3 R = reflect( -lightDir, normal );
//         float NdotHV = dot( R, normalize(-eyeVec) );
    half NdotHV0 = dot( normal, half3(gl_LightSource[0].halfVector.xyz) );
    // remark that for correct calculations with big glossines (and
    // therefore small normal variance) we need float normals
    // calculations instead of half, but with it we also need float
    // eyeBasis and eat more GPU resources, so we leave half precision
    // cacluations for the moment.
    // why `pow(RdotE_phong, s) = pow(NdotHV_blinn, 4*s)' ??? 
    if ( NdotHV0 > half(0.0) ) // faster than use max(0,...) by 5% (at least on normal mapped)
        // I don't see difference if we remove this if
    {
        half3 specular0 = half3(gl_FrontMaterial.specular.rgb * gl_LightSource[0].specular.rgb) *
            pow( NdotHV0, glossiness );
        color += specular0;
    }

//     float NdotHV1 = dot( normal, gl_LightSource[1].halfVector.xyz );
//     if ( NdotHV1 > 0.0 )
//     {
//         vec3 specular1 = gl_FrontMaterial.specular.rgb * gl_LightSource[1].specular.rgb * 
//             pow( NdotHV1, glossiness );
//         color += specular1;
//     }
#endif // SHINING

#if OPACITY
    half opacity = half(gl_FrontMaterial.diffuse.a);
  #if RGBA == 1
    half4 fragColor = half4(color, opacity*decalColor4.a);
  #else
    half4 fragColor = half4(color, opacity);
  #endif
#else
  #if RGBA == 1
    half4 fragColor = half4(color, decalColor4.a);
  #else
    half4 fragColor = half4(color, 1.0);
  #endif
#endif

#if FOG
//     float fog = (gl_Fog.end - gl_FogFragCoord) * gl_Fog.scale;
//     // GL_FOG_MODE = GL_LINEAR

    half fog = half(exp(-gl_Fog.density * gl_FogFragCoord));
    // GL_FOG_MODE = GL_EXP

//     float fog = exp(-gl_Fog.density * gl_Fog.density *
//                     gl_FogFragCoord * gl_FogFragCoord);
//     // GL_FOG_MODE = GL_EXP2

    fog = clamp( fog, 0.0, 1.0 );
    
    fragColor = mix( half4(gl_Fog.color), fragColor, fog );
#endif // FOG
//    gl_FragDepth = gl_FragCoord.z;

    gl_FragColor = vec4(fragColor);
}
