#include "cinder/app/AppBasic.h"
//#include "cinder/gl/GlslProg.h"
#include "cinder/TriMesh.h"
#include "cinder/gl/Texture.h"
#include "cinder/gl/gl.h"
#include "cinder/Camera.h"
#include "cinder/params/Params.h"
#include "cinder/Utilities.h"
#include "cinder/ImageIo.h"
#include "cinder/Rand.h"
#include "Kinect.h"
#include "Resources.h"

static const int KINECT_X_RES = 640;
static const int KINECT_Y_RES = 480;

using namespace ci;
using namespace ci::app;
using namespace std;

class kinectMesh : public AppBasic {
  public:
	void prepareSettings( Settings* settings );
	void setup();
	void updateMesh();
	void update();
	void draw();
	
	// PARAMS
	params::InterfaceGl	mParams;
	float     avgFramerate;
	float			mScale;
	float			mXOff, mYOff;
  int       mMinDepth, mMaxDepth;
	float			mTexOffsetX, mTexOffsetY;
  bool      showTexture;
  bool      showInfrared, setShowInfrared;
  bool      showWireframe, setShowWireframe;
	
	// CAMERA
	CameraPersp		mCam;
	Quatf			mSceneRotation;
	Vec3f			mEye, mCenter, mUp;
	float			mCameraDistance;
	float			mKinectTilt;
	
	// KINECT AND TEXTURES
	Kinect                    mKinect;
  gl::Texture               mImageTexture;
  std::shared_ptr<uint16_t> mDepthData;
  
  int       MESH_DIV;
  int       setMeshDiv;
  int       MESH_X_RES;
  int       MESH_Y_RES;
  
  float*    depthToMeters;
  float     depthScale;
	
	// VBO AND SHADER
  //gl::GlslProg	mShader;
  TriMesh     mMesh;	

};

void kinectMesh::prepareSettings( Settings* settings )
{
	settings->setWindowSize( 1280, 720 );
}

void kinectMesh::setup()
{	
	// SETUP PARAMS
	mParams = params::InterfaceGl( "Sembiki Kinect Mesh", Vec2i( 200, 300 ) );
	mParams.addParam( "Kinect Tilt", &mKinectTilt, "min=-31 max=31 keyIncr=q keyDecr=a" );
  
	mParams.addParam( "Scene Rotation", &mSceneRotation, "opened=1" );
	mParams.addParam( "Cam Distance", &mCameraDistance, "min=100.0 max=5000.0 step=10.0 keyIncr=s keyDecr=w" );
	mParams.addParam( "Min Depth", &mMinDepth, "min=0 max=65000 step=100 keyIncr=e keyDecr=d" );
	mParams.addParam( "Max Depth", &mMaxDepth, "min=0 max=65000 step=100 keyIncr=r keyDecr=f" );
	mParams.addParam( "Depth Scale", &depthScale, "min=0 max=5 step=.01 keyIncr=t keyDecr=g" );
  
	mParams.addParam( "Mesh Resolution", &setMeshDiv, "min=1 max=100 step=1 keyIncr=u keyDecr=j" );
	mParams.addParam( "Show Wireframe", &setShowWireframe, "" );
	mParams.addParam( "Show Texture", &showTexture, "" );
	mParams.addParam( "Show Infrared", &setShowInfrared, "" );
	mParams.addParam( "RGB Offset X", &mTexOffsetX, "min=-.5 max=.5 step=.001 keyIncr=i keyDecr=k" );
	mParams.addParam( "RGB Offset Y", &mTexOffsetY, "min=-.5 max=.5 step=.001 keyIncr=o keyDecr=l" );

  mParams.addParam( "Framerate", &avgFramerate, "min=0 max=120 step=1" );

  avgFramerate = 0.0;
	
	// SETUP CAMERA
	mCameraDistance = 1000.0f;
	mEye			= Vec3f( 0.0f, 0.0f, mCameraDistance );
	mCenter			= Vec3f::zero();
	mUp				= Vec3f::yAxis();
	mCam.setPerspective( 75.0f, getWindowAspectRatio(), 1.0f, 8000.0f );
  mKinectTilt = 0;
	
	// SETUP KINECT AND TEXTURES
	console() << "There are " << Kinect::getNumDevices() << " Kinects connected." << std::endl;
	mKinect			= Kinect( Kinect::Device() ); // use the default Kinect
  mMinDepth = 0;
  mMaxDepth = 65000;
  depthScale = 1;
  mTexOffsetX = -0.024;
  mTexOffsetY = 0.038;
  showTexture = true;
  showInfrared = false;
  setShowInfrared = false;
  showWireframe = false;
  setShowWireframe = false;
  
  // Depth lookup table
  depthToMeters = new float[mMaxDepth];
  for (int i=0; i<mMaxDepth; ++i) {
//    float hmm = (float)i / (float)mMaxDepth * 2047.0f; // Translate to 0-2047 range
//    depthToMeters[i] = 1.0f / (hmm * -0.0030711016f + 3.3309495161f);
    depthToMeters[i] = (float)i/50;
  }
	
  //mShader	= gl::GlslProg( loadResource( RES_VERT_ID ), loadResource( RES_FRAG_ID ) );
  MESH_DIV = 2;
  setMeshDiv = 2;
  MESH_X_RES = KINECT_X_RES/MESH_DIV;
  MESH_Y_RES = KINECT_Y_RES/MESH_DIV;
	
	// SETUP GL
	gl::enableDepthWrite();
	gl::enableDepthRead();

}

void kinectMesh::updateMesh()
{

  for (int y=0; y<MESH_Y_RES; ++y) {
    for (int x=0; x<MESH_X_RES; ++x) {
      
      int depth = mDepthData.get()[(y*MESH_DIV)*KINECT_X_RES+(x*MESH_DIV)];
      
      float zPos = (float)mMinDepth;
      if (mMinDepth <= depth && depth <= mMaxDepth ) {
        zPos = depthToMeters[depth] * depthScale;
      }
      mMesh.appendVertex( Vec3f((float)x/(float)MESH_X_RES*800.0f-400.0f, 300.0f-(float)y/(float)MESH_Y_RES*600.0f, zPos) );
      if (showInfrared)
        mMesh.appendTexCoord( Vec2f((float)x/(float)MESH_X_RES, (float)y/(float)MESH_Y_RES) );
      else
        mMesh.appendTexCoord( Vec2f((float)x/(float)MESH_X_RES+mTexOffsetX, (float)y/(float)MESH_Y_RES+mTexOffsetY) );
      
      if (x>0 && y>0) {
        // Two triangles per square
        int idx = mMesh.getNumVertices() - 1;
        mMesh.appendTriangle( idx-1-MESH_X_RES, idx-MESH_X_RES, idx );
        mMesh.appendTriangle( idx-1-MESH_X_RES, idx-1, idx );
      }
      
    }
  }
    
}

void kinectMesh::update()
{
	if( mKinectTilt != mKinect.getTilt() )
		mKinect.setTilt( mKinectTilt );
	
  if( !mKinect.checkNewDepthFrame() ) 
    return;
  
//  mDepthSurface = mKinect.getDepthImage();
  mDepthData = mKinect.getDepthData();
  
  if (showInfrared != setShowInfrared) {
    showInfrared = setShowInfrared;
    mKinect.setVideoInfrared(showInfrared);
  }
  
  if (showWireframe != setShowWireframe) {
    showWireframe = setShowWireframe;
    if (showWireframe)
      gl::enableWireframe();
    else
      gl::disableWireframe();
  }
  
  if (MESH_DIV != setMeshDiv) {
    MESH_DIV = setMeshDiv;
    MESH_X_RES = KINECT_X_RES/MESH_DIV;
    MESH_Y_RES = KINECT_Y_RES/MESH_DIV;
  }
  
	if( showTexture && mKinect.checkNewVideoFrame() )
		mImageTexture = gl::Texture(mKinect.getVideoImage());
		
  // Update mesh with new webcam capture frame
  mMesh.clear();
  updateMesh();
  
  // update camera information
	mEye	= Vec3f( 0.0f, 0.0f, mCameraDistance );
  mCam.lookAt( mEye, mCenter, mUp );
	gl::setMatrices( mCam );
	gl::rotate( mSceneRotation );
}

void kinectMesh::draw()
{
	gl::clear( Color( 0.0f, 0.0f, 0.0f ), true );
  if (showTexture && mImageTexture) {
    mImageTexture.enableAndBind();
    gl::draw(mMesh);
    mImageTexture.unbind();
  } else {
    gl::draw(mMesh);
  }
  
  avgFramerate = getAverageFps();
	params::InterfaceGl::draw();
}


CINDER_APP_BASIC( kinectMesh, RendererGl )
