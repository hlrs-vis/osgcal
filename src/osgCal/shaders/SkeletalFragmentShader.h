shaderText += "// -*-c++-*-\n";
shaderText += "\n";
if ( TEXTURING == 1 ) {
shaderText += "uniform sampler2D decalMap;\n";
}
shaderText += "\n";
if ( NORMAL_MAPPING == 1 ) {
shaderText += "uniform sampler2D normalMap;\n";
shaderText += "\n";
shaderText += "varying mat3 eyeBasis; // in tangent space\n";
}
shaderText += "\n";
if ( SHINING ) {
shaderText += "//varying vec3 eyeVec;//phong\n";
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
shaderText += "uniform float face;\n";
shaderText += "\n";
shaderText += "void main()\n";
shaderText += "{\n";
shaderText += "    // -- Calculate normal --\n";
shaderText += "//    face = gl_FrontFacing ? 1.0 : -1.0;\n";
shaderText += "    // two-sided lighting\n";
shaderText += "    // ATI doesn't know about gl_FrontFacing ???\n";
shaderText += "    // it says that it unsupported language element\n";
shaderText += "    // and shader will run in software\n";
shaderText += "    // GeForce < 6.x also doesn't know about this.\n";
if ( NORMAL_MAPPING == 1 ) {
shaderText += "    vec2 ag = 2.0*(texture2D(normalMap, texUV).ag - 0.5);\n";
shaderText += "    vec3 normal = face*vec3(ag, sqrt(1.0 - dot( ag, ag )));\n";
shaderText += "//    vec3 normal = face*normalize(2.0 * (texture2D(normalMap, texUV).rgb - 0.5));\n";
shaderText += "    normal = normalize( normal * eyeBasis );\n";
shaderText += "//     normal = normalize( normal * mat3( normalize( eyeBasis[0] ),\n";
shaderText += "//                                        normalize( eyeBasis[1] ),\n";
shaderText += "//                                        normalize( eyeBasis[2] ) ) );\n";
shaderText += "    // ^ not much difference\n";
} else {        
shaderText += "    vec3 normal = face*normalize(transformedNormal);\n";
}
shaderText += "\n";
shaderText += "    // -- Calculate decal (texture) color --\n";
// if ( RGBA == 1 ) {
//     shaderText += "#define vec3 vec4 <- this is slower than decalColor4...\n";
//     shaderText += "#define rgb  rgba <-\n";
// }
shaderText += "    \n";
if ( TEXTURING == 1 ) {
  if ( RGBA == 1 ) {
shaderText += "    vec4 decalColor4 = texture2D(decalMap, texUV).rgba;\n";
shaderText += "    vec3 decalColor = decalColor4.rgb;\n";
  } else {
shaderText += "    vec3 decalColor = texture2D(decalMap, texUV).rgb;\n";
  }
}
shaderText += "\n";
shaderText += "    // -- Global Ambient --\n";
shaderText += "    vec3 globalAmbient = gl_FrontMaterial.ambient.rgb * gl_LightModel.ambient.rgb;\n";
shaderText += "\n";
shaderText += "    vec3 color = globalAmbient;\n";
shaderText += "\n";
shaderText += "    // -- Lights ambient --\n";
shaderText += "    vec3 ambient0 = gl_FrontMaterial.ambient.rgb * gl_LightSource[0].ambient.rgb;\n";
shaderText += "    color += ambient0;\n";
shaderText += "\n";
shaderText += "//     vec3 ambient1 = gl_FrontMaterial.ambient.rgb * gl_LightSource[1].ambient.rgb;\n";
shaderText += "//     color += ambient1;\n";
shaderText += "\n";
shaderText += "    // -- Lights diffuse --\n";
shaderText += "    vec3 lightDir0 = gl_LightSource[0].position.xyz;\n";
shaderText += "    float NdotL0 = max(0.0, dot( normal, lightDir0 ) );\n";
shaderText += "    vec3 diffuse0 = gl_FrontMaterial.diffuse.rgb * gl_LightSource[0].diffuse.rgb;\n";
shaderText += "    color += NdotL0 * diffuse0;\n";
shaderText += "\n";
shaderText += "//     vec3 lightDir1 = gl_LightSource[1].position.xyz;\n";
shaderText += "//     float NdotL1 = max(0.0, dot( normal, lightDir1 ) );\n";
shaderText += "//     vec3 diffuse1 = gl_FrontMaterial.diffuse.rgb * gl_LightSource[1].diffuse.rgb;\n";
shaderText += "//     color += NdotL1 * diffuse1;\n";
shaderText += "\n";
shaderText += "    // -- Apply decal --\n";
if ( TEXTURING == 1 ) {
shaderText += "    color *= decalColor;\n";
}
shaderText += "\n";
shaderText += "    // -- Specular --\n";
if ( SHINING == 1 ) {
shaderText += "//         vec3 R = reflect( -lightDir, normal );\n";
shaderText += "//         float NdotHV = dot( R, normalize(-eyeVec) );\n";
shaderText += "    //vec3 H = lightDir + normalize(-eyeVec); // per-pixel half vector - very slow\n";
shaderText += "    float NdotHV0 = dot( normal, gl_LightSource[0].halfVector.xyz );\n";
shaderText += "    // why `pow(RdotE_phong, s) = pow(NdotHV_blinn, 4*s)' ??? \n";
shaderText += "    if ( NdotHV0 > 0.0 ) // faster than use max(0,...) by 5% (at least on normal mapped)\n";
shaderText += "        // I don't see difference if we remove this if\n";
shaderText += "    {\n";
shaderText += "        vec3 specular0 = gl_FrontMaterial.specular.rgb * gl_LightSource[0].specular.rgb * \n";
shaderText += "            pow( NdotHV0, gl_FrontMaterial.shininess );\n";
shaderText += "        color += specular0;\n";
shaderText += "    }\n";
shaderText += "\n";
shaderText += "//     float NdotHV1 = dot( normal, gl_LightSource[1].halfVector.xyz );\n";
shaderText += "//     if ( NdotHV1 > 0.0 )\n";
shaderText += "//     {\n";
shaderText += "//         vec3 specular1 = gl_FrontMaterial.specular.rgb * gl_LightSource[1].specular.rgb * \n";
shaderText += "//             pow( NdotHV1, gl_FrontMaterial.shininess );\n";
shaderText += "//         color += specular1;\n";
shaderText += "//     }\n";
} // SHINING
shaderText += "\n";
if ( OPACITY ) {
shaderText += "    float opacity = gl_FrontMaterial.diffuse.a;\n";
  if ( RGBA == 1 ) {
shaderText += "    gl_FragColor = /*vec4(color.rgb, opacity*color.a);/*/vec4(color, opacity*decalColor4.a);\n";
  } else {
shaderText += "    gl_FragColor = vec4(color, opacity);\n";
  }
} else {
  if ( RGBA == 1 ) {
shaderText += "    gl_FragColor = /*color;/*/vec4(color, decalColor4.a);\n";
  } else {
shaderText += "    gl_FragColor = vec4(color, 1.0);\n";
  }
}
shaderText += "\n";
if ( FOG ) {
shaderText += "//     float fog = (gl_Fog.end - gl_FogFragCoord) * gl_Fog.scale;\n";
shaderText += "//     // GL_FOG_MODE = GL_LINEAR\n";
shaderText += "\n";
shaderText += "    float fog = exp(-gl_Fog.density * gl_FogFragCoord);\n";
shaderText += "    // GL_FOG_MODE = GL_EXP\n";
shaderText += "\n";
shaderText += "//     float fog = exp(-gl_Fog.density * gl_Fog.density *\n";
shaderText += "//                     gl_FogFragCoord * gl_FogFragCoord);\n";
shaderText += "//     // GL_FOG_MODE = GL_EXP2\n";
shaderText += "\n";
shaderText += "    fog = clamp( fog, 0.0, 1.0 );\n";
shaderText += "    \n";
shaderText += "    gl_FragColor = mix( gl_Fog.color, gl_FragColor, fog );\n";
} // FOG
shaderText += "//    gl_FragDepth = gl_FragCoord.z;\n";
shaderText += "}\n";
