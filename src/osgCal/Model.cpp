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

#include <osg/Notify>
#include <osg/NodeCallback>
#include <osg/Timer>
#include <osg/Geode>
#include <osg/MatrixTransform>
#include <osg/io_utils>

#include <osgCal/Model>
#include <osgCal/HardwareMesh>
#include <osgCal/SoftwareMesh>

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
            
            double deltaTime = 0;

            if (!nv->getFrameStamp())
            {
                osg::Timer_t current = timer.tick();
                deltaTime = timer.delta_s(previous, current);
                previous = current;
            }
            else
            {
                double time = nv->getFrameStamp()->getSimulationTime();
                deltaTime = time - prevTime;
                prevTime = time;
            }

            //std::cout << "CalUpdateCallback: " << deltaTime << std::endl;
            if ( deltaTime > 0.0 )
            {
                model->update( deltaTime );
            }
            //std::cout << "CalUpdateCallback: ok" << std::endl;

            traverse(node, nv); // may be needed for user nodes
//            node->dirtyBound(); <- is it necessary?
        }

    private:

        osg::Timer timer;
        osg::Timer_t previous;
        double prevTime;

};

Model::Model()
{
    setDataVariance( DYNAMIC ); // we can add or remove objects dynamically
}

Model::Model( const Model&, const osg::CopyOp& )
    : Group() // to eliminate warning
{
    throw std::runtime_error( "Model copying is not supported" );
}

Model::~Model()
{
    setUpdateCallback( 0 );
}

// static
// osg::Drawable*
// linesDrawable( osg::Vec3Array* lines,
//                const osg::Vec3& color )
// {
//     osg::Geometry* g = new osg::Geometry;
//     osg::Vec3Array* vcolor = new osg::Vec3Array;
//     vcolor->push_back( color );
//     g->setVertexArray( lines );
//     g->setColorArray( vcolor );
//     g->setColorBinding( osg::Geometry::BIND_OVERALL );
//     g->addPrimitiveSet( new osg::DrawArrays( osg::PrimitiveSet::LINES, 0, lines->size() ) );
//     g->getOrCreateStateSet()->setMode(GL_LIGHTING,osg::StateAttribute::OVERRIDE|osg::StateAttribute::OFF);;
//     return g;
// }

void
Model::load( CoreModel*      _coreModel,
             BasicMeshAdder* _meshAdder )
{
    if ( modelData.valid() )
    {
        throw std::runtime_error( "Model already load" );
    }

    modelData = new ModelData( _coreModel, this );

    setUpdateCallback( new CalUpdateCallback() );

    osg::ref_ptr< BasicMeshAdder > meshAdder( _meshAdder ? _meshAdder :
                                              new DefaultMeshAdder );

    // -- Process meshes --
    const CoreModel::MeshVector& meshes = modelData->getCoreModel()->getMeshes();
    
    for ( size_t i = 0; i < meshes.size(); i++ )
    {
        meshAdder->add( this, meshes[i].get() );
    }

//     // -- TBN debug --
//     if ( coreModel->getFlags() & CoreModel::SHOW_TBN )
//     {
//         osg::Vec3Array* t = new osg::Vec3Array;
//         osg::Vec3Array* b = new osg::Vec3Array;
//         osg::Vec3Array* n = new osg::Vec3Array;
//         const NormalBuffer& vb = *(coreModel->getVertexBuffer());
//         const NormalBuffer& tb = *(coreModel->getTangentBuffer());
//         const NormalBuffer& bb = *(coreModel->getBinormalBuffer());
//         const NormalBuffer& nb = *(coreModel->getNormalBuffer());
//         const float scale = 1.0f;

//         for ( size_t i = 0; i < vb.size(); i++ )
//         {
//             if ( tb[i].length2() > 0 )
//             {
//                 t->push_back( vb[i] );
//                 b->push_back( vb[i] );
//                 n->push_back( vb[i] );
//                 t->push_back( vb[i] + tb[i]*scale );
//                 b->push_back( vb[i] + bb[i]*scale );
//                 n->push_back( vb[i] + nb[i]*scale );
//             }
//         }

//         geode->addDrawable( linesDrawable( t, osg::Vec3( 1.0, 0.0, 0.0 ) ) );
//         geode->addDrawable( linesDrawable( b, osg::Vec3( 0.0, 1.0, 0.0 ) ) );
//         geode->addDrawable( linesDrawable( n, osg::Vec3( 0.0, 0.0, 1.0 ) ) );
//     }

    updatableMeshes.swap( updatableMeshes ); // trim vector
}


Mesh*
Model::addMesh( const CoreMesh* mesh )
{
    Mesh* g = 0;

    // -- Create mesh drawable --
    if ( mesh->parameters->software )
    {
        g = new SoftwareMesh( modelData.get(), mesh );
    }
    else
    {
        g = new HardwareMesh( modelData.get(), mesh );
    }

    // -- Remember drawable --
    meshes[ mesh->data->name ].push_back( g );

    // -- Remember Updatable (and update) --
    if ( mesh->data->rigid == false )
    {
        g->update();
        updatableMeshes.push_back( g );
    }
    else
    {        
        nonUpdatableMeshes.push_back( g );
    }

    addMeshDrawable( mesh, g );

    return g;
}

template < typename T >
void
removeElement( std::vector< T* >& v,
               T*                 e )
{
    typename std::vector< T* >::iterator i =
        std::find( v.begin(), v.end(), e );

    if ( i == v.end() )
    {
        throw std::runtime_error( "osgCal. removeElement: element not found" );
    }
    else
    {
        v.erase( i );
    }
}

void
Model::removeMesh( Mesh* mesh )
{
    // -- UnRemember drawable --
    removeElement( meshes[ mesh->getCoreMesh()->data->name ], mesh );

    // -- Remember Updatable (and update) --
    if ( mesh->getCoreMesh()->data->rigid == false )
    {
        removeElement( updatableMeshes, mesh );
    }
    else
    {        
        removeElement( nonUpdatableMeshes, mesh );
    }

    removeMeshDrawable( mesh->getCoreMesh(), mesh );
}

void
Model::addMeshDrawable( const CoreMesh* mesh,
                        osg::Drawable*  drawable )
{
    // -- Add to geode or bone matrix transform --
    if ( mesh->data->rigid == false // deformable
         || (mesh->data->rigid && mesh->data->rigidBoneId == -1) // unrigged
        )
    {
        if ( !geode.valid() )
        {
            geode = new osg::Geode;
            geode->setDataVariance( DYNAMIC ); // we can add or remove nodes dynamically
            addChild( geode.get() );
        }
        
        geode->addDrawable( drawable );
    }
    else
    {
        addDrawable( mesh->data->rigidBoneId, drawable );
    }
}

void
Model::removeMeshDrawable( const CoreMesh* mesh,
                           osg::Drawable*  drawable )
{
    if ( mesh->data->rigid == false // deformable
         || (mesh->data->rigid && mesh->data->rigidBoneId == -1) // unrigged
        )
    {
        geode->removeDrawable( drawable );
        if ( geode->getNumDrawables() == 0 )
        {
            removeChild( geode.get() );
            geode = 0;
        }
    }
    else
    {
        removeDrawable( mesh->data->rigidBoneId, drawable );
    }
}

void
Model::addDepthMesh( DepthMesh* depthMesh )
{
    addMeshDrawable( depthMesh->getHardwareMesh()->getCoreMesh(),
                     depthMesh );
}

void
Model::removeDepthMesh( DepthMesh* depthMesh )
{
    removeMeshDrawable( depthMesh->getHardwareMesh()->getCoreMesh(),
                        depthMesh );
}

Model::TransformAndGeode&
Model::getOrCreateTransformAndGeode( int boneId )
{
    // for rigged rigid meshes we use bone
    // transform w/o per-vertex transformations
    RigidTransformsMap::iterator
        boneMT = rigidTransforms.find( boneId );

    if ( boneMT != rigidTransforms.end() )
    {
        // use ready bone matrix transform
        return boneMT->second;
    }
    else
    {
        // create new matrix transform for bone
        osg::MatrixTransform* mt = new osg::MatrixTransform;

        mt->setDataVariance( DYNAMIC );
        // ^ we can add or remove nodes dynamically, plus we don't
        // allow osgUtil::Optimizer to optimize out our transform
        mt->setMatrix( modelData->getBoneMatrix( boneId ) );

        addChild( mt );

        return rigidTransforms.insert(
            RigidTransformsMap::value_type( boneId,
                                            std::make_pair( mt, (osg::Geode*)NULL ) )
            ).first->second;
        // we only create MatrixTransform here, no geode is added, hence NULL
    }
}

void
Model::addNode( int boneId,
                osg::Node* node )
{
    TransformAndGeode& tg = getOrCreateTransformAndGeode( boneId );
    tg.first->addChild( node );
}

void
Model::removeNode( int boneId,
                   osg::Node* node )
{
    TransformAndGeode& tg = getOrCreateTransformAndGeode( boneId );
    tg.first->removeChild( node );

    if ( tg.first->getNumChildren() == 0 )
    {
        removeChild( tg.first );

        rigidTransforms.erase( boneId );
    }
}

void
Model::addDrawable( int boneId,
                    osg::Drawable* drawable )
{
    TransformAndGeode& tg = getOrCreateTransformAndGeode( boneId );

    if ( tg.second == NULL ) // no geode created yet
    {
        tg.second = new osg::Geode;
        tg.second->setDataVariance( DYNAMIC ); // we can add or remove nodes dynamically
        tg.first->addChild( tg.second );
    }

    tg.second->addDrawable( drawable );
}

void
Model::removeDrawable( int boneId,
                       osg::Drawable* drawable )
{
    TransformAndGeode& tg = getOrCreateTransformAndGeode( boneId );

    if ( tg.second == NULL ) // no geode created yet
    {
        throw std::runtime_error( "Model::removeDrawable -- no drawable was added" );
    }

    tg.second->removeDrawable( drawable );
    if ( tg.second->getNumDrawables() == 0 )
    {
        tg.first->removeChild( tg.second );
        geode = 0;
    }

    if ( tg.first->getNumChildren() == 0 )
    {
        removeChild( tg.first );

        rigidTransforms.erase( boneId );
    }
}

void
Model::update( double deltaTime ) 
{
    if ( modelData->update( deltaTime ) == false )
    {
        return;
    }

#ifdef _OPENMP
#pragma omp parallel for
#endif // _OPENMP
    for ( std::vector< Mesh* >::iterator
              u    = updatableMeshes.begin(),
              uEnd = updatableMeshes.end();
          u < uEnd; ++u )
    {
        (*u)->update();
    }

    for ( RigidTransformsMap::iterator
              t    = rigidTransforms.begin(),
              tEnd = rigidTransforms.end();
          t != tEnd; ++t )
    {
        if ( modelData->getBoneParams( t->first ).changed )
        {
            t->second.first->setMatrix( modelData->getBoneMatrix( t->first ) );
        }
    }
}

void
Model::blendCycle( int id,
                   float weight,
                   float delay )
{
    modelData->getCalMixer()->blendCycle( id, weight, delay );
}

void
Model::clearCycle( int id,
                   float delay )
{
    modelData->getCalMixer()->clearCycle( id, delay );
}

const CoreModel*
Model::getCoreModel() const
{
    return modelData->getCoreModel();
}

const Model::MeshesList&
Model::getMeshes( const std::string& name ) const throw (std::runtime_error)
{
    MeshMap::const_iterator i = meshes.find( name );

    if ( i != meshes.end() )
    {
        return i->second;
    }
    else
    {
        throw std::runtime_error( "Model::getMeshes - can't find mesh \"" + name + "\"" );
    }
}

void
Model::accept( osg::NodeVisitor& nv )
{
    osgUtil::GLObjectsVisitor* glv = dynamic_cast< osgUtil::GLObjectsVisitor* >( &nv );

    if ( glv )
    {
        osg::notify( osg::INFO )
            << "compiling shaders and display lists" << std::endl;

        // -- Compile shaders --
        for ( std::vector< Mesh* >::iterator
                  h    = updatableMeshes.begin(),
                  hEnd = updatableMeshes.end();
              h != hEnd; ++h )
        {
            (*h)->accept( glv );
        }

        for ( std::vector< Mesh* >::iterator
                  h    = nonUpdatableMeshes.begin(),
                  hEnd = nonUpdatableMeshes.end();
              h != hEnd; ++h )
        {
            (*h)->accept( glv ); // <- is it needed, they don't change state set
        }
    }

    osg::Group::accept( nv );        
}

// -- MeshAdder --

void
DefaultMeshAdder::add( Model* model,
                       const CoreMesh* mesh )
{
    model->addMesh( mesh );
}


// -- ModelData --

ModelData::ModelData( CoreModel* cm,
                      Model*     m )
    : coreModel( cm )
    , model( m )
{
    calModel = new CalModel( coreModel->getCalCoreModel() );
    calModel->update( 0 );
    calMixer = (CalMixer*)calModel->getAbstractMixer();

    const std::vector< CalBone* >& vectorBone = calModel->getSkeleton()->getVectorBone();

    bones.resize( vectorBone.size() );

    std::vector< CalBone* >::const_iterator b = vectorBone.begin();        
    for ( BoneParamsVector::iterator
              bp = bones.begin(),
              bpEnd = bones.end();
          bp < bpEnd; ++bp, ++b )
    {
        bp->bone = *b;
    }
}

ModelData::~ModelData()
{
    delete calModel;
}

Model*
ModelData::getModel()
    throw (std::runtime_error)
{
    if ( model.valid() )
    {
        return model.get();
    }
    else
    {
        throw std::runtime_error( "ModelData::getModel() -- model was already deleted" );
    }
}

inline
float
square( float x )
{
    return x*x;
}

bool
ModelData::update( float deltaTime )
{
    // -- Update calMixer & skeleton --
    if ( //calMixer->getAnimationVector().size() == <total animations count>
         calMixer->getAnimationActionList().size() == 0 &&
         calMixer->getAnimationCycle().size() == 0 )
    {
        return false; // no animations, nothing to update
    }

    calMixer->updateAnimation( deltaTime ); 
    calMixer->updateSkeleton();

    // -- Update bone parameters --
    bool anythingChanged = false;
    for ( BoneParamsVector::iterator
              b    = bones.begin(),
              bEnd = bones.end();
          b < bEnd; ++b )
    {
        const CalQuaternion& rotation = b->bone->getRotationBoneSpace();
        const CalVector&     translation = b->bone->getTranslationBoneSpace();
        const CalMatrix&     rm = b->bone->getTransformMatrix();

        const osg::Matrix3   r( rm.dxdx, rm.dydx, rm.dzdx,
                                rm.dxdy, rm.dydy, rm.dzdy,
                                rm.dxdz, rm.dydz, rm.dzdz );
        const osg::Vec3f     t( translation.x, translation.y, translation.z );

        // -- Check for deformed --
        b->deformed =
            // cal3d reports nonzero translations for non-animated models
            // and non zero quaternions (seems like some FP round-off error). 
            // So we must check for deformations using some epsilon value.
            // Problem:
            //   * It is cal3d that must return correct values, no epsilons
            // But nevertheless we use this to reduce CPU load.

            t.length() > /*boundingBox.radius() **/ 1e-5 // usually 1e-6 .. 1e-7
            ||
            osg::Vec3d( rotation.x,
                        rotation.y,
                        rotation.z ).length() > 1e-6 // usually 1e-7 .. 1e-8
            ;

        // -- Check for changes --
        float s = 0;
        for ( int j = 0; j < 9; j++ )
        {
            s += square( r[j] - b->rotation[j] );
        }
        s += ( t - b->translation ).length2();

        if ( s < 1e-7 ) // usually 1e-11..1e-12
        {
            b->changed = false;
        }
        else
        {
            b->changed = true;
            anythingChanged = true;
            b->rotation = r;
            b->translation = t;
        }
        
//         std::cout << "bone: " << b->bone->getCoreBone()->getName() << std::endl
//                   << "quaternion: "
//                   << rotation.x << ' '
//                   << rotation.y << ' '
//                   << rotation.z << ' '
//                   << rotation.w << std::endl
//                   << "translation: "
//                   << t.x() << ' ' << t.y() << ' ' << t.z()
//                   << "; len = " << t.length()
//                   << "; s = " << s // << std::endl
//                   << "; changed = " << b->changed // << std::endl
//                   << "; deformed = " << b->deformed // << std::endl
// //                  << "len / bbox.radius = " << translation.length() / boundingBox.radius()
//                   << std::endl;
    }

    return anythingChanged;
}
