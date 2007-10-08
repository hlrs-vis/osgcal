/*
    Copyright (C) 2007 Vladimir Shabanov <vshabanoff@gmail.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/
#include <stdexcept>
#include <iostream>

#include <osg/Notify>
#include <osgDB/ReadFile>
#include <osgDB/WriteFile>
#include <osgDB/FileNameUtils>
#include <osg/Texture2D>

#ifdef __MINGW32__
#define _STDCALL_SUPPORTED 1 // <- otherwise DevIL thinks that we are
                             // real gcc on the real linux and we are
                             // don't support __stdcall
#endif

#include <IL/il.h>
#include <IL/ilu.h>

inline
double
colorToVector( unsigned char c )
{
    return ( static_cast< double >( c ) - 127 ) / 127;
//    return 2*( static_cast< double >( c ) / 255 - 0.5 );
}

inline
unsigned char
vectorToColor( double c )
{
    return static_cast< unsigned char >( round( c*127 + 127) );
//    return static_cast< unsigned char >( round( (c/2 + 0.5)*255 ) );
}

// Convert from GL_RGB8 to GL_LUMINANCE_ALPHA data format
void
makeA8L8fromRGB( int width,
                 int height,
                 const void* src,
                 void* dst )
{
    const char* s = (const char*) src;
    const char* sEnd = s + (width*height*3);
    char* d = (char*) dst;

    while ( s < sEnd )
    {
        // normalization of this type doesn't give
        // necessary results since video card use 
        // more than one mipmap to interpolate color, so
        // when color in one mipmap is overnormalized (white)
        // we get errors (that we don't get on scaled RGB textures).
//         double x = colorToVector( s[0] );
//         double y = colorToVector( s[1] );
//         double z = colorToVector( s[2] );

//         double length = sqrt( x*x + y*y + z*z );

//         if ( length > 0.0 )
//         {
//             // we normalize vectors as if we used normalize
//             // for samples taken from RGB texture
//             double scale = 1 / length;

//             d[0] = vectorToColor( y*scale );
//             d[1] = vectorToColor( x*scale );
//         }
//         else
//         {           
//             d[1] = s[0];
//             d[0] = s[1];
//         }
//
//         s += 3;
//         d += 2;

        d[1] = *s++; // x (r => a)
        d[0] = *s++; // y (g => lum)
        s++;         // z skipped
        
        d += 2;
    };
}

void
normalizeZ( int width,
            int height,
            void* data )
{
    unsigned char* d = (unsigned char*) data;
    unsigned char* dEnd = d + width*height*3;

    while ( d < dEnd )
    {
        // rescale Z coordinate
        double x = colorToVector( d[0] );
        double y = colorToVector( d[1] );

        d[2] = vectorToColor( sqrt( 1 - ( x*x + y*y ) ) );

        d += 3;
    }
}

void
grayToBlue( int width,
            int height,
            void* data )
{
    unsigned char* d = (unsigned char*) data;
    unsigned char* dEnd = d + width*height*3;

    while ( d < dEnd )
    {
        if (      d[0] == 127
             && ( d[1] == 127 || d[1] == 128 )
             &&   d[2] == 127 )
        {
            d[2] = 255;
        }
        d += 3;
    }
}

void
normalizeRGB( int width,
              int height,
              void* data )
{
    unsigned char* d = (unsigned char*) data;
    unsigned char* dEnd = d + width*height*3;

    while ( d < dEnd )
    {
        double x = colorToVector( d[0] );
        double y = colorToVector( d[1] );
        double z = colorToVector( d[2] );

        double length = sqrt( x*x + y*y + z*z );

        if ( length > 0.0 )
        {
            double scale = 1 / length;

            d[0] = vectorToColor( x*scale );
            d[1] = vectorToColor( y*scale );
            d[2] = vectorToColor( z*scale );
        }       

        d += 3;
    }
}

void
flip( int width,
      int height,
      void* data,
      int offset = 0,
      int pixelSize = 1 )
{
    unsigned char* s = (unsigned char*) data + offset;
    unsigned char* sEnd = s + (width*height*pixelSize);

    while ( s < sEnd ) { *s = 0xFF - *s; s += pixelSize; }
}

void flipX( int width,
            int height,
            void* a8l8 )
{
    flip( width, height, a8l8, 1, 2 );
}

void flipY( int width,
            int height,
            void* a8l8 )
{
    flip( width, height, a8l8, 0, 2 );
}

// The main function.
// Read; Scale; Convert to A8L8; Save (as dds)
void
toA8L8( const std::string& iFile,
        const std::string& oFile,
        int maxWidth = 4096,
        int maxHeight = 4096,
        bool flipx = false,
        bool flipy = false )
{
    // -- Load image --
    osg::ref_ptr< osg::Image > img = osgDB::readImageFile( iFile );

    if ( !img )
    {
        throw std::runtime_error( "Can't load " + iFile );
    }

    // -- Swap data if needed --
    if ( img->getPixelFormat() == GL_BGR )
    {
        //std::cout << "GL_BGR: swapping B and R\n";
        unsigned char* d = img->data();
        unsigned char* dEnd = d + img->s() * img->t() * 3;

        while ( d < dEnd )
        {
            unsigned char b = d[0];
            d[0] = d[2];
            d[2] = b;
            d += 3;
        }
    }
    else if ( img->getPixelFormat() != GL_RGB )
    {        
        throw std::runtime_error( "Unsupported image pixel format (GL_RBG or GL_BGR needed)" + iFile );
    }

//     unsigned char* d = img->data();
//     unsigned char* dEnd = d + img->s() * img->t() * 3;
//     while ( d < dEnd )
//     {
//         std::cout << (int)d[0] << '\t' << (int)d[1] << '\t' << (int)d[2] << '\n';
//         d += 3;
//     }
    
    // -- Calculate width & height --
    int widthP2  = std::min( osg::Image::computeNearestPowerOfTwo( img->s() ), maxWidth );
    int heightP2 = std::min( osg::Image::computeNearestPowerOfTwo( img->t() ), maxHeight );

    // -- Calculate dataSize, mipmapIndexes & allocate A8L8 data --
    int dataSize = widthP2*heightP2*2;

    osg::Image::MipmapDataType mipmapIndexes;

    for ( int w1 = widthP2/2, h1 = heightP2/2;
          !(w1 == 0 && h1 == 0); w1/=2, h1/=2)
    {
        if ( !w1 ) w1 = 1;
        if ( !h1 ) h1 = 1;
        mipmapIndexes.push_back( dataSize );
        dataSize += w1*h1*2;
    }

    void* a8l8data = malloc( dataSize );

    // -- Scale image & create mipmaps --
    //img->scaleImage( widthP2, heightP2, 1 );
    // scaleImage only works when the OpenGL is initialized
    // so we use DevIL for scaling
    ILuint imageName;
    
    ilGenImages( 1, &imageName );
    ilBindImage( imageName );

    normalizeZ( img->s(), img->t(), img->data() );
    // We prenormalize first texture as if we used it in A8L8 format.
    // This is necessary for correct normalized mipmap generation.
    //grayToBlue( img->s(), img->t(), img->data() );
    // ^ simple color replacing doesn't replace filtered
    // semi-gray pixels
    
    if ( ilTexImage( img->s(), img->t(), 1, 3,
                     IL_RGB, IL_UNSIGNED_BYTE,
                     (ILvoid*)img->data() ) == IL_FALSE )
    {
        throw std::runtime_error( std::string( "ilTexImage failed: " )
                                  + iluErrorString( ilGetError() ) );
    }

    //ilSaveImage( "c:/test.tga" );

    // which filter is better for scaling?
    iluImageParameter( ILU_FILTER, ILU_SCALE_BOX ); // good (and used by video hardware?)
//    iluImageParameter( ILU_FILTER, ILU_SCALE_TRIANGLE ); // ~same as BSPLINE?
//    iluImageParameter( ILU_FILTER, ILU_SCALE_BELL ); // ~same as BSPLINE?(box (*) box (*) box)
//    iluImageParameter( ILU_FILTER, ILU_SCALE_BSPLINE ); // bit smoother and slower than box (box (*) box (*) box (*) box)
    char* curData = (char*)a8l8data;

    for ( int w1 = widthP2, h1 = heightP2; // remark - no (/2)
          !(w1 == 0 && h1 == 0); w1/=2, h1/=2)
    {
        if ( !w1 ) w1 = 1;
        if ( !h1 ) h1 = 1;

        if ( iluScale( w1, h1, 1 ) == IL_FALSE )
        {
            throw std::runtime_error( std::string( "ilScale failed: " )
                                      + iluErrorString( ilGetError() ) );
        }

        normalizeRGB( w1, h1, ilGetData() );

        //ilSaveImage( "c:/test.tga" );
        makeA8L8fromRGB( w1, h1, ilGetData(), curData );

        if ( flipx )
        {
            flipX( w1, h1, curData );
        }

        if ( flipy )
        {
            flipY( w1, h1, curData );
        }

        curData += w1*h1*2;
    }

    ilDeleteImages( 1, &imageName );

    // -- Create final image & save it --
    osg::ref_ptr< osg::Image > imgA8L8 = new osg::Image();

    imgA8L8->setImage( widthP2,
                       heightP2,
                       1,
                       GL_LUMINANCE8_ALPHA8, // internalTextureformat
                       GL_LUMINANCE_ALPHA, // pixelFormat
                       GL_UNSIGNED_BYTE,
                       (unsigned char*) a8l8data,
                       osg::Image::USE_MALLOC_FREE );
    imgA8L8->setMipmapLevels( mipmapIndexes );

    osgDB::writeImageFile( *imgA8L8, oFile );
}

// -- Options parsing stuff --

class Setter : public osg::Referenced
{
    public:
        virtual ~Setter() {}
        virtual void set( const std::string& s ) = 0;
};

class StringSetter : public Setter
{
    public:
        StringSetter( std::string* s )
            : ptr( s )
        {}
        
        virtual void set( const std::string& s ) { *ptr = s; };
    private:
        std::string* ptr;
};

class IntSetter : public Setter
{
    public:
        IntSetter( int* i )
            : ptr( i )
        {}
        
        virtual void set( const std::string& s ) { *ptr = atoi( s.c_str() ); };
    private:
        int* ptr;
};

class BoolSetter : public Setter
{
    public:
        BoolSetter( bool* i )
            : ptr( i )
        {}
        
        virtual void set( const std::string& s )
        {
            *ptr = ( s=="yes" || s=="ye" || s=="y" || s=="1" || s=="true" );
        }
        
    private:
        bool* ptr;
};

struct Option
{
    public:

        enum Kind
        {
            Required,
            Optional
        };
        
        Option( const std::string& name,
                const std::string& desc,
                int* val,
                Kind kind = Optional )
            : name( name )
            , description( desc )
            , kind( kind )
            , optionSet( false )
            , setter( new IntSetter( val ) )
        {}
        
        Option( const std::string& name,
                const std::string& desc,
                std::string* val,
                Kind kind = Optional )
            : name( name )
            , description( desc )
            , kind( kind )
            , optionSet( false )
            , setter( new StringSetter( val ) )
        {}
        
        Option( const std::string& name,
                const std::string& desc,
                bool* val,
                Kind kind = Optional )
            : name( name )
            , description( desc )
            , kind( kind )
            , optionSet( false )
            , setter( new BoolSetter( val ) )
        {}
        
        std::string name;
        std::string description;
        Kind        kind;

        void set( const std::string& arg )
        {
            optionSet = true;
            setter->set( arg );
        }

        bool isOptionSet() const { return optionSet; }

    private:

        bool                    optionSet;
        osg::ref_ptr< Setter >  setter;
       
};

typedef std::vector< Option > Options;

bool
parseOptions( Options& options,
              int argc,
              const char** argv )
{
    // -- Scan argv --
    for ( int i = 1; i < argc; i += 2 )
    {
        bool found = false;
        
        for ( size_t j = 0; j < options.size(); j++ )
        {
            if ( argv[i] == options[j].name )
            {
                if ( i+1 == argc )
                {
                    std::cerr << "No parameter specified for option "
                              << options[j].name << std::endl;
                    return false;
                }
                
                options[j].set( argv[i+1] );
                found = true;
                break;
            }
        }

        if ( !found )
        {
            std::cerr << "Unknown option: " << argv[i] << std::endl;
            return false;
        }
    }

    // -- Check than all required options are set --
    for ( size_t i = 0; i < options.size(); i++ )
    {
        if (    options[i].kind == Option::Required
             && options[i].isOptionSet() == false )
        {
            std::cerr << "Required option " << options[i].name << " not set"
                      << std::endl;
            return false;
        }
    }

    return true;
}

void
printUsage( const Options& options )
{
    std::cout << "Options are:" << std::endl;
    
    for ( size_t i = 0; i < options.size(); i++ )
    {
        std::cout << "  " << options[i].name
                  << " " << options[i].description
                  << (options[i].kind == Option::Required ? " (required)" : "")
                  << std::endl;
    }
}

// -- Main --

int
main( int argc,
      const char** argv )
{
//    osg::setNotifyLevel( osg::DEBUG_FP );

    std::string iFile;
    std::string oFile;
    int         maxWidth = 4096;
    int         maxHeight = 4096;
    bool        flipX = false;
    bool        flipY = false;

    Options options;

    options.push_back( Option( "-i", "input file", &iFile, Option::Required ) );
    options.push_back( Option( "-o", "output file.dds", &oFile, Option::Required ) );
    options.push_back( Option( "-maxw", "maximum width", &maxWidth ) );
    options.push_back( Option( "-maxh", "maximum height", &maxHeight ) );
    options.push_back( Option( "-flipx", "flip X values", &flipX ) );
    options.push_back( Option( "-flipy", "flip Y values", &flipY ) );

    if ( !parseOptions( options, argc, argv ) )
    {
        printUsage( options );
        return EXIT_FAILURE;
    }

    if ( osgDB::getLowerCaseFileExtension( oFile ) != "dds" )
    {
        std::cerr << "Output file must have .dds extension" << std::endl;       
        return EXIT_FAILURE;
    }

    try
    {
        ilInit();               

//        ilEnable( IL_ORIGIN_SET );
//        ilOriginFunc( IL_ORIGIN_LOWER_LEFT );
        // ^ glDrawPixels draws from lover left

        iluInit();

        toA8L8( iFile, oFile, maxWidth, maxHeight, flipX, flipY );
    }
    catch ( std::runtime_error& e )
    {
        std::cerr << "Error converting to A8L8:\n  "
                  << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
