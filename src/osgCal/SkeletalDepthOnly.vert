// -*-c++-*-

#if BONES_COUNT >= 1
# define weight gl_MultiTexCoord1
# define index  gl_MultiTexCoord2

uniform mat3 rotationMatrices[31];
uniform vec3 translationVectors[31];
#endif
void main()
{
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

#else // no bones

    // dont touch anything when no bones influence mesh
    gl_Position = ftransform();
#endif // BONES_COUNT >= 1
}
