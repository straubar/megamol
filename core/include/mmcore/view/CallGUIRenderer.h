/*
 * CallGUIRenderer.h
 *
 * Copyright (C) 2019 by VISUS (Universitaet Stuttgart).
 * Alle Rechte vorbehalten.
 */

#ifndef MEGAMOLCORE_CALLGUIRENDERER_H_INCLUDED
#define MEGAMOLCORE_CALLGUIRENDERER_H_INCLUDED
#if (defined(_MSC_VER) && (_MSC_VER > 1000))
#pragma once
#endif /* (defined(_MSC_VER) && (_MSC_VER > 1000)) */


#include "mmcore/api/MegaMolCore.std.h"
#include "mmcore/view/InputCall.h"
#include "mmcore/factories/CallAutoDescription.h"

#include "vislib/math/Rectangle.h"


namespace megamol {
namespace core {
namespace view {

    /**
     * ...
     */
    class MEGAMOLCORE_API CallGUIRenderer : public InputCall {
    public:

        /**
         * Answer the name of the objects of this description.
         *
         * @return The name of the objects of this description.
         */
        static const char *ClassName(void) {
            return "CallGUIRenderer";
        }

        /**
         * Gets a human readable description of the module.
         *
         * @return A human readable description of the module.
         */
        static const char *Description(void) {
            return "Call the GUIRenderer";
        }

		/** Function index of 'render' */
        static const unsigned int FnRender = 5;

        /**
         * Answer the number of functions used for this call.
         *
         * @return The number of functions used for this call.
         */
        static unsigned int FunctionCount(void) {
            ASSERT(FnRender == InputCall::FunctionCount() && "Enum has bad magic number");
            return InputCall::FunctionCount() + 1;
        }

        /**
         * Answer the name of the function used for this call.
         *
         * @param idx The index of the function to return it's name.
         *
         * @return The name of the requested function.
         */
		static const char* FunctionName(unsigned int idx) {
            #define CaseFunction(id) case Fn##id: return #id
            switch (idx) {
                CaseFunction(Render);
            default: return InputCall::FunctionName(idx);
            }
            #undef CaseFunction
		}

        /**
         * Ctor.
         */
        CallGUIRenderer(void);

        /**
         * ~Dtor.
         */
        virtual ~CallGUIRenderer(void);


        /**
         * Answer the viewport to be used.
         *
         * @return The viewport to be used
         */
        const vislib::math::Rectangle<int>& GetViewport(void) const {
            return this->viewport;
        }

        /**
         * Gets the instance time code
         *
         * @return The instance time code
         */
        inline double InstanceTime(void) const {
            return this->instTime;
        }

        /**
         * Resize the viewport.
         *
         * @param width  The width of the viewport to set
         * @param height The height of the viewport to set
         */
        const void Resize(int width, int height) {
            this->viewport.SetWidth(width);
            this->viewport.SetHeight(height);
        }

        /**
         * Sets the instance time code
         *
         * @param time The time code of the frame to render
         */
        inline void SetInstanceTime(double time) {
            this->instTime = time;
        }

    private:

        /** The instance time code */
        double instTime;
      
        /** The viewport on the buffer */
        vislib::math::Rectangle<int> viewport;
    };

    /** Description class typedef */
    typedef megamol::core::factories::CallAutoDescription<CallGUIRenderer>
        CallGUIRendererDescription;

} /* end namespace view */
} /* end namespace core */
} /* end namespace megamol */

#endif /* MEGAMOLCORE_CALLGUIRENDERER_H_INCLUDED */
