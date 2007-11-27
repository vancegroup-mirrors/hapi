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
/// \file HLDepthBufferShape.cpp
/// \brief cpp file for HLDepthBufferShape
///
//
//////////////////////////////////////////////////////////////////////////////

#include <HAPI/HLDepthBufferShape.h>
#include <GL/glew.h>

#ifdef HAVE_OPENHAPTICS

using namespace HAPI;

void HLDepthBufferShape::hlRender( HAPI::HAPIHapticsDevice *hd,
                                   HLuint hl_shape_id ) {

  if( OpenHapticsRenderer::surfaceSupported( surface.get() ) ) {
    //&& closeEnoughToBound( hd->proxyPosition->getValue(),  
    //hd->getPreviousProxyPosition(),
    //(Matrix4f)transform.inverse(), 
    //geometry ) ) {
    glMatrixMode( GL_MODELVIEW );
    glPushMatrix();
#if HL_VERSION_MAJOR_NUMBER >= 2
    hlPushAttrib( HL_MATERIAL_BIT | HL_TOUCH_BIT );
#endif
    const Matrix4 &m = transform;
    GLdouble vt[] = { m[0][0], m[1][0], m[2][0], 0,
		     m[0][1], m[1][1], m[2][1], 0,
		     m[0][2], m[1][2], m[2][2], 0,
		     m[0][3], m[1][3], m[2][3], 1 };
    glLoadIdentity();
    glScalef( 1e3f, 1e3f, 1e3f ); 
    glMultMatrixd( vt );
    OpenHapticsRenderer::hlRenderHAPISurface( surface.get() );

    hlTouchableFace( HL_FRONT_AND_BACK );
    Matrix3 m3 = m.getScaleRotationPart();

    if( touchable_face == Collision::BACK ) hlTouchableFace( HL_BACK );
    else if( touchable_face == Collision::FRONT ) hlTouchableFace( HL_FRONT );
    else if( touchable_face == Collision::FRONT_AND_BACK )
      hlTouchableFace( HL_FRONT_AND_BACK );

    if( use_haptic_camera )
      hlEnable( HL_HAPTIC_CAMERA_VIEW );
    else
      hlDisable( HL_HAPTIC_CAMERA_VIEW );

    if( use_adaptive_viewport )
      hlEnable( HL_ADAPTIVE_VIEWPORT );
    else
      hlDisable( HL_ADAPTIVE_VIEWPORT );

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

    hlBeginShape( HL_SHAPE_DEPTH_BUFFER, hl_shape_id );
    glClear( GL_DEPTH_BUFFER_BIT );
    gl_shape->glRender();
    //    geometry->hlRender( hd, (Matrix4f)transform );
    hlEndShape();

    glFrontFace( front_face );
#if HL_VERSION_MAJOR_NUMBER >= 2
    hlPopAttrib();
#endif
    glPopMatrix();
  }
}

#endif
