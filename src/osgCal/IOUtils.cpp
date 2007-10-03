/* -*- c++ -*-
    Copyright (C) 2006 Vladimir Shabanov <vshabanoff@gmail.com>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#include <sys/stat.h>
#include <stdexcept>

#include <osg/io_utils>

#include <osgDB/FileNameUtils>

#include <osgCal/IOUtils>


namespace osgCal
{

std::string
getPrefix( const std::string& str )
{
    std::string::size_type pos;
    pos = str.find_first_of(":");

    if(pos == std::string::npos) return std::string();

    return str.substr(0, pos + 1);
}

std::string
getSuffix( const std::string& str )
{
    std::string::size_type pos;
    pos = str.find_last_of(":");

    if(pos == std::string::npos) return str;

    return str.substr(pos + 1);
}

bool
endsWith( const std::string& str,
          const std::string& suffix )
{
    std::string::size_type suffl = suffix.length();;
    std::string::size_type strl = str.length();;

    return ( strl >= suffl && str.substr( strl - suffl ) == suffix );
}

std::string
getAfter( const std::string& prefix,
          const std::string& str )
{
    return str.substr(prefix.size());
}

bool
prefixEquals( const std::string& str,
              const std::string& prefix )
{
    if ( str.length() < prefix.length() )
    {
        return false;
    }
    else
    {
        return str.substr( 0, prefix.length() ) == prefix;
    }
}

bool
isFileOlder( const std::string& f1,
             const std::string& f2 )
{
    struct stat stat1;
    struct stat stat2;

    if ( stat( f1.c_str(), &stat1 ) != 0 )
    {
        throw std::runtime_error( "isFileOlder: stat error for " + f1 );
    }

    if ( stat( f2.c_str(), &stat2 ) != 0 )
    {
        throw std::runtime_error( "isFileOlder: stat error for " + f2 );
    }

    return ( stat1.st_mtime < stat2.st_mtime );
}

bool
isFileExists( const std::string& f )
{
    struct stat st;

    return ( stat( f.c_str(), &st ) == 0 );
}

std::string
VBOsCacheFileName( const std::string& cfgFileName )
{
    return cfgFileName + ".vbos.cache";
}

std::string
HWModelCacheFileName( const std::string& cfgFileName )
{
    return cfgFileName + ".hwmodel.cache";
}

// -- VBOs data type & I/O --

VBOs::VBOs( int maxVertices,
            int maxFaces )
    : vertexBuffer( new VertexBuffer( maxVertices ) )
    , weightBuffer( new WeightBuffer( maxVertices ) )
    , matrixIndexBuffer( new MatrixIndexBuffer( maxVertices ) )
    , normalBuffer( new NormalBuffer( maxVertices ) )
    , tangentBuffer( new TangentBuffer( maxVertices ) )
    , binormalBuffer( new BinormalBuffer( maxVertices ) )
    , texCoordBuffer( new TexCoordBuffer( maxVertices ) )
    , indexBuffer( new IndexBuffer( maxFaces*3 ) )
    , vertexCount( maxVertices )
    , faceCount( maxFaces )
{}

VBOs::~VBOs()
{}

int
VBOs::getVertexCount() const
{
    return vertexCount;
}

int
VBOs::getFaceCount() const
{
    return faceCount;
}

template < typename T >
void
resize( osg::ref_ptr< T >& v,
        int                size )
{
    v = new T( v->begin(), v->begin() + size );
    // STL vector doesn't cut memory on resize
    // so we manually recreate one of necessary size
}

void
VBOs::setVertexCount( int vc )
{
    vertexCount = vc;
    resize( vertexBuffer, vc );
    resize( weightBuffer, vc );
    resize( matrixIndexBuffer, vc );
    resize( normalBuffer, vc );
    resize( tangentBuffer, vc );
    resize( binormalBuffer, vc );
    resize( texCoordBuffer, vc );
}

void
VBOs::setFaceCount( int fc )
{
    faceCount = fc;
    resize( indexBuffer, fc*3 );
}


#if defined(_MSC_VER)
    typedef int int32_t;
#endif
    
VBOs*
loadVBOs( const std::string& fn ) throw (std::runtime_error)
{
    FILE* f = fopen( fn.c_str(), "rb" );

    if ( f == NULL )
    {
        throw std::runtime_error( "Can't open " + fn );
    }

    int32_t vertexCount;
    int32_t faceCount;

#define READ_( _buf, _size )                                                        \
    if ( fread( _buf, _size, 1, f ) != 1 )                                          \
    {                                                                               \
        throw std::runtime_error( "Can't read "#_buf + std::string(" from ") + fn );\
    }

#define READ( _buf )                                                    \
    if ( fread( (void*)_buf->getDataPointer(), _buf->getTotalDataSize(), 1, f ) != 1 ) \
    {                                                                   \
        throw std::runtime_error( "Can't read "#_buf + std::string(" from ") + fn ); \
    }

    READ_( &vertexCount, 4 );
    READ_( &faceCount, 4 );

    std::auto_ptr< VBOs > vbos( new VBOs( vertexCount, faceCount ) );

    READ( vbos->vertexBuffer );
    READ( vbos->weightBuffer );
    READ( vbos->matrixIndexBuffer );
    READ( vbos->normalBuffer );
    READ( vbos->tangentBuffer );
    READ( vbos->binormalBuffer );
    READ( vbos->texCoordBuffer );
    READ( vbos->indexBuffer );

//#undef READ
    
    fclose( f );

    return vbos.release();
}

void
saveVBOs( VBOs* vbos,
          const std::string& fn ) throw (std::runtime_error)
{
    FILE* f = fopen( fn.c_str(), "wb" );

    if ( f == NULL )
    {
        throw std::runtime_error( "Can't create " + fn );
    }

    int32_t vertexCount = vbos->getVertexCount();
    int32_t faceCount   = vbos->getFaceCount();

#define WRITE_( _buf, _size )                                                       \
    if ( fwrite( _buf, _size, 1, f ) != 1 )                                         \
    {                                                                               \
        throw std::runtime_error( "Can't write "#_buf + std::string(" to ") + fn ); \
    }

#define WRITE( _buf )                                                   \
    if ( fwrite( _buf->getDataPointer(), _buf->getTotalDataSize(), 1, f ) != 1 ) \
    {                                                                   \
        throw std::runtime_error( "Can't write "#_buf + std::string(" to ") + fn ); \
    }

    WRITE_( &vertexCount, 4 );
    WRITE_( &faceCount, 4 );

    WRITE( vbos->vertexBuffer );
    WRITE( vbos->weightBuffer );
    WRITE( vbos->matrixIndexBuffer );
    WRITE( vbos->normalBuffer );
    WRITE( vbos->tangentBuffer );
    WRITE( vbos->binormalBuffer );
    WRITE( vbos->texCoordBuffer );
    WRITE( vbos->indexBuffer );

//#undef WRITE
    
    fclose( f );
}

VBOs*
loadVBOs( CalHardwareModel* calHardwareModel ) throw (std::runtime_error)
{
    std::auto_ptr< VBOs > vbos( new VBOs() );
    float* floatMatrixIndexBuffer = new float[vbos->getVertexCount()*4];

    calHardwareModel->setVertexBuffer((char*)vbos->vertexBuffer->getDataPointer(),
                                      3*sizeof(float));
#ifdef OSG_CAL_BYTE_BUFFERS
    float* floatNormalBuffer = new float[vbos->getVertexCount()*3];
    calHardwareModel->setNormalBuffer((char*)floatNormalBuffer,
                                      3*sizeof(float));
#else
    calHardwareModel->setNormalBuffer((char*)vbos->normalBuffer->getDataPointer(),
                                      3*sizeof(float));
#endif
    calHardwareModel->setWeightBuffer((char*)vbos->weightBuffer->getDataPointer(),
                                      4*sizeof(float));
    calHardwareModel->setMatrixIndexBuffer((char*)floatMatrixIndexBuffer,
                                           4*sizeof(float));
    calHardwareModel->setTextureCoordNum(1);
    calHardwareModel->setTextureCoordBuffer(0, // texture stage #
                                            (char*)vbos->texCoordBuffer->getDataPointer(),
                                            2*sizeof(float));
    GLuint*  indexBuffer = (GLuint*)vbos->indexBuffer->getDataPointer();
    calHardwareModel->setIndexBuffer((CalIndex*)indexBuffer);
    // calHardwareModel->setCoreMeshIds(_activeMeshes);
    // if ids not set all meshes will be used at load() time

    //std::cout << "calHardwareModel->load" << std::endl;
    calHardwareModel->load( 0, 0, OSGCAL_MAX_BONES_PER_MESH );
    //std::cout << "calHardwareModel->load ok" << std::endl;

    // the index index in pIndexBuffer are relative to the begining of the hardware mesh,
    // we make them relative to the begining of the buffer.
    for(int hardwareMeshId = 0; hardwareMeshId < calHardwareModel->getHardwareMeshCount(); hardwareMeshId++)
    {
        calHardwareModel->selectHardwareMesh(hardwareMeshId);

        for(int faceId = 0; faceId < calHardwareModel->getFaceCount(); faceId++)
        {
            indexBuffer[faceId*3+0+ calHardwareModel->getStartIndex()]
                += calHardwareModel->getBaseVertexIndex();
            indexBuffer[faceId*3+1+ calHardwareModel->getStartIndex()]
                += calHardwareModel->getBaseVertexIndex();
            indexBuffer[faceId*3+2+ calHardwareModel->getStartIndex()]
                += calHardwareModel->getBaseVertexIndex();
        }
    }

    vbos->setVertexCount( calHardwareModel->getTotalVertexCount() );
    vbos->setFaceCount( calHardwareModel->getTotalFaceCount() );
    // set{Vertex/Face}Count also resizes VBOs from default 1000000 size
    // to their real size.
    //
    // BTW: that was an "Out of memory" error on some ATI cards
    // (first reported by Jan Ciger)
    //
    // TODO: Interesting -- ATI doesn't support VBOs of a 1 million elements?
    // The one buffer (and the total sum) is smaller than available
    // memory, why these "Out of memory" errors?
    //

    indexBuffer = (GLuint*)vbos->indexBuffer->getDataPointer();
    // reinitialize index buffer after resize
    
    GLfloat* texCoordBuffer = (GLfloat*) vbos->texCoordBuffer->getDataPointer();

#ifdef OSG_CAL_BYTE_BUFFERS
    typedef GLubyte MatrixIndex;
#else
    typedef GLshort MatrixIndex;
#endif
    MatrixIndex* matrixIndexBuffer = (MatrixIndex*) vbos->matrixIndexBuffer->getDataPointer();

    for ( int i = 0; i < vbos->getVertexCount()*4; i++ )
    {
        matrixIndexBuffer[i] = static_cast< MatrixIndex >( floatMatrixIndexBuffer[i] );
    }

    delete[] floatMatrixIndexBuffer;

    //std::cout << "Total vertex count : " << vbos->getVertexCount() << std::endl;
    //std::cout << "Total face count   : " << vbos->faceCount << std::endl;
   
    // Generate tangents for whole model.
    // We can do this only for normal mapped meshes, but in future
    // we will make all VBOs cached in separate file, so this calculation
    // will be necessary only one time
    {
        CalVector* tan1 = new CalVector[vbos->getVertexCount()];
        CalVector* tan2 = new CalVector[vbos->getVertexCount()];

        GLfloat* vertexBuffer = (GLfloat*) vbos->vertexBuffer->getDataPointer();
#ifdef OSG_CAL_BYTE_BUFFERS
        GLfloat* tangentBuffer = new GLfloat[ vbos->getVertexCount()*3 ];
        GLfloat* normalBuffer = floatNormalBuffer;
        GLfloat* binormalBuffer = new GLfloat[ vbos->getVertexCount()*3 ];;
#else
        GLfloat* tangentBuffer = (GLfloat*) vbos->tangentBuffer->getDataPointer();
        GLfloat* normalBuffer = (GLfloat*) vbos->normalBuffer->getDataPointer();
        GLfloat* binormalBuffer = (GLfloat*) vbos->binormalBuffer->getDataPointer();
#endif

        for ( int face = 0; face < calHardwareModel->getTotalFaceCount(); face++ )
        {
            for ( int j = 0; j < 3; j++ )
            {
                // there seems to be no visual difference in calculating
                // tangent per vertex (as is tan1[i1] += spos(j=0,1,2))
                // or per face (tan1[i1,i2,i3] += spos)
                GLuint i1 = indexBuffer[face*3+(j+0)%3];
                GLuint i2 = indexBuffer[face*3+(j+1)%3];
                GLuint i3 = indexBuffer[face*3+(j+2)%3];
        
                const float* v1 = &vertexBuffer[i1*3];
                const float* v2 = &vertexBuffer[i2*3];
                const float* v3 = &vertexBuffer[i3*3];

                const float* w1 = &texCoordBuffer[i1*2];
                const float* w2 = &texCoordBuffer[i2*2];
                const float* w3 = &texCoordBuffer[i3*2];

#define x(_a) (_a[0])
#define y(_a) (_a[1])
#define z(_a) (_a[2])
        
                float x1 = x(v2) - x(v1);
                float x2 = x(v3) - x(v1);
                float y1 = y(v2) - y(v1);
                float y2 = y(v3) - y(v1);
                float z1 = z(v2) - z(v1);
                float z2 = z(v3) - z(v1);
        
                float s1 = x(w2) - x(w1);
                float s2 = x(w3) - x(w1);
                float t1 = y(w2) - y(w1);
                float t2 = y(w3) - y(w1);

#undef x
#undef y
#undef z
        
                //float r = 1.0F / (s1 * t2 - s2 * t1);
                float r = (s1 * t2 - s2 * t1) < 0 ? -1.0 : 1.0;
                CalVector sdir((t2 * x1 - t1 * x2) * r, (t2 * y1 - t1 * y2) * r,
                               (t2 * z1 - t1 * z2) * r);
                CalVector tdir((s1 * x2 - s2 * x1) * r, (s1 * y2 - s2 * y1) * r,
                               (s1 * z2 - s2 * z1) * r);

                // sdir & tdir can be 0 (when UV unwrap doesn't exists
                // or has errors like coincide points)
                // we ignore them
                if ( sdir.length() > 0 && tdir.length() > 0 )
                {
                    sdir.normalize(); 
                    tdir.normalize();

                    tan1[i1] += sdir;
                    //tan1[i2] += sdir;
                    //tan1[i3] += sdir;
        
                    tan2[i1] += tdir;
                    //tan2[i2] += tdir;
                    //tan2[i3] += tdir;
                }
            }
        }
    
        for (long a = 0; a < vbos->getVertexCount(); a++)
        {
            CalVector tangent;
            CalVector binormal;
            CalVector t = tan1[a];

            // tangent can be zero when UV unwrap doesn't exists
            // or has errors like coincide points
            if ( t.length() > 0 )
            {
                CalVector n = CalVector( normalBuffer[a*3+0],
                                         normalBuffer[a*3+1],
                                         normalBuffer[a*3+2] );

                t.normalize();
        
                // Gram-Schmidt orthogonalize
                tangent = t - n * (n*t);
                tangent.normalize();

                // Calculate handedness
                binormal = CalVector(n % tangent) *
                    ((((n % t) * tan2[a]) < 0.0F) ? -1.0f : 1.0f);
                binormal.normalize();
            }

//             std::cout << "t = " << tangent.x  << '\t' << tangent.y  << '\t' << tangent.z << '\n' ;
//             std::cout << "b = " << binormal.x << '\t' << binormal.y << '\t' << binormal.z << '\n';;

            tangentBuffer[a*3+0] = tangent.x; 
            tangentBuffer[a*3+1] = tangent.y;
            tangentBuffer[a*3+2] = tangent.z;
        
            binormalBuffer[a*3+0] = binormal.x;
            binormalBuffer[a*3+1] = binormal.y;
            binormalBuffer[a*3+2] = binormal.z;
        }
    
        delete[] tan1;
        delete[] tan2;

#ifdef OSG_CAL_BYTE_BUFFERS
        GLbyte* tangents = (GLbyte*) vbos->tangentBuffer->getDataPointer();
        GLbyte* binormals = (GLbyte*) vbos->binormalBuffer->getDataPointer();

        for ( int i = 0; i < vbos->getVertexCount()*3; i++ )
        {
            tangents[i]  = static_cast< GLbyte >( tangentBuffer[i]*127.0 );
            binormals[i] = static_cast< GLbyte >( binormalBuffer[i]*127.0 );
            //std::cout << (int)tangents[i] << '\n';
        }

        delete[] tangentBuffer;
        delete[] binormalBuffer;
#endif
    }

#ifdef OSG_CAL_BYTE_BUFFERS
    GLbyte* normals = (GLbyte*) vbos->normalBuffer->getDataPointer();

    for ( int i = 0; i < vbos->getVertexCount()*3; i++ )
    {
        normals[i]  = static_cast< GLbyte >( floatNormalBuffer[i]*127.0 );
    }

    delete[] floatNormalBuffer;
#endif

    // invert UVs for OpenGL (textures are inverted otherwise - for example, see abdulla/klinok)
    for ( float* tcy = texCoordBuffer + 1;
          tcy < texCoordBuffer + 2*vbos->getVertexCount();
          tcy += 2 )
    {
        *tcy = 1.0f - *tcy;
    }

    // prepare tangents and binormals
//     for ( int i = 0; i < vbos->getVertexCount(); i++ )
//     {
// //        tmpTangentSpace[i].tangent.x = -tmpTangentSpace[i].tangent.x;
//         // no flip tangent - looks like anisotropic lights
//         // ATI NMF generated normals require x flip
        
//         vbos->tangentBuffer[i*3+0] = tmpTangentSpace[i].tangent.x; 
//         vbos->tangentBuffer[i*3+1] = tmpTangentSpace[i].tangent.y;
//         vbos->tangentBuffer[i*3+2] = tmpTangentSpace[i].tangent.z;

//         CalVector binormal = (tmpTangentSpace[i].tangent % CalVector( vbos->normalBuffer[i*3+0],
//                                          vbos->normalBuffer[i*3+1],
//                                          vbos->normalBuffer[i*3+2] )
//                               ) * tmpTangentSpace[i].crossFactor;

//         vbos->binormalBuffer[i*3+0] = binormal.x;
//         vbos->binormalBuffer[i*3+1] = binormal.y;
//         vbos->binormalBuffer[i*3+2] = binormal.z;
//     }

    return vbos.release();
}

static const int HW_MODEL_FILE_VERSION = 0xCA3D0001;
    
CalHardwareModel*
loadHardwareModel( const CalCoreModel* calCoreModel,
                   const std::string& fn,
                   std::vector< std::string >& meshNames )
    throw (std::runtime_error)
{
    FILE* f = fopen( fn.c_str(), "rb" );

    if ( f == NULL )
    {
        throw std::runtime_error( "Can't open " + fn );
    }

#define READ_I32( _i ) { int32_t _i32_tmp = 0; READ_( &_i32_tmp, 4 ); _i = _i32_tmp; }

    int version;

    READ_I32( version );
    if ( version != HW_MODEL_FILE_VERSION )
    {
        fclose( f );
        throw std::runtime_error( "Incorrect file version " + fn + ". Try rerun osgCalPreparer." );
    }

    std::auto_ptr< CalHardwareModel > calHardwareModel(
        new CalHardwareModel(const_cast< CalCoreModel* >( calCoreModel ) ) );
    
    std::vector< CalHardwareModel::CalHardwareMesh >& hwMeshes =
        calHardwareModel->getVectorHardwareMesh();

    int size = 0;

    READ_I32( size );
    hwMeshes.resize( size );

    for ( int i = 0; i < size; i++ )
    {
        CalHardwareModel::CalHardwareMesh& mesh = hwMeshes[i];

        int biSize = 0;
        READ_I32( biSize );
        mesh.m_vectorBonesIndices.resize( biSize );
        for ( size_t j = 0; j < mesh.m_vectorBonesIndices.size(); j++ )
        {
            READ_I32( mesh.m_vectorBonesIndices[j] );
        }
        READ_I32( mesh.baseVertexIndex );
        READ_I32( mesh.vertexCount );
        READ_I32( mesh.startIndex );
        READ_I32( mesh.faceCount );
        READ_I32( mesh.meshId );
        READ_I32( mesh.submeshId );

        int coreMaterialThreadId;
        READ_I32( coreMaterialThreadId );

//         CalCoreMesh*    calCoreMesh = calCoreModel->getCoreMesh( mesh.meshId );
//         CalCoreSubmesh* calCoreSubmesh = calCoreMesh->getCoreSubmesh( mesh.submeshId );

//         mesh.pCoreMaterial =
//             calCoreModel->getCoreMaterial( calCoreSubmesh->getCoreMaterialThreadId() );
        mesh.pCoreMaterial = const_cast< CalCoreModel* >( calCoreModel )->
            getCoreMaterial( coreMaterialThreadId );

        int nameBufSize;
        READ_I32( nameBufSize );
        if ( nameBufSize > 1024 )
        {
            fclose( f );
            throw std::runtime_error( "Too long mesh name (incorrect hwmodel.cache file?)." );
        }
        char name[ 1024 ];
        READ_( name, nameBufSize );
        meshNames.push_back( std::string( &name[0], &name[ nameBufSize ] ) );
    }

    fclose( f );
    
    return calHardwareModel.release();
}

int
getCoreMaterialThreadId( CalCoreModel* model,
                         const CalCoreMaterial* material )
{
    for ( int i = 0; i < model->getCoreMaterialCount(); i++ )
    {
        if ( model->getCoreMaterial( i ) == material )
        {
            return i;
        }
    }
    return (-1);
}

void
saveHardwareModel( const CalHardwareModel* calHardwareModel,
                   const CalCoreModel*     calCoreModel,
                   const std::vector< std::string >& meshNames,
                   const std::string& fn ) throw (std::runtime_error)
{
    const std::vector< CalHardwareModel::CalHardwareMesh >& hwMeshes =
        const_cast< CalHardwareModel* >( calHardwareModel )->getVectorHardwareMesh();

    if ( meshNames.size() != hwMeshes.size() )
    {
        throw std::runtime_error( "meshNames.size() != hwMeshes.size()" );
    }

    FILE* f = fopen( fn.c_str(), "wb" );

    if ( f == NULL )
    {
        throw std::runtime_error( "Can't create " + fn );
    }

#define WRITE_I32( _i ) { int32_t _i32_tmp = _i; WRITE_( &_i32_tmp, 4 ); }

    WRITE_I32( HW_MODEL_FILE_VERSION );

    WRITE_I32( hwMeshes.size() );

    for ( size_t i = 0; i < hwMeshes.size(); i++ )
    {
        const CalHardwareModel::CalHardwareMesh& mesh = hwMeshes[i];
        WRITE_I32( mesh.m_vectorBonesIndices.size() );
        for ( size_t j = 0; j < mesh.m_vectorBonesIndices.size(); j++ )
        {
            WRITE_I32( mesh.m_vectorBonesIndices[j] );
        }
        WRITE_I32( mesh.baseVertexIndex );
        WRITE_I32( mesh.vertexCount );
        WRITE_I32( mesh.startIndex );
        WRITE_I32( mesh.faceCount );
        WRITE_I32( mesh.meshId );
        WRITE_I32( mesh.submeshId );

        int coreMaterialThreadId = getCoreMaterialThreadId(
            const_cast< CalCoreModel* >( calCoreModel ), mesh.pCoreMaterial );
        if ( coreMaterialThreadId < 0 )
        {
            fclose( f );
            throw std::runtime_error( "Can't get coreMaterialThreadId (mesh.pCoreMaterial not found in coreModel?" );            
        }
        WRITE_I32( coreMaterialThreadId );

        const std::string& name = meshNames[ i ];
        WRITE_I32( name.size() );
        WRITE_( name.data(), name.size() );
    }

    fclose( f );
}

CalCoreModel*
loadCoreModel( const std::string& cfgFileName,
               float& scale,
               bool ignoreMeshes )
    throw (std::runtime_error)
{
    // -- Initial loading of model --
    scale = 1.0f;
    bool bScale = false;

    FILE* f = fopen( cfgFileName.c_str(), "r" );
    if( !f )
    {
        throw std::runtime_error( "Can't open " + cfgFileName );
    }

    std::auto_ptr< CalCoreModel > calCoreModel( new CalCoreModel( "dummy" ) );

    // Extract path from fileName
    std::string dir = osgDB::getFilePath( cfgFileName );

    static const int LINE_BUFFER_SIZE = 4096;
    char buffer[LINE_BUFFER_SIZE];

    while ( fgets( buffer, LINE_BUFFER_SIZE,f ) )
    {
        // Ignore comments or empty lines
        if ( *buffer == '#' || *buffer == 0 )
            continue;

        char* equal = strchr( buffer, '=' );
        if ( equal )
        {
            // Terminates first token
            *equal++ = 0;
            // Removes ending newline ( CR & LF )
            {
                int last = strlen( equal ) - 1;
                if ( equal[last] == '\n' ) equal[last] = 0;
                if ( last > 0 && equal[last-1] == '\r' ) equal[last-1] = 0;
            }

            // extract file name. all animations, meshes and materials names
            // are taken from file name without extension
            std::string nameToLoad;            
            char* point = strrchr( equal, '.' );
            if ( point )
            {
                nameToLoad = std::string( equal, point );
            }
            else
            {
                nameToLoad = equal;
            }
                
            std::string fullpath = dir + "/" + std::string( equal );

            // process .cfg parameters
            if ( !strcmp( buffer, "scale" ) ) 
            { 
                bScale	= true;
                scale	= atof( equal );
                continue;
            }

            if ( !strcmp( buffer, "skeleton" ) ) 
            {
                if( !calCoreModel->loadCoreSkeleton( fullpath ) )
                {
                    throw std::runtime_error( "Can't load skeleton: "
                                             + CalError::getLastErrorDescription() );
                }                
            } 
            else if ( !strcmp( buffer, "animation" ) )
            {
                int animationId = calCoreModel->loadCoreAnimation( fullpath );
                if( animationId < 0 )
                {
                    throw std::runtime_error( "Can't load animation " + nameToLoad + ": "
                                             + CalError::getLastErrorDescription() );
                }
                calCoreModel->getCoreAnimation(animationId)->setName( nameToLoad );
            }
            else if ( !strcmp( buffer, "mesh" ) )
            {
                if ( ignoreMeshes )
                {
                    continue; // we don't need meshes since VBO data is already loaded from cache
                }

                int meshId = calCoreModel->loadCoreMesh( fullpath );
                if( meshId < 0 )
                {
                    throw std::runtime_error( "Can't load mesh " + nameToLoad + ": "
                                             + CalError::getLastErrorDescription() );
                }
                calCoreModel->getCoreMesh( meshId )->setName( nameToLoad );

                // -- Remove zero influence vertices --
                // warning: this is a temporary workaround and subject to remove!
                // (this actually must be fixed in blender exporter)
                CalCoreMesh* cm = calCoreModel->getCoreMesh( meshId );

                for ( int i = 0; i < cm->getCoreSubmeshCount(); i++ )
                {
                    CalCoreSubmesh* sm = cm->getCoreSubmesh( i );

                    std::vector< CalCoreSubmesh::Vertex >& v = sm->getVectorVertex();

                    for ( size_t j = 0; j < v.size(); j++ )
                    {
                        std::vector< CalCoreSubmesh::Influence >& infl = v[j].vectorInfluence;

                        for ( size_t ii = 0; ii < infl.size(); ii++ )
                        {
                            if ( infl[ii].weight == 0 )
                            {
                                infl.erase( infl.begin() + ii );
                                --ii;
                            }
                        }
                    }
                }
            }
            else if ( !strcmp( buffer, "material" ) )  
            {
                int materialId = calCoreModel->loadCoreMaterial( fullpath );

                if( materialId < 0 ) 
                {
                    throw std::runtime_error( "Can't load material " + nameToLoad + ": "
                                             + CalError::getLastErrorDescription() );
                } 
                else 
                {
                    calCoreModel->createCoreMaterialThread( materialId ); 
                    calCoreModel->setCoreMaterialId( materialId, 0, materialId );

                    CalCoreMaterial* material = calCoreModel->getCoreMaterial( materialId );
                    material->setName( nameToLoad );
                }
            }
        }
    }

    // we don't use embedded cal3d tangent vectors generation 
    // since it doesn't take into account that meshes can be mirrored
    // and tangent basis handedness can be inverted, so we need to calculate
    // bitangent to determine handedness (cal3d doesn't caculate handedness)
//     // enable tangent space generation
//     for ( int meshId = 0; meshId < calCoreModel->getCoreMeshCount(); ++meshId )
//     {
//         CalCoreMesh* cm = calCoreModel->getCoreMesh( meshId );

//         for ( int i = 0; i < cm->getCoreSubmeshCount(); i++ )
//         {
//             CalCoreMaterial* mat = calCoreModel->getCoreMaterial( cm->getCoreSubmesh( i )->getCoreMaterialThreadId() );

//             for ( int j = 0; j < mat->getMapCount(); j++ )
//             {
//                 if ( getPrefix( mat->getMapFilename(j) ) == "NormalsMap:" )
//                 {
//                     cm->getCoreSubmesh( i )->enableTangents( 0, true );
//                     break;
//                 }                
//             }
//         }
//     }
    
    // scaling must be done after everything has been created
    if( bScale )
    {
        calCoreModel->scale( scale );
    }
    fclose( f );

    return calCoreModel.release();
}

}
