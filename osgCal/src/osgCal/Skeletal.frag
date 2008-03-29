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

varying vec3 vNormal;
#if NORMAL_MAPPING == 1 || BUMP_MAPPING == 1
varying vec3 tangent;
varying vec3 binormal;
#endif

uniform float glossiness;

#if TWO_SIDED == 1
uniform float frontFacing;
#endif

#if FOG
varying vec3 eyeVec;
#endif

void main()
{
    // -- Calculate normal --
#if NORMAL_MAPPING == 1 || BUMP_MAPPING == 1
    half2 ag = half2(0.0);
    #if NORMAL_MAPPING == 1
      ag += half(2.0)*(half2(texture2D(normalMap, gl_TexCoord[0].st).ag) - half(0.5));
    #endif
    #if BUMP_MAPPING == 1
       ag += bumpMapAmount * half(2.0)*(half2(texture2D(bumpMap, gl_TexCoord[0].st).ag) - half(0.5));
    #endif
    half3 n = half3(ag, sqrt(half(1.0) - dot( ag, ag )));
    vec3 normal = normalize( n.x * tangent + n.y * binormal + n.z * vNormal );
#else
    vec3 normal = normalize(vNormal);
    // Remark that we calculate lighting (normals) with full precision
    // but colors only with half one.
    // We previously calculated lighting in half precision too, but it gives us
    // precision errors on meshes with high glossiness, so we reverted to full precision.
#endif

#if TWO_SIDED == 1
//    if ( !gl_FrontFacing ) // gl_FrontFacing is not always available,
    if ( frontFacing == 0.0 )
    {
        normal = -normal;
    }
#endif
    

#if TEXTURING == 1
    // -- Calculate decal (texture) color --
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

    # define i 0
#if 0
    // ^ Strange but `int i=0;' or `float i=0.0;' & `gl_LightSource[int(i)]'
    // doesn't work on R300 (radeon 9600).
    // It says that 'available number of constants exceeded' for
    // fragment shader (? ? ?).
    // If `float i = 0.0' & `gl_LightSource[i]' used (`i' w/o cast to
    // int) all works ok on ATI (? ? ?). NVidia warns that implicit cast
    // is there. So now we just define i to be zero. Loop for several
    // lights must be tested on 9600.
#endif

    // -- Lights ambient --
    half3 ambient = half3(gl_FrontMaterial.ambient.rgb * gl_LightSource[i].ambient.rgb);
    color += ambient;

    // -- Lights diffuse --
    vec3 lightDir = gl_LightSource[i].position.xyz;
    half  NdotL = max( half(0.0), half(dot( normal, lightDir )) );
    half3 diffuse = half3(gl_FrontMaterial.diffuse.rgb * gl_LightSource[i].diffuse.rgb);
    color += NdotL * diffuse;

#if TEXTURING == 1
    // -- Apply decal --
    color *= decalColor;
#endif

#if SHINING == 1
    // -- Specular --
    float NdotHV = dot( normal, gl_LightSource[i].halfVector.xyz );
    if ( NdotHV > 0.0 ) // faster than use max(0,...) by 5% (at least on normal mapped)
        // I don't see difference if we remove this if
    {
        half3 specular = half3(gl_FrontMaterial.specular.rgb * gl_LightSource[i].specular.rgb) *
            half(pow( NdotHV, glossiness ));
        color += specular;
    }
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
    float fogFragCoord = length( eyeVec );

    #if FOG_MODE == SHADER_FLAG_FOG_MODE_LINEAR
    half fog = half((gl_Fog.end - fogFragCoord) * gl_Fog.scale);
    #endif

    #if FOG_MODE == SHADER_FLAG_FOG_MODE_EXP
    half fog = half(exp(-gl_Fog.density * fogFragCoord));
    #endif

    #if FOG_MODE == SHADER_FLAG_FOG_MODE_EXP2
    half fog = half(exp(-gl_Fog.density * gl_Fog.density *
                        fogFragCoord * fogFragCoord));
    #endif

    fog = clamp( fog, 0.0, 1.0 );
    
    fragColor = mix( half4(gl_Fog.color), fragColor, fog );
#endif // FOG

    gl_FragColor = vec4(fragColor);
}
