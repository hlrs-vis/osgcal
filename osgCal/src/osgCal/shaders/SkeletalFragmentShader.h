shaderText += "// -*-c++-*-\n";
shaderText += "\n";
if ( TEXTURING == 1 ) {
shaderText += "uniform sampler2D decalMap;\n";
}
shaderText += "\n";
if ( NORMAL_MAPPING == 1 ) {
shaderText += "uniform sampler2D normalMap;\n";
shaderText += "\n";
shaderText += "varying vec3 lightVec;\n";
if ( SHINING ) {
shaderText += "varying vec3 halfVec;//blinn\n";
}
}
shaderText += "\n";
if ( SHINING ) {
shaderText += "//varying vec3 eyeVec;\n";
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
shaderText += "    float NdotL = max(0.0, dot( normal, normalize(lightVec) ));\n";
} else {        
shaderText += "    vec3 normal = face*normalize(transformedNormal);\n";
shaderText += "    vec3 lightDir = normalize(vec3(gl_LightSource[0].position));\n";
shaderText += "    float NdotL = max(0.0, dot( normal, lightDir ));\n";
}
shaderText += "   \n";
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
shaderText += "    vec3 ambient = gl_FrontMaterial.ambient.rgb * gl_LightSource[0].ambient.rgb;\n";
shaderText += "    vec3 globalAmbient = gl_FrontMaterial.ambient.rgb * gl_LightModel.ambient.rgb;\n";
shaderText += "    // there is no performance gain when ambient and diffuse are made uniform, it seems that\n";
shaderText += "    // driver itself determine that these values are constant and doesn't need to recalculate\n";
shaderText += "    // for each pixel\n";
shaderText += "\n";
shaderText += "    vec3 color = ambient + globalAmbient;\n";
shaderText += "\n";
shaderText += "//    if ( NdotL > 0.0 ) // slower than use max(0,...) by 11% (maybe else branch is slow?)\n";
shaderText += "    {\n";
shaderText += "        vec3 diffuse = gl_FrontMaterial.diffuse.rgb * gl_LightSource[0].diffuse.rgb;\n";
shaderText += "        color += NdotL * diffuse;\n";
if ( TEXTURING == 1 ) {
shaderText += "        color *= decalColor;\n";
}
shaderText += "\n";
if ( SHINING == 1 ) {
if ( NORMAL_MAPPING == 1 ) {
shaderText += "//         vec3 R = reflect( -normalize(lightVec), normal );\n";
shaderText += "//         float NdotHV = dot( R, normalize(-eyeVec) ); // phong R*E\n";
shaderText += "        //vec3 H = normalize(lightVec) + normalize(-eyeVec); // per-pixel half vector\n";
shaderText += "        // ^ H is even slower than phong\n";
shaderText += "        float NdotHV = dot( normal, normalize(/*H*/halfVec) ); // blinn  N*H\n";
} else {
shaderText += "//         vec3 R = reflect( -lightDir, normal );\n";
shaderText += "//         float NdotHV = dot( R, normalize(-eyeVec) );\n";
shaderText += "        //vec3 H = lightDir + normalize(-eyeVec); // per-pixel half vector\n";
shaderText += "        float NdotHV = dot( normal, normalize(/*H*/gl_LightSource[0].halfVector.xyz) );\n";
shaderText += "        // why `pow(RdotE_phong, s) = pow(NdotHV_blinn, 4*s)' ??? \n";
}
shaderText += "        if ( NdotHV > 0.0 ) // faster than use max(0,...) by 5% (at least on normal mapped)\n";
shaderText += "        // I don't see difference if we remove this if\n";
shaderText += "        {\n";
shaderText += "            vec3 specular = gl_FrontMaterial.specular.rgb * gl_LightSource[0].specular.rgb * \n";
shaderText += "				pow( NdotHV, gl_FrontMaterial.shininess );\n";
shaderText += "            color += specular;\n";
shaderText += "        }\n";
}
shaderText += "    }\n";
shaderText += "//    else\n";
shaderText += "//    {\n";
shaderText += "//        color *= decalColor;\n";
shaderText += "//    }\n";
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
shaderText += "}\n";
