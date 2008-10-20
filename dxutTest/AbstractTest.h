/*
 * AbstractTest.h
 *
 * Copyright (C) 2006 - 2008 by Universitaet Stuttgart (VIS). 
 * Alle Rechte vorbehalten.
 */

#ifndef VISLIBTEST_ABSTRACTEST_H_INCLUDED
#define VISLIBTEST_ABSTRACTEST_H_INCLUDED
#if (defined(_MSC_VER) && (_MSC_VER > 1000))
#pragma once
#endif /* (defined(_MSC_VER) && (_MSC_VER > 1000)) */
#if defined(_WIN32) && defined(_MANAGED)
#pragma managed(push, off)
#endif /* defined(_WIN32) && defined(_MANAGED) */


#include "vislib/String.h"


class AbstractTest {

public:

    virtual ~AbstractTest(void);

    const vislib::TString& GetName(void) const {
        return this->name;
    }

    virtual HRESULT OnD3D9CreateDevice(PDIRECT3DDEVICE9 pd3dDevice, 
        const D3DSURFACE_DESC *pBackBufferSurfaceDesc);

    virtual void OnD3D9DestroyDevice(void);

    virtual void OnD3D9FrameRender(PDIRECT3DDEVICE9 pd3dDevice, double fTime, 
        float fElapsedTime) = 0;

    virtual void OnD3D9LostDevice(void);

    virtual HRESULT OnD3D9ResetDevice(PDIRECT3DDEVICE9 pd3dDevice, 
        const D3DSURFACE_DESC *pBackBufferSurfaceDesc);

    virtual void OnFrameMove(double fTime, float fElapsedTime);

    virtual void OnKeyboard(UINT nChar, bool bKeyDown, bool bAltDown);

    virtual void OnMouse(bool bLeftButtonDown, bool bRightButtonDown, 
        bool bMiddleButtonDown, bool bSideButton1Down, bool bSideButton2Down,
        INT nMouseWheelDelta, INT xPos, INT yPos);

protected:

    AbstractTest(const vislib::StringW& name);

    float aspectRatio;

    vislib::StringW name;
};

#if defined(_WIN32) && defined(_MANAGED)
#pragma managed(pop)
#endif /* defined(_WIN32) && defined(_MANAGED) */
#endif /* VISLIB_ABSTRACTDIMENSIONIMPL_H_INCLUDED */
