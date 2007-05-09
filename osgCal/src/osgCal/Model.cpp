/* -*- c++ -*-
    Copyright (C) 2003 <ryu@gpul.org>
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
#include <cal3d/model.h>

#include <osg/BlendFunc>
#include <osg/Image>
#include <osg/Material>
#include <osg/NodeCallback>
#include <osg/ShapeDrawable>
#include <osg/TexEnv>
#include <osg/TexGen>
#include <osg/TexEnv>
#include <osg/TexMat>
#include <osg/Texture2D>
#include <osg/Timer>
#include <osg/MatrixTransform>

#include <osgDB/ReadFile>

#include <osgCal/Model>
#include <osgCal/SubMeshHardware>
#include <osgCal/SubMeshSoftware>

using namespace osgCal;

class CalUpdateCallback: public osg::NodeCallback 
{
    public:

        CalUpdateCallback()
            : previous(0)
            , prevTime(0)
        {}

        virtual void operator()( osg::Node*        node,
                                 osg::NodeVisitor* nv )
        {
            Model* model = dynamic_cast< Model* >( node );

            if ( previous == 0 )
            {
                previous = timer.tick();
            }
            
            float deltaTime = 0;

            if (!nv->getFrameStamp())
            {
                osg::Timer_t current = timer.tick();
                deltaTime = (float)timer.delta_s(previous, current);
                previous = current;
            }
            else
            {
                double time = nv->getFrameStamp()->getSimulationTime();
                deltaTime = time - prevTime;
                prevTime = time;
            }

            CalModel* calModel = model->getCalModel();

            calModel->getAbstractMixer()->updateAnimation(deltaTime);
            calModel->getAbstractMixer()->updateSkeleton();

            model->update();

            traverse(node, nv);
//            node->dirtyBound(); <- is it necessary?
        }

    private:

        osg::Timer timer;
        osg::Timer_t previous;
        double prevTime;

};

Model::Model()
    : calModel( 0 )
{
//     std::cout << "Model::Model()" << std::endl;
}

Model::Model( const Model&       model,
              const osg::CopyOp& copyop )
{
    throw std::runtime_error( "Model copying is not supported" );
}

Model::~Model()
{
    setUpdateCallback( 0 );
    delete calModel;
//     std::cout << "Model::~Model()" << std::endl;
//     std::cout << "CoreModel::referenceCount() = "
//               << coreModel->referenceCount() << std::endl;
}

void
Model::load( CoreModel* cm,
             MeshTyper* meshTyper,
             MeshFilter* meshFilter )
{
    if ( calModel != 0 )
    {
        throw std::runtime_error( "Model already load" );
    }

    coreModel = cm;

    vertexBuffer = (VertexBuffer*) cm->getVertexBuffer()->clone( osg::CopyOp::DEEP_COPY_ALL );
    updateFlagBuffer = new UpdateFlagBuffer( vertexBuffer->size() );

    calModel = new CalModel( coreModel->getCalCoreModel() );
    calModel->update( 0 );

    setUpdateCallback( new CalUpdateCallback() );

    if ( meshTyper == 0 )
    {
        meshTyper = new AllMeshesHardware();
    }
    else
    {
        meshTyper->ref();
    }
    
    for ( size_t i = 0; i < coreModel->getMeshes().size(); i++ )
    {
        const CoreModel::Mesh& mesh = coreModel->getMeshes()[i];
        
        if ( meshFilter != NULL && meshFilter->filter( mesh ) == false )
        {
            continue;
        }

        osg::Geometry* g = 0;

        switch ( meshTyper->type( mesh ) )
        {
            case MT_HARDWARE:
                g = new SubMeshHardware( this, i, mesh.rigid );
                break;

            case MT_SOFTWARE:
                if ( normalBuffer.get() == 0 )
                {
                    // create local normals buffer only if necessary.
                    normalBuffer = (NormalBuffer*) cm->getNormalBuffer()->clone( osg::CopyOp::DEEP_COPY_ALL );
                }
                
                g = new SubMeshSoftware( this, i, mesh.rigid );
                break;

            default:
                throw std::runtime_error( "Model::load - unknown mesh type" );
        }

        meshes[ mesh.name ] = g;
        osg::Geode* geode = new osg::Geode;
        geode->addDrawable( g );

        if ( !mesh.rigid )
        {
            addChild( geode );        
        }
        else
        {
            // for rigid meshes we use bone transform w/o
            // per-vertex transformations
            osg::MatrixTransform* mt = new osg::MatrixTransform;

            mt->setMatrix( osg::Matrix::identity() );
            mt->addChild( geode );
            addChild( mt );
        }
    }

    meshTyper->unref();
}

void
Model::update() 
{
    memset( (void*)updateFlagBuffer->getDataPointer(),
            0,
            updateFlagBuffer->getTotalDataSize() );

#ifdef _OPENMP
#pragma omp parallel for
#endif // _OPENMP
    for(unsigned int i = 0; i < getNumChildren(); i++)
    {
        osg::Node* node = getChild(i);

        // -- Update SubMeshHardware/Software if child is Geode --
        osg::Geode* geode = dynamic_cast< osg::Geode* >( node );
        if ( geode )
        {
            for ( size_t j = 0; j < geode->getNumDrawables(); j++ )
            {
                osg::Drawable* drawable = geode->getDrawable( j );
            
                SubMeshHardware* hardware = dynamic_cast< SubMeshHardware* >( drawable );
                if ( hardware )
                {
                    hardware->update();
                    continue;
                }
            
                SubMeshSoftware* software = dynamic_cast< SubMeshSoftware* >( drawable );
                if ( software )
                {
                    software->update();
                    continue;
                }

                throw std::runtime_error( "unexpected drawable type" );
            }
            
            continue;
        }

        // -- Set transformation matrix if child is MatrixTransform --
        osg::MatrixTransform* mt = dynamic_cast< osg::MatrixTransform* >( node );
        if ( mt )
        {
            osg::Drawable* drawable = static_cast< osg::Geode* >( mt->getChild( 0 ) )->getDrawable( 0 );
            CoreModel::Mesh* m = 0;
            SubMeshHardware* hardware = dynamic_cast< SubMeshHardware* >( drawable );
            if ( hardware )
            {
                m = hardware->getCoreModelMesh();
            }
            else
            {               
                SubMeshSoftware* software = dynamic_cast< SubMeshSoftware* >( drawable );
                if ( software )
                {
                    m = software->getCoreModelMesh();
                }
                else
                {
                    throw std::runtime_error( "unexpected drawable type" );                    
                }
            }

            CalQuaternion   rotationBoneSpace;
            CalVector       translationBoneSpace;
	    
            CalHardwareModel* hardwareModel = coreModel->getCalHardwareModel();

#ifdef _OPENMP
#pragma omp critical
#endif // _OPENMP
            {
                hardwareModel->selectHardwareMesh( m->hardwareMeshId );

                if ( hardwareModel->getBoneCount() > 1 )
                {
                    throw std::runtime_error( "more than one bone in rigid mesh" );
                }

                // -- Get bone matrix --
                if ( hardwareModel->getBoneCount() == 1 )
                {
                    rotationBoneSpace =
                        hardwareModel->getRotationBoneSpace( 0, calModel->getSkeleton() );
                    translationBoneSpace =
                        hardwareModel->getTranslationBoneSpace( 0, calModel->getSkeleton() );
                }
                else // hardwareModel->getBoneCount() == 0
                {
                    // do not touch default no rotation & zero translation
                }
            }

            CalMatrix       rotationMatrix = rotationBoneSpace;
            GLfloat         rotation[9];
            GLfloat         translation[3];

            rotation[0] = rotationMatrix.dxdx;
            rotation[1] = rotationMatrix.dxdy;
            rotation[2] = rotationMatrix.dxdz;
            rotation[3] = rotationMatrix.dydx;
            rotation[4] = rotationMatrix.dydy;
            rotation[5] = rotationMatrix.dydz;
            rotation[6] = rotationMatrix.dzdx;
            rotation[7] = rotationMatrix.dzdy;
            rotation[8] = rotationMatrix.dzdz;

            translation[0] = translationBoneSpace.x;
            translation[1] = translationBoneSpace.y;
            translation[2] = translationBoneSpace.z;

            mt->setMatrix( osg::Matrixf( rotation[0]   , rotation[3]   , rotation[6]   , 0,
                                         rotation[1]   , rotation[4]   , rotation[7]   , 0,
                                         rotation[2]   , rotation[5]   , rotation[8]   , 0,
                                         translation[0], translation[1], translation[2], 1 ) );
        }
    }
}

void
Model::blendCycle( int id,
                   float weight,
                   float delay )
{
    getCalModel()->getMixer()->blendCycle( id, weight, delay );
}

void
Model::clearCycle( int id,
                   float delay )
{
    getCalModel()->getMixer()->clearCycle( id, delay );
}

osg::Geometry*
Model::getMesh( const std::string& name ) const throw (std::runtime_error)
{
    MeshMap::const_iterator i = meshes.find( name );

    if ( i != meshes.end() )
    {
        return i->second.get();
    }
    else
    {
        throw std::runtime_error( "Model::getMesh - can't find mesh \"" + name + "\"" );
    }
}
