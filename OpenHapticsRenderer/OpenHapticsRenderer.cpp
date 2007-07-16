//////////////////////////////////////////////////////////////////////////////
//    Copyright 2004, SenseGraphics AB
//
//    This file is part of H3D API.
//
//    H3D API is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    H3D API is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with H3D API; if not, write to the Free Software
//    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//    A commercial license is also available. Please contact us at 
//    www.sensegraphics.com for more information.
//
//
/// \file OpenHapticsRenderer.cpp
/// \brief cpp file for OpenHapticsRenderer.
///
//
//////////////////////////////////////////////////////////////////////////////

#include "OpenHapticsRenderer.h"
#include "H3DMath.h"

#include "PhantomHapticsDevice.h"
#include "AnyHapticsDevice.h"
#include <FrictionSurface.h>

#ifdef HAVE_OPENHAPTICS
#include <HD/hd.h>
#if defined(_MSC_VER) || defined(__BORLANDC__)
#pragma comment( lib, "hl.lib" )
#pragma comment( lib, "hlu.lib" )
#endif

using namespace HAPI;

HAPIHapticsRenderer::HapticsRendererRegistration 
OpenHapticsRenderer::renderer_registration(
                            "OpenHaptics",
                            &(newInstance< OpenHapticsRenderer >)
                            );
namespace OpenHapticsRendererInternals {
  string getHLErrorString( HLerror error ) {
    if( error.errorCode == HL_DEVICE_ERROR ) {
      stringstream s;
      s << "HL_DEVICE_ERROR( " 
        << hdGetErrorString( error.errorInfo.errorCode )
        << " )";
      return s.str();
    } else {
      return error.errorCode;
    }
  }
}


OpenHapticsRenderer::OpenHapticsSurface::OpenHapticsSurface(  
                          HAPIFloat _stiffness,
                          HAPIFloat _damping,
                          HAPIFloat _static_friction,
                          HAPIFloat _dynamic_friction,
                          bool _magnetic,
                          HAPIFloat _snap_distance ):
  stiffness( _stiffness ),
  damping( _damping ),
  static_friction( _static_friction ),
  dynamic_friction( _dynamic_friction ),
  magnetic( _magnetic ),
  snap_distance( _snap_distance ) {
}

void OpenHapticsRenderer::OpenHapticsSurface::hlRender() {
  hlRenderRelative( stiffness,
                    damping,
                    static_friction,
                    dynamic_friction, 
                    magnetic,
                    snap_distance );
}


/// Initialize the renderer to be used with the given haptics device.
void OpenHapticsRenderer::initRenderer( HAPIHapticsDevice *hd ) {
  HHLRC haptic_context = initHLLayer( hd );
  if( haptic_context ) {
    context_map[ hd ] = haptic_context;
  }
}

/// Release all resources that has been used in the renderer for
/// the given haptics device.
void OpenHapticsRenderer::releaseRenderer( HAPIHapticsDevice *hd ) {
  ContextMap::iterator i = context_map.find( hd );
  if( i != context_map.end() ) {
    hlMakeCurrent( NULL );
    hlDeleteContext( context_map[ hd ] );
    context_map.erase( i );
  }

  // Ugly solution to clean up stuff correctly when the device is removed.
  // TODO: Find a better solution.
  OpenHapticsWorkAroundToCleanUpHLContext * temp_ptr =
    new OpenHapticsWorkAroundToCleanUpHLContext();
  temp_ptr->dummy_context = dummy_context;
  clean_up_stuff[ hd ].push_back( temp_ptr );

  callback_data.clear();
  for( IdMap::iterator i = id_map.begin();
       i != id_map.end();
       i++ ) {
    HLuint hl_shape_id = (*i).second; 
    hlDeleteShapes( hl_shape_id, 1 );
    hlRemoveEventCallback( HL_EVENT_MOTION, 
                           hl_shape_id,
                           HL_CLIENT_THREAD,
                           &motionCallback );
    hlRemoveEventCallback( HL_EVENT_TOUCH, 
                           hl_shape_id,
                           HL_CLIENT_THREAD,
                           &touchCallback );
    hlRemoveEventCallback( HL_EVENT_UNTOUCH, 
                           hl_shape_id,
                           HL_CLIENT_THREAD,
                           &untouchCallback );
  }
  id_map.clear();
}

HAPIForceEffect::EffectOutput 
OpenHapticsRenderer::renderHapticsOneStep( 
                     HAPIHapticsDevice *hd,
                     const HapticShapeVector &shapes ) {
  PhantomHapticsDevice *pd = dynamic_cast< PhantomHapticsDevice * >( hd );
  if( !pd ) return HAPIForceEffect::EffectOutput();

  hdMakeCurrentDevice( pd->getDeviceHandle() );

  // add the resulting force and torque to the rendered force.
  HDdouble force[3];
  hdGetDoublev( HD_CURRENT_FORCE, force );
  
  HDdouble torque[3];
  hdGetDoublev( HD_CURRENT_TORQUE, torque );

  // add the resulting force and torque to the rendered force.
    
  return HAPIForceEffect::EffectOutput( Vec3( force[0], force[1], force[2] ), 
                                        Vec3( torque[0], torque[1], torque[2] ) );
}


void OpenHapticsRenderer::preProcessShapes( HAPIHapticsDevice *hd,
                                            const HapticShapeVector &shapes ) {
  HHLRC haptic_context = NULL;

  if( context_map.find( hd ) == context_map.end() ) {
    haptic_context = initHLLayer( hd );
  } else {
    haptic_context = context_map[ hd ];
  }

  if( !haptic_context ) return;

  glMatrixMode( GL_MODELVIEW );
  glPushMatrix();
  glLoadIdentity();
  glClear(GL_DEPTH_BUFFER_BIT);   
  hlMatrixMode( HL_VIEWTOUCH );
  hlLoadIdentity();
  hlMatrixMode( HL_TOUCHWORKSPACE );
  hlLoadIdentity();

  // TODO: fix matrices
  //hlPushMatrix();

  const Matrix4 &pcal = hd->getPositionCalibrationInverse();
  HLdouble m[16] = { pcal[0][0], pcal[1][0], pcal[2][0], pcal[3][0], 
                     pcal[0][1], pcal[1][1], pcal[2][1], pcal[3][1], 
                     pcal[0][2], pcal[1][2], pcal[2][2], pcal[3][2], 
                     pcal[0][3], pcal[1][3], pcal[2][3], pcal[3][3] }; 
  hlLoadIdentity();

  // apply calibration matrix
  hlMultMatrixd( m );
  
  hlMakeCurrent( haptic_context );  
  hlBeginFrame();
  hlCheckEvents();
  
  for( HapticShapeVector::const_iterator i = shapes.begin();
       i != shapes.end();
         i++ ) {
    HLuint hl_shape_id = getHLShapeId( *i, hd );
    
    HLShape *hl = dynamic_cast< HLShape * >( *i );
    if( hl ) {    
      hl->hlRender( hd, hl_shape_id );
    } else {
      if( surfaceSupported( (*i)->surface ) ) {
        OpenHapticsOptions::ShapeType shape_type = default_gl_shape;
        bool camera_view = default_haptic_camera_view;
        bool adaptive_viewport = default_adaptive_viewport;
        
        OpenHapticsRenderer::OpenHapticsOptions *options;
        (*i)->getRenderOption( options );
          if( options ) {
            shape_type = options->shape_type;
            camera_view = options->use_haptic_camera_view;
            adaptive_viewport = options->use_adaptive_viewport;
          }
          
          glMatrixMode( GL_MODELVIEW );
          glPushMatrix();
#if HL_VERSION_MAJOR_NUMBER >= 2
          hlPushAttrib( HL_MATERIAL_BIT | HL_TOUCH_BIT );
#endif

          glLoadIdentity();
          hlRenderHAPISurface( (*i)->surface );

          HLenum touchable_face;
          Bounds::FaceType face = (*i)->touchable_face;
          if( face == Bounds::BACK ) touchable_face = HL_BACK;
          else if( face == Bounds::FRONT ) touchable_face = HL_FRONT;
          else touchable_face = HL_FRONT_AND_BACK;
          
          hlTouchableFace( touchable_face );

          if( adaptive_viewport )
            hlEnable( HL_ADAPTIVE_VIEWPORT );
          else
            hlDisable( HL_ADAPTIVE_VIEWPORT );

          if( camera_view )
            hlEnable( HL_HAPTIC_CAMERA_VIEW );
          else
            hlDisable( HL_HAPTIC_CAMERA_VIEW );
          
          Matrix3 m3 = (*i)->transform.getScaleRotationPart();
          GLint front_face;
          
          bool negative_scaling = 
            ( ( m3.getRow( 0 ) % m3.getRow( 1 ) ) * m3.getRow(2) ) < 0;
          glGetIntegerv( GL_FRONT_FACE, &front_face );
          // if front_face == GL_CW then the GLWindow we render to is mirrored.
          // front_face has to be flipped each time a negative scaling is applied
          // in order for normals to be correct. 
          if( front_face == GL_CW ) {
            if( !negative_scaling )
              glFrontFace( GL_CCW );
          } else if( negative_scaling )
            glFrontFace( GL_CW );
          
          if( shape_type == OpenHapticsOptions::DEPTH_BUFFER ) {
            hlBeginShape( HL_SHAPE_DEPTH_BUFFER, hl_shape_id );
            glClear( GL_DEPTH_BUFFER_BIT );
            (*i)->glRender();
            hlEndShape();
          } else if( shape_type == OpenHapticsOptions::FEEDBACK_BUFFER ) {
            int nr_vertices = (*i)->nrVertices();
            if( nr_vertices == -1 )
              hlHinti( HL_SHAPE_FEEDBACK_BUFFER_VERTICES, 65536 );
            else
              hlHinti( HL_SHAPE_FEEDBACK_BUFFER_VERTICES, nr_vertices );

            hlBeginShape( HL_SHAPE_FEEDBACK_BUFFER, hl_shape_id );
            (*i)->glRender();
            hlEndShape();         
          } else {
            // custom shape, use intersection functions
            hlBeginShape(HL_SHAPE_CALLBACK, hl_shape_id );
            hlCallback(HL_SHAPE_INTERSECT_LS, 
                       (HLcallbackProc) intersectCallback, (*i) );
            hlCallback(HL_SHAPE_CLOSEST_FEATURES, 
                       (HLcallbackProc) closestFeaturesCallback, (*i) );
            hlEndShape();
          }
          
#if HL_VERSION_MAJOR_NUMBER >= 2
          hlPopAttrib();
#endif
          glPopMatrix();
      } else {
        H3DUtil::Console(2) 
          <<  "Surface type not supported by OpenHapticsRenderer." << endl;
      }
    } 
  }
  glMatrixMode( GL_MODELVIEW );
  glPopMatrix();

  hlEndFrame(); 
  //hlMatrixMode( HL_VIEWTOUCH );
  //hlPopMatrix();
  //hlMatrixMode( HL_TOUCHWORKSPACE );
  //hlPopMatrix();
    
  // check for any errors
  HLerror error;
  while ( HL_ERROR(error = hlGetError()) ) {
    H3DUtil::Console(4) << OpenHapticsRendererInternals::getHLErrorString( error )
                        << endl;
    }   
}


HHLRC OpenHapticsRenderer::initHLLayer( HAPIHapticsDevice *hd ) {
  PhantomHapticsDevice *pd = dynamic_cast< PhantomHapticsDevice * >( hd );
  if( !pd ) {
    AnyHapticsDevice *d = dynamic_cast< AnyHapticsDevice *>( hd );
    if( d ) {
      pd = dynamic_cast< PhantomHapticsDevice * >( d->getActualHapticsDevice() );
    }
    if( !pd ) return NULL;
  }

  if( pd->getDeviceState() != HAPIHapticsDevice::UNINITIALIZED ) {
    if( context_map.find( pd ) == context_map.end() ) {
      // Create a haptic context for the device. The haptic context maintains 
      // the state that persists between frame intervals and is used for
      // haptic rendering.
      HHD jj = pd->getDeviceHandle();
      context_map[ pd ] = hlCreateContext( pd->getDeviceHandle() );
      if( !dummy_context )
        dummy_context = hlCreateContext( pd->getDeviceHandle() );
      hlMakeCurrent( context_map[ pd ] );  

      hlEnable(HL_HAPTIC_CAMERA_VIEW);
      hlEnable(HL_ADAPTIVE_VIEWPORT );
      return context_map[ pd ];
    }
  }
  return NULL;
}

HAPI::Vec3 OpenHapticsRenderer::getProxyPosition() {
  HLdouble pos[3];
  hlGetDoublev( HL_PROXY_POSITION, pos );
  return HAPI::Vec3( pos[0], pos[1], pos[2] );
}

void HLCALLBACK OpenHapticsRenderer::motionCallback( HLenum event,
                                                     HLuint object,
                                                     HLenum thread,
                                                     HLcache *cache,
                                                     void *userdata ) {
  CallbackData *cb_data = static_cast< CallbackData * >( userdata ); 

  OpenHapticsRenderer *renderer = cb_data->renderer;
  HAPIHapticShape *shape = cb_data->shape.get();
  
  HLdouble n[3], p[3];
  hlCacheGetDoublev( cache, 
                     HL_PROXY_POSITION,
                     p );
  hlCacheGetDoublev( cache, 
                     HL_PROXY_TOUCH_NORMAL,
                     n );
  HLdouble hlforce[3];
  hlGetShapeDoublev( object, HL_REACTION_FORCE, hlforce );

  HAPI::Vec3 cp = HAPI::Vec3( p[0], p[1], p[2] );
  HAPI::Vec3 cn = HAPI::Vec3( n[0], n[1], n[2] );
  HAPI::Vec3 f  = HAPI::Vec3( hlforce[0], hlforce[1], hlforce[2] );
  for( Contacts::iterator i = renderer->contacts.begin();
       i != renderer->contacts.end(); i++ ) {
    if( (*i).first.get()->shape_id == shape->shape_id ) {
      if( (*i).first.get() != shape ) {
        (*i).first.reset( shape );
      }
      HAPISurfaceObject::ContactInfo &ci = (*i).second;
      ci.contact_point_global = cp;
      ci.y_axis = cn;
      ci.force_global = f;
      return;
    }
  }
  // should always have a contact with the shape
  assert( 0 );
}

void HLCALLBACK OpenHapticsRenderer::touchCallback( HLenum event,
                                                HLuint object,
                                                HLenum thread,
                                                HLcache *cache,
                                                void *userdata ) {
  CallbackData *cb_data = static_cast< CallbackData * >( userdata ); 
  
  OpenHapticsRenderer *renderer = cb_data->renderer;
  HAPIHapticShape *shape = cb_data->shape.get();

  renderer->contacts.push_back( make_pair( shape, HAPISurfaceObject::ContactInfo()) );
  OpenHapticsRenderer::motionCallback( event, object, thread, cache, userdata );
}

void HLCALLBACK OpenHapticsRenderer::untouchCallback( HLenum event,
                                                  HLuint object,
                                                  HLenum thread,
                                                  HLcache *cache,
                                                  void *userdata ) {
  CallbackData *cb_data = static_cast< CallbackData * >( userdata ); 
  OpenHapticsRenderer *renderer = cb_data->renderer;
  HAPIHapticShape *shape = cb_data->shape.get();
  Contacts::iterator to_remove = renderer->contacts.end();

  Contacts::iterator i;
  for( i = renderer->contacts.begin();
       i != renderer->contacts.end(); i++ ) {
    if( (*i).first.get()->shape_id == shape->shape_id ) {
      to_remove = i;
      break;
    }
  }

  assert( i != renderer->contacts.end() );
  renderer->contacts.erase( i );
}


HLuint OpenHapticsRenderer::getHLShapeId( HAPIHapticShape *hs,
                                          HAPIHapticsDevice *hd ) {
  pair< int, HAPIHapticsDevice * > key = make_pair( hs->shape_id, hd );

  if( id_map.find( key ) == id_map.end() ) {
    HLuint hl_shape_id = hlGenShapes(1);
    id_map[ key ] = hl_shape_id;
  
    CallbackData *cb_data = new CallbackData( this, hs );
    callback_data.push_back( cb_data );
    hlEventd(  HL_EVENT_MOTION_LINEAR_TOLERANCE, 0 );
    
    
    hlAddEventCallback( HL_EVENT_MOTION, 
                        hl_shape_id,
                        HL_CLIENT_THREAD,
                        &motionCallback,
                        cb_data );
    hlAddEventCallback( HL_EVENT_TOUCH, 
                        hl_shape_id,
                        HL_CLIENT_THREAD,
                        &touchCallback,
                        cb_data );
    hlAddEventCallback( HL_EVENT_UNTOUCH, 
                        hl_shape_id,
                        HL_CLIENT_THREAD,
                        &untouchCallback,
                        cb_data );
  } else {
    for( int i = 0; i < callback_data.size(); i++ ) {
      if( callback_data[i]->shape->shape_id == hs->shape_id
          && callback_data[i]->renderer == this ) {
        callback_data[i]->shape.reset( hs );
      }
    }
  }
  return id_map[ key ];
}

HLboolean HLCALLBACK OpenHapticsRenderer::intersectCallback( 
                                      const HLdouble *start_point, 
                                      const HLdouble *end_point,
                                      HLdouble *intersection_point, 
                                      HLdouble *intersection_normal,
                                      HLenum* hl_face,
                                      void *user_data ) {
  
  HAPIHapticShape* object = 
    static_cast<HAPIHapticShape*>( user_data );
  Bounds::IntersectionInfo i;

  HLboolean b = object->lineIntersect( Vec3(start_point[0], 
                                            start_point[1], 
                                            start_point[2] ), 
                                       Vec3(end_point[0],
                                            end_point[1],
                                            end_point[2] ),
                                       i, 
                                       Bounds::FRONT_AND_BACK );
  intersection_point[0] = i.point.x;
  intersection_point[1] = i.point.y;
  intersection_point[2] = i.point.z;

  if( b && i.face == Bounds::BACK ) {
      i.normal = -i.normal;
  }

  intersection_normal[0] = i.normal.x;
  intersection_normal[1] = i.normal.y;
  intersection_normal[2] = i.normal.z;

  *hl_face = (i.face == Bounds::FRONT ? HL_FRONT: HL_BACK );
  
  return b;
}


HLboolean HLCALLBACK OpenHapticsRenderer::closestFeaturesCallback( 
                                       const HLdouble *query_point, 
                                       const HLdouble *target_point, 
                                       HLgeom *geom,
                                       HLdouble *closest_point,
                                       void* user_data ) {
  HAPIHapticShape* object = 
    static_cast<HAPIHapticShape*>( user_data );

  Vec3 qp( (HAPIFloat)query_point[0],
           (HAPIFloat)query_point[1],
           (HAPIFloat)query_point[2] );

  Vec3 closest, normal, tex_coord;
  object->closestPoint( qp, closest, normal, tex_coord );
  // TODO: do we want the normal of the geometry or the normal below
  //Vec3 normal = qp - closest;
  //normal.normalizeSafe();

  HLdouble cn[] = { normal.x, 
                    normal.y,
                    normal.z };
  closest_point[0] = closest.x;
  closest_point[1] = closest.y;
  closest_point[2] = closest.z;

  hlLocalFeature2dv( geom, HL_LOCAL_FEATURE_PLANE, 
                     cn, closest_point );

  return true;
}


/// Renders a HAPISurface object with OpenHaptics. Returns true if it 
/// succeeded, false otherwise. Not all surface types are valid.
bool OpenHapticsRenderer::hlRenderHAPISurface( HAPISurfaceObject *s ) {
  if( HLSurface *hl_surface = dynamic_cast< HLSurface * >( s ) ) {
    hl_surface->hlRender();
    return true;
  }
  if( FrictionSurface *friction_surface =
      dynamic_cast< FrictionSurface * >( s ) ) {
    HAPI::OpenHapticsRenderer::hlRenderAbsolute(
      friction_surface->stiffness,
      friction_surface->damping,
      friction_surface->static_friction,
      friction_surface->dynamic_friction,
      false,
      0 );
    return true;
  }
  return false;
}

bool OpenHapticsRenderer::surfaceSupported( HAPISurfaceObject *s ) {
  return dynamic_cast< HLSurface * >( s ) != NULL ||
         dynamic_cast< FrictionSurface * >( s ) != NULL;
}

/// Sets up the surface parameters for HL API. All values are given in
/// values between 0 and 1(except snapDistance which is in mm) just
/// as normally done in OpenHaptics. If you want to specify absolute
/// values instead use hlRenderAbsolute instead.
void OpenHapticsRenderer::hlRenderRelative( HAPIFloat stiffness,
                                            HAPIFloat damping,
                                            HAPIFloat static_friction,
                                            HAPIFloat dynamic_friction,
                                            bool magnetic,
                                            HAPIFloat snap_distance ) {
  if( magnetic ) hlTouchModel( HL_CONSTRAINT );
  else hlTouchModel( HL_CONTACT );

  hlMaterialf(HL_FRONT_AND_BACK, HL_STIFFNESS, (HLfloat)stiffness );
  hlMaterialf(HL_FRONT_AND_BACK, HL_DAMPING, (HLfloat)damping );
  hlMaterialf(HL_FRONT_AND_BACK, HL_DYNAMIC_FRICTION, (HLfloat)dynamic_friction );
  hlMaterialf(HL_FRONT_AND_BACK, HL_STATIC_FRICTION, (HLfloat)static_friction );
  // convert to millimeters
  hlTouchModelf( HL_SNAP_DISTANCE, snap_distance );
}

/// Sets up the surface parameters for HL API. 
/// TODO: Fix comment
/// stiffness is given as N/mm
/// damping as ...
/// ..
void OpenHapticsRenderer::hlRenderAbsolute( HAPIFloat stiffness,
                                            HAPIFloat damping,
                                            HAPIFloat static_friction,
                                            HAPIFloat dynamic_friction,
                                            bool magnetic,
                                            HAPIFloat snap_distance ) {
  hlRenderRelative( stiffness / 0.7,
                    damping / 0.7,
                    static_friction,
                    dynamic_friction,
                    magnetic,
                    snap_distance );
}

void OpenHapticsRenderer::OpenHapticsWorkAroundToCleanUpHLContext::cleanUp() {
  if( dummy_context ) {
    hlDeleteContext( dummy_context );
    dummy_context = NULL;
  }
}

#endif
