shaderText += "// -*-c++-*-\n";
shaderText += "\n";
if ( BONES_COUNT >= 1 ) {
shaderText += "attribute vec4 weight;\n";
shaderText += "attribute ivec4 index; /* ivec gives small speedup 17.7 vs 17.9 msec\n";
shaderText += "                          (but gl_FrontFacing is not used with 17.9)\n";
shaderText += "                          but it's not compatible with ATI cards */\n";
shaderText += "\n";
shaderText += "uniform mat3 rotationMatrices[31];\n";
shaderText += "uniform vec3 translationVectors[31];\n";
}
shaderText += "void main()\n";
shaderText += "{\n";
if ( BONES_COUNT >= 1 ) {
shaderText += "    mat3 totalRotation = weight.x * rotationMatrices[int(index.x)];\n";
shaderText += "    vec3 totalTranslation = weight.x * translationVectors[int(index.x)];\n";
shaderText += "\n";
if ( BONES_COUNT >= 2 ) {
shaderText += "    totalRotation += weight.y * rotationMatrices[int(index.y)];\n";
shaderText += "    totalTranslation += weight.y * translationVectors[int(index.y)];\n";
shaderText += "\n";
if ( BONES_COUNT >= 3 ) {
shaderText += "    totalRotation += weight.z * rotationMatrices[int(index.z)];\n";
shaderText += "    totalTranslation += weight.z * translationVectors[int(index.z)];\n";
shaderText += "\n";
if ( BONES_COUNT >= 4 ) {
shaderText += "    totalRotation += weight.w * rotationMatrices[int(index.w)];\n";
shaderText += "    totalTranslation += weight.w * translationVectors[int(index.w)];\n";
} // BONES_COUNT >= 4
} // BONES_COUNT >= 3
} // BONES_COUNT >= 2
shaderText += "\n";
shaderText += "    vec3 transformedPosition = totalRotation * gl_Vertex.xyz + totalTranslation;\n";
shaderText += "    gl_Position = gl_ModelViewProjectionMatrix * vec4(transformedPosition, 1.0);\n";
shaderText += "\n";
} else { // no bones
shaderText += "\n";
shaderText += "    // dont touch anything when no bones influence mesh\n";
shaderText += "    gl_Position = ftransform();\n";
} // BONES_COUNT >= 1
shaderText += "}\n";
