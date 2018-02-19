/*
 * SDFFont.h
 *
 * Copyright (C) 2006 - 2010 by Visualisierungsinstitut Universitaet Stuttgart. 
 * Alle Rechte vorbehalten.
 */

#ifndef MEGAMOL_SDFFONT_H_INCLUDED
#define MEGAMOL_SDFFONT_H_INCLUDED

#if (defined(_MSC_VER) && (_MSC_VER > 1000))
#pragma once
#endif /* (defined(_MSC_VER) && (_MSC_VER > 1000)) */

#if defined(_WIN32) && defined(_MANAGED)
#pragma managed(push, off)
#endif /* defined(_WIN32) && defined(_MANAGED) */


#include "mmcore/view/special/AbstractFont.h"

#include "vislib/graphics/gl/GLSLShader.h"
#include "vislib/graphics/gl/OpenGLTexture2D.h"
#include "vislib/math/Vector.h"

#include <vector>


namespace megamol {
    namespace core {
        namespace view {
            namespace special {


    /**
     * Implementation of simple font using outline glyph information to render
     * the font
     */
    class SDFFont : public AbstractFont {
    public:

        /** Available predefined open source bitmap fonts. */
        enum BitmapFont {
            BMFONT_EVOLVENTA
        };

        /** Possible render types for the font. */
        enum RenderType {
            RENDERTYPE_NONE,              // Do not render anything
            RENDERTYPE_OUTLINE,           // Render the outline 
            RENDERTYPE_FILL,              // Render the filled glyphs */
            RENDERTYPE_FILL_AND_OUTLINE   // Render the filled glyphs with and the outline 
        };

        /**
         * Ctor.
         *
         * @param ofi The outline font info of the font
         */
        SDFFont(const BitmapFont bmf);

        /**
         * Ctor.
         *
         * @param ofi    The outline font info of the font
         * @param render The render type to be used
         */
        SDFFont(const BitmapFont bmf, RenderType render);

        /**
         * Ctor.
         *
         * @param ofi  The outline font info of the font
         * @param size The size of the font in logical units
         */
        SDFFont(const BitmapFont bmf, float size);

        /**
         * Ctor.
         *
         * @param ofi   The outline font info of the font
         * @param flipY The vertical flip flag
         */
        SDFFont(const BitmapFont bmf, bool flipY);

        /**
         * Ctor.
         *
         * @param ofi    The outline font info of the font
         * @param render The render type to be used
         * @param flipY  The vertical flip flag
         */
        SDFFont(const BitmapFont bmf, RenderType render, bool flipY);

        /**
         * Ctor.
         *
         * @param ofi   The outline font info of the font
         * @param size  The size of the font in logical units
         * @param flipY The vertical flip flag
         */
        SDFFont(const BitmapFont bmf, float size, bool flipY);

        /**
         * Ctor.
         *
         * @param ofi    The outline font info of the font
         * @param size   The size of the font in logical units
         * @param render The render type to be used
         */
        SDFFont(const BitmapFont bmf, float size, RenderType render);

        /**
         * Ctor.
         *
         * @param ofi    The outline font info of the font
         * @param size   The size of the font in logical units
         * @param render The render type to be used
         * @param flipY  The vertical flip flag
         */
        SDFFont(const BitmapFont bmf, float size, RenderType render, bool flipY);

        /**
         * Ctor.
         *
         * @param src The source object to clone from
         */
        SDFFont(const SDFFont& src);

        /**
         * Ctor.
         *
         * @param src    The source object to clone from
         * @param render The render type to be used
         */
        SDFFont(const SDFFont& src, RenderType render);

        /**
         * Ctor.
         *
         * @param src  The source object to clone from
         * @param size The size of the font in logical units
         */
        SDFFont(const SDFFont& src, float size);

        /**
         * Ctor.
         *
         * @param src   The source object to clone from
         * @param flipY The vertical flip flag
         */
        SDFFont(const SDFFont& src, bool flipY);

        /**
         * Ctor.
         *
         * @param src    The source object to clone from
         * @param render The render type to be used
         * @param flipY  The vertical flip flag
         */
        SDFFont(const SDFFont& src, RenderType render, bool flipY);

        /**
         * Ctor.
         *
         * @param src   The source object to clone from
         * @param size  The size of the font in logical units
         * @param flipY The vertical flip flag
         */
        SDFFont(const SDFFont& src, float size, bool flipY);

        /**
         * Ctor.
         *
         * @param src    The source object to clone from
         * @param size   The size of the font in logical units
         * @param render The render type to be used
         */
        SDFFont(const SDFFont& src, float size, RenderType render);

        /**
         * Ctor.
         *
         * @param src    The source object to clone from
         * @param size   The size of the font in logical units
         * @param render The render type to be used
         * @param flipY  The vertical flip flag
         */
        SDFFont(const SDFFont& src, float size, RenderType render, bool flipY);

        /** Dtor. */
        virtual ~SDFFont(void);

        /**
         * Calculates the height of a text block in number of lines, when
         * drawn with the rectangle-based versions of 'DrawString' with the
         * specified maximum width and font size.
         *
         * @param maxWidth The maximum width.
         * @param size     The font size to use.
         * @param txt      The text to measure.
         *
         * @return The height of the text block in number of lines.
         */
        virtual unsigned int BlockLines(float maxWidth, float size, const char *txt) const;
        virtual unsigned int BlockLines(float maxWidth, float size, const wchar_t *txt) const;

        /**
         * Draws a text into a specified rectangular area, and performs
         * soft-breaks if necessary.
         *
         * @param x     The left coordinate of the rectangle.
         * @param y     The upper coordinate of the rectangle.
         * @param w     The width of the rectangle.
         * @param h     The height of the rectangle.
         * @param size  The size to use.
         * @param flipY The flag controlling the direction of the y-axis.
         * @param txt   The zero-terminated string to draw.
         * @param align The alignment of the text inside the area.
         */
        virtual void DrawString(float x, float y, float w, float h, float size, bool flipY, const char *txt, Alignment align = ALIGN_LEFT_TOP) const;
        virtual void DrawString(float x, float y, float w, float h, float size, bool flipY, const wchar_t *txt, Alignment align = ALIGN_LEFT_TOP) const;

        /**
         * Draws a text at the specified position.
         *
         * @param x     The x coordinate of the position.
         * @param y     The y coordinate of the position.
         * @param size  The size to use.
         * @param flipY The flag controlling the direction of the y-axis.
         * @param txt   The zero-terminated string to draw.
         * @param align The alignment of the text.
         */
        virtual void DrawString(float x, float y, float size, bool flipY, const char *txt, Alignment align = ALIGN_LEFT_TOP) const;
        virtual void DrawString(float x, float y, float size, bool flipY, const wchar_t *txt, Alignment align = ALIGN_LEFT_TOP) const;

        /**
        * Draws a text at the specified position.
        *
        * @param x     The x coordinate of the position.
        * @param y     The y coordinate of the position.
        * @param z     The z coordinate of the position.
        * @param size  The size to use.
        * @param flipY The flag controlling the direction of the y-axis.
        * @param txt   The zero-terminated string to draw.
        * @param align The alignment of the text.
        */
        virtual void DrawString(float x, float y, float z, float size, bool flipY, const char *txt, Alignment align = ALIGN_LEFT_TOP) const;
        virtual void DrawString(float x, float y, float z, float size, bool flipY, const wchar_t *txt, Alignment align = ALIGN_LEFT_TOP) const;

        /**
        * Answers the width of the line 'txt' in logical units.
        *
        * @param size The font size to use.
        * @param txt  The text to measure.
        *
        * @return The width in the text in logical units.
        */
        virtual float LineWidth(float size, const char *txt) const;
        virtual float LineWidth(float size, const wchar_t *txt) const;

        /**
         * Answers the render type of the font
         *
         * @return The render type of the font
         */
        inline RenderType GetRenderType(void) const {
            return this->renderType;
        }

        /**
         * Sets the render type of the font
         *
         * @param t The render type for the font
         */
        inline void SetRenderType(RenderType t) {
            this->renderType = t;
        }

    protected:

        /**
         * Initialises the object. You must not call this method directly.
         * Instead call 'Initialise'. You must call 'Initialise' before the
         * object can be used.
         *
         * @return 'true' on success, 'false' on failure.
         */
        virtual bool initialise(void);

        /**
         * Deinitialises the object. You must not call this method directly.
         * Instead call 'Deinitialise'. Derived classes must call
         * 'Deinitialise' in EACH dtor.
         */
        virtual void deinitialise(void);

    private:

        /**********************************************************************
        * variables
        **********************************************************************/

        /** The SDF font kerning struct holding the kerninng information of the bitmap font. */
        struct SDFFontKerning {
            int previous;   // The previous character id
            int amount;     // How much the x position should be adjusted when drawing this character immediately following the previous one
        };

        /** The SDF font info struct holding the character information of the bitmap font. */
        struct SDFFontCharacter {
            int id;           // The character id
            float texX0;      // The left position of the character image in the texture
            float texY0;      // The top position of the character image in the texture
            float texX1;      // The right position of the character image in the texture
            float texY1;      // The bottom position of the character image in the texture
            float width;      // The width of the character 
            float height;     // The height of the character 
            float xoffset;    // How much the current position should be offset when copying the image from the texture to the screen
            float yoffset;    // How much the current position should be offset when copying the image from the texture to the screen
            float xadvance;   // How much the current position should be advanced after drawing the character
            // Kerning
            std::vector<SDFFontKerning> kernings;
        };

        /** The sdf font. */
        BitmapFont font;

        /** The texture of the font. */
        vislib::graphics::gl::OpenGLTexture2D texture;

        /** The shader of the font. */
        vislib::graphics::gl::GLSLShader shader;

        /** The render type used. */
        RenderType renderType;

        /** Font characters. */
        std::vector<SDFFontCharacter> characters;
        /** Font indices for characters. */
        std::vector<SDFFontCharacter *> indices;

        /** Vertex array object. */
        GLuint vaoHandle;

        /** Vertex buffer object. */
        enum VBOAttrib {
            POSITION  = 0,
            TEXTURE   = 1
        };
        struct SDFVBO {
            GLuint                 handle;  // buffer handle
            vislib::StringA        name;    // varaible name of attribute in shader
            GLuint                 index;   // index of attribute location
            unsigned int           dim;     // dimension of data
        };
        std::vector<SDFVBO> vbos;

        /**********************************************************************
        * functions
        **********************************************************************/

        /** Loading font. */
        bool loadFont(BitmapFont bmf);

        /** Load buffers. */
        bool loadFontBuffers();

        /** Load font info from file. */
        bool loadFontInfo(vislib::StringA filename);

        /** Load texture from file. */
        bool loadFontTexture(vislib::StringA filename);

        /** Load shaders from files. */
        bool loadFontShader(vislib::StringA vert, vislib::StringA frag);

        /** Load file into outData buffer and return size. */
        SIZE_T loadFile(vislib::StringA filename, void **outData);

        /** Number of lines with maxWidth and font size in the text. */
        unsigned int lineCount(float maxWidth, float size, const char *txt) const;
        unsigned int lineCount(float maxWidth, float size, const wchar_t *txt) const;

        /**
         * Draw font glyphs.
         *
         * @param txt   The pointer to the text string
         * @param x     The reference x coordinate
         * @param y     The reference y coordinate
         * @param z     The reference z coordinate
         * @param size  The size
         * @param flipY The flag controlling the direction of the y-axis
         * @param align The alignment
         */
        void draw(const char *txt, float x, float y, float z, float size, bool flipY, Alignment align) const;
        void draw(const wchar_t *txt, float x, float y, float z, float size, bool flipY, Alignment align) const;

    };

            } /* end namespace special */
        } /* end namespace view */
    } /* end namespace core */
} /* end namespace megamol */

#if defined(_WIN32) && defined(_MANAGED)
#pragma managed(pop)
#endif /* defined(_WIN32) && defined(_MANAGED) */

#endif /* MEGAMOL_SDFFONT_H_INCLUDED */

