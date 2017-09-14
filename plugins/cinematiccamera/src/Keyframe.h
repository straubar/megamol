/*
 *Keyframe.h
 *
 */

#ifndef MEGAMOL_CINEMATICCAMERA_KEYFRAME_H_INCLUDED
#define MEGAMOL_CINEMATICCAMERA_KEYFRAME_H_INCLUDED
#if (defined(_MSC_VER) && (_MSC_VER > 1000))
#pragma once
#endif /* (defined(_MSC_VER) && (_MSC_VER > 1000)) */

#include "CinematicCamera/CinematicCamera.h"

#include "vislib/graphics/Camera.h"
#include "vislib/math/Point.h"
#include "vislib/Serialisable.h"
#include "vislib/math/Vector.h"

namespace megamol {
	namespace cinematiccamera {

		class Keyframe{

		public:

			/** CTOR */
            Keyframe();
            Keyframe(vislib::graphics::Camera c, float t);

			/** DTOR */
			~Keyframe();

            /** */
			vislib::graphics::Camera getCamera(){
				return this->camera;
			}

            /** */
			void setCamera(vislib::graphics::Camera c) {
				this->camera = c;
			}

            /** */
			float getTime(){
				return time;
			}

            /** */
			void setTime(float t) {
				this->time = t;
			}

            /** */
            // Compare the relevant values and not just the pointer to the camera parameters
			bool operator==(Keyframe const& rhs){
				return ((this->camera.Parameters()->Position() == rhs.camera.Parameters()->Position()) &&
                    (this->camera.Parameters()->LookAt() == rhs.camera.Parameters()->LookAt()) &&
                    (this->camera.Parameters()->Up() == rhs.camera.Parameters()->Up()) &&
                    (this->camera.Parameters()->ApertureAngle() == rhs.camera.Parameters()->ApertureAngle()) &&
                    (this->time == rhs.time));
			}
            /*
            bool operator==(Keyframe const& rhs) {
                return ((this->camera == rhs.camera) && (this->time == rhs.time));
            }
            */

            /** */
            // Compare the relevant values and not just the pointer to the camera parameters
            bool operator!=(Keyframe const& rhs) {
                return ((this->camera.Parameters()->Position() != rhs.camera.Parameters()->Position()) ||
                    (this->camera.Parameters()->LookAt() != rhs.camera.Parameters()->LookAt()) ||
                    (this->camera.Parameters()->Up() != rhs.camera.Parameters()->Up()) ||
                    (this->camera.Parameters()->ApertureAngle() != rhs.camera.Parameters()->ApertureAngle()) ||
                    (this->time != rhs.time));
            }
            /*
            bool operator!=(Keyframe const& rhs) {
                return (!(this->camera == rhs.camera) || (this->time != rhs.time));
            }
            */

            /** */
			vislib::math::Point<FLOAT, 3> getCamPosition(){
				return this->camera.Parameters()->Position();
			}

            /** */
			vislib::math::Point<FLOAT, 3> getCamLookAt(){
				return this->camera.Parameters()->LookAt();
			}

            /** */
			vislib::math::Vector<FLOAT, 3> getCamUp(){
				return this->camera.Parameters()->Up();
			}

            /** */
			float getCamApertureAngle(){
				return this->camera.Parameters()->ApertureAngle();
			}
			
            /** */
			vislib::SmartPtr<vislib::graphics::CameraParameters> getCamParameters(){
				return this->camera.Parameters();
			}

            /** */
			void setCameraPosition(vislib::math::Point <float, 3> pos){
                this->camera.Parameters()->SetPosition(pos);
			}

            /** */
			void setCameraLookAt(vislib::math::Point <float, 3> look){
                this->camera.Parameters()->SetLookAt(look);
			}

            /** */
			void setCameraUp(vislib::math::Vector<float, 3> up){
                this->camera.Parameters()->SetUp(up);
			}

            /** */
			void setCameraApertureAngele(float appertureAngle){
                this->camera.Parameters()->SetApertureAngle(appertureAngle);
			}

            /** */
			void setCameraParameters(vislib::SmartPtr<vislib::graphics::CameraParameters> params){
                this->camera.SetParameters(params);
			}			

		private:

            /**********************************************************************
            * variables
            **********************************************************************/

			vislib::graphics::Camera camera;
			float                    time;

		};

	} /* end namespace cinematiccamera */
} /* end namespace megamol */
#endif /* MEGAMOL_CINEMATICCAMERA_KEYFRAME_H_INCLUDED */