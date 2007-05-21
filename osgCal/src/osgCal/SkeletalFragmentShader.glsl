// -*-c++-*-

#if TEXTURING == 1
uniform sampler2D decalMap;
#endif

#if NORMAL_MAPPING == 1
uniform sampler2D normalMap;

varying mat3 eyeBasis; // in tangent space
#endif

#if SHINING
//varying vec3 eyeVec;//phong
#endif

#if TEXTURING == 1 || NORMAL_MAPPING == 1
varying vec2 texUV;
#endif

#ifndef NORMAL_MAPPING
varying vec3 transformedNormal;
#endif

uniform float face;

void main()
{
    // -- Calculate normal --
//    face = gl_FrontFacing ? 1.0 : -1.0;
    // two-sided lighting
    // ATI doesn't know about gl_FrontFacing ???
    // it says that it unsupported language element
    // and shader will run in software
    // GeForce < 6.x also doesn't know about this.
#if NORMAL_MAPPING == 1
    vec2 ag = 2.0*(texture2D(normalMap, texUV).ag - 0.5);
    vec3 normal = face*vec3(ag, sqrt(1.0 - dot( ag, ag )));
//    vec3 normal = face*normalize(2.0 * (texture2D(normalMap, texUV).rgb - 0.5));
    normal = normalize( normal * eyeBasis );
//     normal = normalize( normal * mat3( normalize( eyeBasis[0] ),
//                                        normalize( eyeBasis[1] ),
//                                        normalize( eyeBasis[2] ) ) );
    // ^ not much difference
#else        
    vec3 normal = face*normalize(transformedNormal);
#endif

    // -- Calculate decal (texture) color --
// #if RGBA == 1
//     #define vec3 vec4 <- this is slower than decalColor4...
//     #define rgb  rgba <-
// #endif
    
#if TEXTURING == 1
  #if RGBA == 1
    vec4 decalColor4 = texture2D(decalMap, texUV).rgba;
    vec3 decalColor = decalColor4.rgb;
  #else
    vec3 decalColor = texture2D(decalMap, texUV).rgb;
  #endif
#endif

    // -- Global Ambient --
    vec3 globalAmbient = gl_FrontMaterial.ambient.rgb * gl_LightModel.ambient.rgb;

    vec3 color = globalAmbient;

    // -- Lights ambient --
    vec3 ambient0 = gl_FrontMaterial.ambient.rgb * gl_LightSource[0].ambient.rgb;
    color += ambient0;

//     vec3 ambient1 = gl_FrontMaterial.ambient.rgb * gl_LightSource[1].ambient.rgb;
//     color += ambient1;

    // -- Lights diffuse --
    vec3 lightDir0 = gl_LightSource[0].position.xyz;
    float NdotL0 = max(0.0, dot( normal, lightDir0 ) );
    vec3 diffuse0 = gl_FrontMaterial.diffuse.rgb * gl_LightSource[0].diffuse.rgb;
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
    //vec3 H = lightDir + normalize(-eyeVec); // per-pixel half vector - very slow
    float NdotHV0 = dot( normal, gl_LightSource[0].halfVector.xyz );
    // why `pow(RdotE_phong, s) = pow(NdotHV_blinn, 4*s)' ??? 
    if ( NdotHV0 > 0.0 ) // faster than use max(0,...) by 5% (at least on normal mapped)
        // I don't see difference if we remove this if
    {
        vec3 specular0 = gl_FrontMaterial.specular.rgb * gl_LightSource[0].specular.rgb * 
            pow( NdotHV0, gl_FrontMaterial.shininess );
        color += specular0;
    }

//     float NdotHV1 = dot( normal, gl_LightSource[1].halfVector.xyz );
//     if ( NdotHV1 > 0.0 )
//     {
//         vec3 specular1 = gl_FrontMaterial.specular.rgb * gl_LightSource[1].specular.rgb * 
//             pow( NdotHV1, gl_FrontMaterial.shininess );
//         color += specular1;
//     }
#endif // SHINING

#if OPACITY
    float opacity = gl_FrontMaterial.diffuse.a;
  #if RGBA == 1
    gl_FragColor = /*vec4(color.rgb, opacity*color.a);/*/vec4(color, opacity*decalColor4.a);
  #else
    gl_FragColor = vec4(color, opacity);
  #endif
#else
  #if RGBA == 1
    gl_FragColor = /*color;/*/vec4(color, decalColor4.a);
  #else
    gl_FragColor = vec4(color, 1.0);
  #endif
#endif

#if FOG
//     float fog = (gl_Fog.end - gl_FogFragCoord) * gl_Fog.scale;
//     // GL_FOG_MODE = GL_LINEAR

    float fog = exp(-gl_Fog.density * gl_FogFragCoord);
    // GL_FOG_MODE = GL_EXP

//     float fog = exp(-gl_Fog.density * gl_Fog.density *
//                     gl_FogFragCoord * gl_FogFragCoord);
//     // GL_FOG_MODE = GL_EXP2

    fog = clamp( fog, 0.0, 1.0 );
    
    gl_FragColor = mix( gl_Fog.color, gl_FragColor, fog );
#endif // FOG
}
