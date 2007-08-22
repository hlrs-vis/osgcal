                _____     __
 ___  ___ ___ _/ ___/__ _/ /
/ _ \(_-</ _ `/ /__/ _ `/ / 
\___/___/\_, /\___/\_,_/_/
        /___/
*===========================*

osgCal is an adapter to use cal3d (http://home.gna.org/cal3d/) inside an
OSG (http://www.openscenegraph.com/) tree.

Requirements:

* Cal3D (version 0.11.0 or higher)
* OSG   (version 2.0.0 or higher)

Features:

* Completely wraps Cal3D so that users doesn't need to know the cal3d API. 
  But the simplified API is similar to Cal3D one to ease the use for Cal3D
  users.
* Handles several animations, meshes and materials in a single model. Offers
  an API for changing between different animations without knowing anything
  about cal3d.
* Uses GLSL hardware skinning, yet supporting OSG picking.
* Can be switched to fixed function implementation.
* Supports normal mapped, two-sided & transparent meshes.
* Uses different shaders (with minimum of instructions) for different
  materials.
* Calculates deformations only when bone positions are changed.
* Uses non-skinning shader for fast drawing of non-deformed meshes. 
* Puts each submesh inside a different osg::Drawable to take advantage of
  the OSG state sorting.

How to build:

* type the following commands:

    make
    make install

* If you want to install into /usr/ instead of /usr/local, use this instead:

    make install DESTDIR=/usr

After installation you get:

* libosgCal{.so|.dll} -- the library itself.

* osgCalViewer[.exe] -- model viewer:

    osgCalViewer cal3d.cfg   - view model
    osgCalViewer <...>.cmf   - view one mesh from model
    osgCalViewer <...>.caf   - view specified animation
    osgCalViewer --help      - for additional command line parameters

  In the viewer itself you can switch animations using '1', '2', etc. keys.
  Also you can press 'h' key to see other available key combinations.

* osgCalPreparer[.exe] -- vertex buffers cache preparer. Can be used
  on hi-poly models to speedup subsequent loading times.
  When running:

    osgCalPreparer cal3d.cfg

  it creates two cache files (`cal3d.cfg.vbos.cache' and
  `cal3d.cfg.hwmodel.cache') which later used when loading model.
