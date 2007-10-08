// -*-c++-*-

#if BONES_COUNT >= 1
attribute vec4 weight;
attribute ivec4 index; /* ivec gives small speedup 17.7 vs 17.9 msec
                          (but gl_FrontFacing is not used with 17.9)
                          but it's not compatible with ATI cards */

uniform vec3 translationVectors[31];
#endif
void main()
{
#if BONES_COUNT >= 1
    vec3 totalTranslation = weight.x * translationVectors[int(index.x)];

#if BONES_COUNT >= 2
    totalTranslation += weight.y * translationVectors[int(index.y)];

#if BONES_COUNT >= 3
    totalTranslation += weight.z * translationVectors[int(index.z)];

#if BONES_COUNT >= 4
    totalTranslation += weight.w * translationVectors[int(index.w)];
#endif // BONES_COUNT >= 4
#endif // BONES_COUNT >= 3
#endif // BONES_COUNT >= 2

    gl_Position = gl_ModelViewProjectionMatrix * vec4(totalTranslation, 1.0);

#else // no bones

    // dont touch anything when no bones influence mesh
    gl_Position = ftransform();
#endif // BONES_COUNT >= 1
}
