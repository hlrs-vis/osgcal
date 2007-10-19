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
shaderText += "# endif\n";
shaderText += "\n";
if ( TEXTURING == 1 ) {
shaderText += "uniform sampler2D decalMap;\n";
}
shaderText += "\n";
if ( NORMAL_MAPPING == 1 ) {
shaderText += "uniform sampler2D normalMap;\n";
}
shaderText += "\n";
if ( BUMP_MAPPING == 1 ) {
shaderText += "uniform sampler2D bumpMap;\n";
shaderText += "uniform half      bumpMapAmount;\n";
}
shaderText += "\n";
if ( NORMAL_MAPPING == 1 || BUMP_MAPPING == 1 ) {
shaderText += "varying mat3 eyeBasis; // in tangent space\n";
} else {
shaderText += "varying vec3 transformedNormal;\n";
}
shaderText += "\n";
if ( !GL_FRONT_FACING ) {
shaderText += "uniform half face;\n";
}
shaderText += "uniform float glossiness;\n";
shaderText += "\n";
if ( FOG ) {
shaderText += "varying vec3 eyeVec;\n";
}
shaderText += "\n";
shaderText += "void main()\n";
shaderText += "{\n";
shaderText += "    // -- Calculate normal --\n";
if ( GL_FRONT_FACING == 1 ) {
shaderText += "    half face = gl_FrontFacing ? half(1.0) : half(-1.0);\n";
shaderText += "    // two-sided lighting\n";
shaderText += "    // ATI doesn't know about gl_FrontFacing ???\n";
shaderText += "    // it says that it unsupported language element\n";
shaderText += "    // and shader will run in software\n";
shaderText += "    // GeForce < 6.x also doesn't know about this.\n";
}
if ( NORMAL_MAPPING == 1 || BUMP_MAPPING == 1 ) {
shaderText += "    half2 ag = half2(0.0);\n";
    if ( NORMAL_MAPPING == 1 ) {
shaderText += "      ag += half(2.0)*(half2(texture2D(normalMap, gl_TexCoord[0].st).ag) - half(0.5));\n";
    }
    if ( BUMP_MAPPING == 1 ) {
shaderText += "       ag += bumpMapAmount * half(2.0)*(half2(texture2D(bumpMap, gl_TexCoord[0].st).ag) - half(0.5));\n";
    }
shaderText += "    half3 hnormal = face*half3(ag, sqrt(half(1.0) - dot( ag, ag )));\n";
shaderText += "    vec3 normal = normalize( vec3(hnormal) * eyeBasis );\n";
shaderText += "//     normal = normalize( normal * mat3( normalize( eyeBasis[0] ),\n";
shaderText += "//                                        normalize( eyeBasis[1] ),\n";
shaderText += "//                                        normalize( eyeBasis[2] ) ) );\n";
shaderText += "    // ^ not much difference\n";
} else {
shaderText += "    vec3 normal = face*normalize(transformedNormal);\n";
shaderText += "    // Remark that we calculate lighting (normals) with full precision\n";
shaderText += "    // but colors only with half one.\n";
shaderText += "    // We previously calculated lighting in half precision too, but it gives us\n";
shaderText += "    // precision errors on meshes with high glossiness, so we reverted to full precision.\n";
}
shaderText += "\n";
shaderText += "    // -- Calculate decal (texture) color --\n";
if ( TEXTURING == 1 ) {
  if ( RGBA == 1 ) {
shaderText += "    half4 decalColor4 = half4(texture2D(decalMap, gl_TexCoord[0].st).rgba);\n";
shaderText += "    half3 decalColor = decalColor4.rgb;\n";
  } else {
shaderText += "    half3 decalColor = half3(texture2D(decalMap, gl_TexCoord[0].st).rgb);\n";
  }
}
shaderText += "\n";
shaderText += "    // -- Global Ambient --\n";
shaderText += "    half3 globalAmbient = half3(gl_FrontMaterial.ambient.rgb * gl_LightModel.ambient.rgb);\n";
shaderText += "\n";
shaderText += "    half3 color = half3(globalAmbient);\n";
shaderText += "\n";
shaderText += "    float i = 0.0;\n";
shaderText += "     \n";
shaderText += "    // -- Lights ambient --\n";
shaderText += "    half3 ambient = half3(gl_FrontMaterial.ambient.rgb * gl_LightSource[int(i)].ambient.rgb);\n";
shaderText += "    color += ambient;\n";
shaderText += "\n";
shaderText += "    // -- Lights diffuse --\n";
shaderText += "    vec3 lightDir = gl_LightSource[int(i)].position.xyz;\n";
shaderText += "    half  NdotL = max( half(0.0), half(dot( normal, lightDir )) );\n";
shaderText += "    half3 diffuse = half3(gl_FrontMaterial.diffuse.rgb * gl_LightSource[int(i)].diffuse.rgb);\n";
shaderText += "    color += NdotL * diffuse;\n";
shaderText += "\n";
shaderText += "    // -- Apply decal --\n";
if ( TEXTURING == 1 ) {
shaderText += "    color *= decalColor;\n";
}
shaderText += "\n";
shaderText += "    // -- Specular --\n";
if ( SHINING == 1 ) {
shaderText += "    float NdotHV = dot( normal, gl_LightSource[int(i)].halfVector.xyz );\n";
shaderText += "    if ( NdotHV > 0.0 ) // faster than use max(0,...) by 5% (at least on normal mapped)\n";
shaderText += "        // I don't see difference if we remove this if\n";
shaderText += "    {\n";
shaderText += "        half3 specular = half3(gl_FrontMaterial.specular.rgb * gl_LightSource[int(i)].specular.rgb) *\n";
shaderText += "            half(pow( NdotHV, glossiness ));\n";
shaderText += "        color += specular;\n";
shaderText += "    }\n";
} // SHINING
shaderText += "\n";
if ( OPACITY ) {
shaderText += "    half opacity = half(gl_FrontMaterial.diffuse.a);\n";
  if ( RGBA == 1 ) {
shaderText += "    half4 fragColor = half4(color, opacity*decalColor4.a);\n";
  } else {
shaderText += "    half4 fragColor = half4(color, opacity);\n";
  }
} else {
  if ( RGBA == 1 ) {
shaderText += "    half4 fragColor = half4(color, decalColor4.a);\n";
  } else {
shaderText += "    half4 fragColor = half4(color, 1.0);\n";
  }
}
shaderText += "\n";
if ( FOG ) {
shaderText += "    float fogFragCoord = length( eyeVec );\n";
shaderText += "\n";
if ( FOG_MODE == SHADER_FLAG_FOG_MODE_LINEAR ) {
shaderText += "    half fog = half((gl_Fog.end - fogFragCoord) * gl_Fog.scale);\n";
    }
shaderText += "\n";
    if ( FOG_MODE == SHADER_FLAG_FOG_MODE_EXP ) {
shaderText += "    half fog = half(exp(-gl_Fog.density * fogFragCoord));\n";
    }
shaderText += "\n";
    if ( FOG_MODE == SHADER_FLAG_FOG_MODE_EXP2 ) {
shaderText += "    half fog = half(exp(-gl_Fog.density * gl_Fog.density *\n";
shaderText += "                        fogFragCoord * fogFragCoord));\n";
    }
shaderText += "\n";
shaderText += "    fog = clamp( fog, 0.0, 1.0 );\n";
shaderText += "    \n";
shaderText += "    fragColor = mix( half4(gl_Fog.color), fragColor, fog );\n";
} // FOG
shaderText += "\n";
shaderText += "    gl_FragColor = vec4(fragColor);\n";
shaderText += "}\n";
