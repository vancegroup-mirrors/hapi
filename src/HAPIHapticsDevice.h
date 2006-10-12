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
/// \file HAPIHapticsDevice.h
/// \brief Header file for HAPIHapticsDevice.
///
//
//////////////////////////////////////////////////////////////////////////////

#ifndef __HAPIHAPTICSDEVICE_H__
#define __HAPIHAPTICSDEVICE_H__

#include <HAPI.h>
#include <HAPIHapticShape.h>
#include <RuspiniRenderer.h>
#include <HapticForceEffect.h>
#include <Threads.h>
#include <AutoRefVector.h>

namespace HAPI {

  /// \class HAPIHapticsDevice
  /// Base class for all haptic devices. 
  class HAPI_API HAPIHapticsDevice {
  public:
    typedef enum {
      UNINITIALIZED, /// the device has not been intialized 
      INITIALIZED,   /// the device has been initialized but not enabled
      ENABLED        /// the device has been initialized and enabled
    } DeviceState;
    
    typedef enum {
      SUCCESS = 0,   
      NOT_INITIALIZED, /// the device has not been initialized
      NOT_ENABLED,     /// the device has not been enabled
      FAIL             /// the operation has failed for another reason
      /// use getLastErrorMsg() to get info about the error.
    } ErrorCode;

    /// Values that are
    struct HAPI_API DeviceValues {
      DeviceValues():
        button_status( 0 ),
        user_data( NULL ) {}

      Vec3 force;      /// The force currently being rendered
      Vec3 torque;     /// The torque currently being rendered
      Vec3 position;   /// The position of the haptics device
      Vec3 velocity;   /// The velocity of the haptics device.
      Rotation orientation; /// The orientation of the haptics device.
      HAPIInt32 button_status; /// The status of the buttons.
      void *user_data;   /// Extra data that can be used be developers when
      /// supporting new device types.
    };

    typedef H3DUtil::AutoRefVector< HAPIHapticShape > HapticShapeVector;
    typedef H3DUtil::AutoRefVector< HapticForceEffect > HapticEffectVector;

    /// Constructor.
    HAPIHapticsDevice() :
      device_state( UNINITIALIZED ) {
      setHapticsRenderer( new RuspiniRenderer );
    }
    
    /// Destructor. Stops haptics rendering and remove callback functions.
    virtual ~HAPIHapticsDevice() {}

    ////////////////////////////////////////////////////////////////////
    // Device handling functions.
    //

    /// Does all the initialization needed for the device before starting to
    /// use it.
    inline ErrorCode initDevice() {
      if( device_state == DeviceState::UNINITIALIZED ) {
        last_device_values = current_device_values = DeviceValues();
        last_raw_device_values = current_raw_device_values = DeviceValues();
        if( !initHapticsDevice() ) {
          return FAIL;
        }
      } 
      device_state = DeviceState::INITIALIZED;
      thread->asynchronousCallback( hapticRenderingCallback,
                                    this );
      return SUCCESS;
    }

    /// Enable the device. Positions can be read and force can be sent.
    inline ErrorCode enableDevice() {
      if( device_state == DeviceState::UNINITIALIZED ) {
        return ErrorCode::NOT_INITIALIZED;
      }

      device_state = DeviceState::ENABLED;
      return SUCCESS;
    }

    /// Temporarily disable the device. Forces sent will be ignored and
    /// positions and orientation will stay the same as previous values.
    inline ErrorCode disableDevice() {
      if( device_state == DeviceState::UNINITIALIZED ) {
        return ErrorCode::NOT_INITIALIZED;
      }

      sendForce( Vec3( 0, 0, 0 ) );
      sendTorque( Vec3( 0, 0, 0 ) );
                 
      device_state = DeviceState::INITIALIZED;
      return SUCCESS;
    }

    /// Perform cleanup and let go of all device resources that are allocated.
    /// After a call to this function no haptic rendering can be performed on
    /// the device until the initDevice() function has been called again.
    inline ErrorCode releaseDevice() {
      if( device_state == DeviceState::UNINITIALIZED ) {
        return ErrorCode::NOT_INITIALIZED;
      }
      
      //if( device_state != DeviceState::INITIALIZED ) 
        if( !releaseHapticsDevice() ) {
          return FAIL;
        }
      device_state = DeviceState::UNINITIALIZED;
      return SUCCESS;
    }      

    ////////////////////////////////////////////////////////////////////
    // Functions for adding and removing shapes to be rendered by the
    // haptics renderer
    //

    /// TODO: permanent shapes??

    /// Add a HAPIHapticShape to be rendered haptically.
    /// \param objects The haptic shapes to render.
    inline void addShape( HAPIHapticShape *shape ) {
      shape_lock.lock();
      tmp_shapes.push_back( shape );
      shape_lock.unlock();
    }
    
    
    /// Set the HapticForceEffects to be rendered.
    /// \param objects The haptic shapes to render.
    inline void setShapes( const HapticShapeVector &shapes ) {
      tmp_shapes = shapes;
    }

    /// Remove a HAPIHapticShape from the shapes being rendered.
    inline void removeShape( HAPIHapticShape *shape ) {
      shape_lock.lock();
      tmp_shapes.erase( shape );
      shape_lock.unlock();
    }
    
    /// Add all shapes between [begin, end)
    template< class InputIterator > 
    inline void addShapes( InputIterator begin, InputIterator end ) {
      shape_lock.lock();
      tmp_shapes.insert( tmp_shapes.end(), begin, end );
      shape_lock.unlock();
    }

    /// Swap the vector of shapes currently being rendered with the
    /// given vector, replacing all shapes being rendered.
    inline void swapShapes( HapticShapeVector &shapes ) {
      shape_lock.lock();
      tmp_shapes.swap( shapes );
      shape_lock.unlock();
    }

    /// Remove all HAPIHapticShape objects that are currently being rendered.
    inline void clearShapes() {
      shape_lock.lock();
      tmp_shapes.clear();
      shape_lock.unlock();
    }

    /// Add a HapticForceEffect to be rendered.
    /// \param objects The haptic shapes to render.
    inline void addEffect( HapticForceEffect *effect ) {
      current_force_effects.push_back( effect );
    }

    /// Set the HapticForceEffects to be rendered.
    /// \param objects The haptic shapes to render.
    inline void setEffects( const HapticEffectVector &effects ) {
      current_force_effects = effects;
    }

    /// Remove a force effect so that it is not rendered any longer.
    inline void removeEffect( HapticForceEffect *effect ) {
      current_force_effects.erase( effect );
    }

    /// Add all effects between [begin, end)
    template< class InputIterator > 
    inline void addEffects( InputIterator begin, InputIterator end ) {
      current_force_effects.insert( current_force_effects.end(), begin, end );
    }

    /// Swap the vector of effects currently being rendered with the
    /// given vector, replacing all effects being rendered.
    inline void swapEffects( HapticEffectVector &effects ) {
      current_force_effects.swap( effects );
    }

    /// Remove all HAPIHapticShape objects that are currently being rendered.
    inline void clearEffects() {
      current_force_effects.clear();
    }
    

    //////////////////////////////////////////////////////////////
    // Functions for getting/settings device values
    //
    
    /// Set the position calibraion matrix, i.e. the transform matrix
    /// from the local device coordinate space to HAPI coordinate space.
    inline void setPositionCalibration( const Matrix4 &m,
                                        const Matrix4 &m_inv ) {
      device_values_lock.lock();
      position_calibration = m;
      position_calibration_inverse = m_inv;
      device_values_lock.unlock();
    }

    /// Set the position calibration matrix, i.e. the transform matrix
    /// from the local device coordinate space to HAPI coordinate space.
    inline void setPositionCalibration( const Matrix4 &m ) {
      device_values_lock.lock();
      position_calibration = m;
      position_calibration_inverse = m.inverse();
      device_values_lock.unlock();
    }

    /// Get the current position calibration matrix    
    inline const Matrix4 &getPositionCalibration() { 
      return position_calibration;
    }

    /// Get the inverse of the current position calibration matrix    
    inline const Matrix4 &getPositionCalibrationInverse() { 
      return position_calibration_inverse;
    }

    /// Set the orientation calibration.
    inline void setOrientationCalibration( const Rotation &r ) {
      device_values_lock.lock();
      orientation_calibration = r;
      device_values_lock.unlock();
    }

    
    /// Get the current orientation calibration.
    inline const Rotation &getOrientationCalibration() {
      return orientation_calibration;
    }

    /// Get the current device state.
    inline DeviceState getDeviceState() {
      return device_state;
    }

    /// Get the current device values in device coordinates.
    inline DeviceValues getRawDeviceValues() {
      device_values_lock.lock();
      DeviceValues dv = current_raw_device_values;
      device_values_lock.unlock();
      return dv;
    }

    /// Get the device values from the last loop in device coordinates.
    inline DeviceValues getLastRawDeviceValues() {
      device_values_lock.lock();
      DeviceValues dv = last_raw_device_values;
      device_values_lock.unlock();
      return dv;
    }

    /// Get the current device values in world coordinates.
    inline DeviceValues getDeviceValues() {
      device_values_lock.lock();
      DeviceValues dv = current_device_values;
      device_values_lock.unlock();
      return dv;
    }
    
    /// Get the device values from the last loop in world coordinates.
    inline DeviceValues getLastDeviceValues() {
      device_values_lock.lock();
      DeviceValues dv = last_device_values;
      device_values_lock.unlock();
      return dv;
    }

    /// Get the position of the haptics device without the calibration
    /// matrix applied, i.e. coordinates in metres.
    inline Vec3 getRawPosition() {
      return getRawDeviceValues().position;
    }

    /// Get the velocity of the haptics device without the calibration
    /// matrix applied. 
    inline Vec3 getRawVelocity() {
      return getRawDeviceValues().velocity;
    }

    /// Get the orientation of the haptics device without the calibration
    /// matrix applied. 
    inline Rotation getRawOrientation() {
      return getRawDeviceValues().orientation;
    }

    /// Get the position of the haptics device with the calibration matrix 
    /// applied. 
    inline Vec3 getPosition() {
      return getDeviceValues().position;
    }

    /// Get the velocity of the haptics device with the calibration matrix 
    /// applied. 
    inline Vec3 getVelocity() {
      return getDeviceValues().velocity;
    }

    /// Get the orientation of the haptics device with the calibration matrix 
    /// applied. 
    inline Rotation getOrientation() {
      return getDeviceValues().orientation;
    }

    /// Get the button status. Bit 0 is button 0, bit 1 is button 1,..
    /// A 1 in the bit position indicates that the button is pressed.
    inline HAPIInt32 getButtonStatus() {
      return getDeviceValues().button_status;
    }

    /// Get the button status for a specified button. true means
    /// that is is pressed.
    inline bool getButtonStatus( unsigned int button_nr ) {
      return (getButtonStatus() & (1 << button_nr)) != 0;
    }

    /// Get the force currently rendered by the haptics device in device
    /// coordinates.
    inline Vec3 getRawForce() {
      return getRawDeviceValues().force;
    }

    /// Get the torque currently rendered by the haptics device in device 
    /// coordinates.
    inline Vec3 getRawTorque() {
      return getRawDeviceValues().torque;
    }

    /// Get the force currently rendered by the haptics device in world
    /// coordinates.
    inline Vec3 getForce() {
      return getDeviceValues().force;
    }

    /// Get the torque currently rendered by the haptics device in world 
    /// coordinates.
    inline Vec3 getTorque() {
      return getDeviceValues().torque;
    }

    /// Get the position of the haptics device from the last loop in device
    /// coordinates. 
    inline Vec3 getLastRawPosition() {
      return getLastRawDeviceValues().position;
    }

    /// Get the velocity of the haptics device from the last loop in device
    /// coordinates. 
    inline Vec3 getLastRawVelocity() {
      return getLastRawDeviceValues().velocity;
    }

    /// Get the orientation of the haptics device from the last loop in device
    /// coordinates. 
    inline Rotation getLastRawOrientation() {
      return getLastRawDeviceValues().orientation;
    }

    /// Get the force rendered by the haptics device in the last loop in device
    /// coordinates.
    inline Vec3 getLastRawForce() {
      return getLastRawDeviceValues().force;
    }

    /// Get the torque rendered by the haptics device in the last loop in 
    /// device coordinates.
    inline Vec3 getLastRawTorque() {
      return getLastRawDeviceValues().torque;
    }

    /// Get the position of the haptics device from the last loop in world
    /// coordinates. 
    inline Vec3 getLastPosition() {
      return getLastDeviceValues().position;
    }
    
    /// Get the velocity of the haptics device from the last loop in world
    /// coordinates. 
    inline Vec3 getLastVelocity() {
      return getLastDeviceValues().velocity;
    }

    /// Get the orientation of the haptics device from the last loop in world
    /// coordinates. 
    inline Rotation getLastOrientation() {
      return getLastDeviceValues().orientation;
    }

    /// Get the force rendered by the haptics device in the last loop in world
    /// coordinates.
    inline Vec3 getLastForce() {
      return getLastDeviceValues().force;
    }

    /// Get the torque rendered by the haptics device in the last loop in world
    /// coordinates.
    inline Vec3 getLastTorque() {
      return getLastDeviceValues().torque;
    }

    /// Get the button status from the last loop.
    inline HAPIInt32 getLastButtonStatus() {
      return getLastDeviceValues().button_status;
    }
    
    /// Get the button status from the last loop.
    inline bool getLastButtonStatus( unsigned int button_nr ) {
      return (getLastButtonStatus() & (1 << button_nr)) != 0;
    }

    /// Set the HAPIHapticsRenderer to use to render the HAPIHapticShapes
    /// specified.
    inline void setHapticsRenderer( HAPIHapticsRenderer *r ) {
      haptics_renderer.reset( r );
    }

    /// Get the currently used HAPIHapticsRenderer.
    inline HAPIHapticsRenderer *getHapticsRenderer() {
      return haptics_renderer.get();
    }

    /// Get the error message from the latest error.
    inline const string &getLastErrorMsg() {
      return last_error_message;
    }

  protected:

    /// Send the force to render on the haptics device in device coordinates. 
    inline void sendRawForce( const Vec3 &f ) {
      if( DeviceState::ENABLED ) {
        device_values_lock.lock();
        output.force = f;
        device_values_lock.unlock();
      }
    }

    /// Send the torque to render on the haptics device in device coordinates. 
    inline void sendRawTorque( const Vec3 &t ) {
      if( DeviceState::ENABLED ) {
        device_values_lock.lock();
        output.torque = t;
        device_values_lock.unlock();
      }
    }

    /// Send the force to render on the haptics device  in world coordinates. 
    inline void sendForce( const Vec3 &f ) {
      if( DeviceState::ENABLED ) {
        device_values_lock.lock();
        output.force = position_calibration_inverse * f;
        device_values_lock.unlock();
      }
    }

    /// Send the torque to render on the haptics device in world coordinates. 
    inline void sendTorque( const Vec3 &t ) {
      if( DeviceState::ENABLED ) {
        device_values_lock.lock();
        output.torque = position_calibration_inverse.getRotationPart() * t;
        device_values_lock.unlock();
      }
    }

    /// Output that is sent to the haptics device to render.
    struct HAPI_API DeviceOutput {
      Vec3 force;  /// The force in Newtons
      Vec3 torque; /// The torque in Newtons/mm
      Matrix4 position_force_jacobian;
    };

    /// Updates the current_device_values member to contain
    /// current values.
    inline void updateDeviceValues() {
      if( device_state == DeviceState::ENABLED ) {
        DeviceValues dv;
        updateDeviceValues( dv, 0 );
        
        device_values_lock.lock();
        last_device_values = current_device_values;
        last_raw_device_values = current_raw_device_values;
        current_raw_device_values = dv;
        current_device_values.position = position_calibration * dv.position;
        current_device_values.velocity = 
          position_calibration.getScaleRotationPart() * dv.velocity;
        current_device_values.orientation = 
          orientation_calibration * dv.orientation;
        current_device_values.force = 
          position_calibration.getScaleRotationPart() * dv.force;
        current_device_values.torque = 
          position_calibration.getScaleRotationPart() * dv.torque;
        device_values_lock.unlock();
      } else {
        if( device_state == DeviceState::INITIALIZED ) {
          device_values_lock.lock();
          last_device_values = current_device_values;
          last_raw_device_values = current_raw_device_values;
          device_values_lock.unlock();
        }
      }
    }

    /// Sends the data in the output member to be rendered at
    /// the haptics device.
    inline void sendOutput() {
      if( device_state == DeviceState::ENABLED ) {
        device_values_lock.lock();
        sendOutput( output, 0 );
        device_values_lock.unlock();
      }
    }

    /// Sets an error message that can be accessed later through
    /// getLastErrorMsg()
    inline void setErrorMsg( const string &err ) {
      last_error_message = err;
    }

    /// This function should be overriden by all subclasses of 
    /// HAPIHapticsDevice in order to fill in the given DeviceValues
    /// structure with current values of the devie.
    virtual void updateDeviceValues( DeviceValues &dv,
                                     HAPITime dt ) {
      dv.force = output.force;
      dv.torque = output.torque;
    }

    /// This function should be overridden by all subclasses of 
    /// HAPIHapticsDevice in order to send the values in the
    /// given DeviceOutput structure to the haptics device.
    virtual void sendOutput( DeviceOutput &dv,
                             HAPITime dt ) = 0;

    /// Initialize the haptics device.
    virtual bool initHapticsDevice() = 0;

    /// Release all resources allocated to the haptics device.
    virtual bool releaseHapticsDevice() = 0;

    /// The thread that this haptics device loop is run in.
    PeriodicThreadBase *thread;

    // the force effects that are currently rendered in the realtime loop.
    // Should not be changed directly from the scenegraph loop but instead
    // use the renderEffects function to set the effects.
    HapticEffectVector current_force_effects;

    // the force effects that was current in the last scene graph loop.
    HapticEffectVector last_force_effects;

    // the shapes that are currently being rendered in the realtime loop.
    HapticShapeVector current_shapes;

    // the values to send to the haptics device.
    DeviceOutput output;

    // lock for when changing the shapes to be rendered
    MutexLock shape_lock;

    // lock for when updating device values/sending output
    MutexLock device_values_lock;

    //
    HapticShapeVector tmp_shapes;

    unsigned int nr_haptics_loops;

    /// The time at the beginning of the last rendering loop
    TimeStamp last_loop_time;

    DeviceState device_state;
    DeviceValues current_device_values;
    DeviceValues last_device_values;
    DeviceValues current_raw_device_values;
    DeviceValues last_raw_device_values;

    Matrix4 position_calibration;
    Matrix4 position_calibration_inverse;
    Rotation orientation_calibration;
    auto_ptr< HAPIHapticsRenderer > haptics_renderer;
    string last_error_message;
    string device_name;

    /// Callback function to render force effects.
    static PeriodicThread::CallbackCode hapticRenderingCallback( void *data );
  };
}

#endif