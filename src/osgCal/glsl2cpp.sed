/#/!s/.*/shaderText += "\0\\n";/;
s/ *# .*/shaderText += "\0\\n";/;
s/#version .*/shaderText += "\0\\n";/;
s/#define .*/shaderText += "\0\\n";/;
s/#ifndef \(.*\)/if (!( \1 )) {/;
s/#if \(.*\)/if ( \1 ) {/;
s/#elseif \(.*\)/} else if ( \1 ) {/;
s/#else/} else {/;
s/#endif/}/;
