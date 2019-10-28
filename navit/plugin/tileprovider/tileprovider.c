/** jandegr 2019
 *
 *
 * This started life as a rework of the low quality preview bitmap of Zanavi
 * using some openGL pieces of a never published openGL version of Navit
 * https://github.com/jandegr/zanavi/blob/openGL/navit/tilefactory.c
 * Due to hardcoded changes to layout in Zanavi graphics the openGL low quality
 * preview bitmap had a higher quality than the regular Zanavi mapview.
 *
 * The purpose of this version is to provide slippy map tiles for any
 * consumer (google map api used for testing) or to draw an entire screen
 * for a more classic Navit setup and eventually have one single version containing all
 * of my earlier Navit openGL related constructs.
 *
 * no oversampling (smooth lines) as long as it uses a pbuffer
 * several things to fix
 *
 * already draws with z but some tweak will have to be done in maptool
 * before bridges and tunnels can be drawn in the correct order using
 * the layer attribute from OSM.
 *
 *
 * WIP WIP WIP
 */




#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <android.h>
#include <android/bitmap.h> // from NDK
#include <stdlib.h>
#include <glib.h>
#include <stdio.h>
#include <math.h>
#include "navit.h"
#include "item.h"
#include "xmlconfig.h"
#include "debug.h"
#include "point.h"
#include "map.h"
#include "transform.h"  // = incl coord.h !!!
#include "plugin.h"
#include "mapset.h"
#include "layout.h"
#include "callback.h"
#include "file.h"
#include <navit/config_.h>
#include <navit/support/glib/gslist.h> //nazien, beter glib. fixen



struct tileprovider {
    struct navit *nav;
};

struct openGL_displaylist {

};

// terzelfdertijd deze entries in een glist steken ordered op Z??
struct openGL_hash_entry {
    int z;
    int defer;
    GList *elements;
    GList *itemlist; //the actual items (GSList not in support/glib ???)
};


GHashTable* htab = NULL;
int htab_order = 99999;
int mapcenter_x = 0;
int mapcenter_y = 0;

jclass TileClass = NULL;
jmethodID Preview_set_bitmap = NULL;


EGLConfig eglConf;
EGLSurface eglSurface;
EGLContext eglCtx;
EGLDisplay eglDisp;


GLuint gvPositionHandle;
GLint gvMvprojHandle;
GLint gvColorHandle;
//GLint gvMapcenterHandle;
GLint gvScaleXHandle;
GLint gvZvalHandle;
GLint gvUseDashesHandle;
GLint gvSourcePointHandle;
GLfloat glMaxLineWidth;

struct tileprovider this;

// EGL config attributes
const EGLint confAttr[] = {
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,    // very important!
		EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,          // we will create a pixelbuffer surface
		EGL_RED_SIZE,   8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE,  8,
		EGL_ALPHA_SIZE, 8,     // if you need the alpha channel
		EGL_DEPTH_SIZE, 16,    // if you need the depth buffer
		EGL_NONE
};

// EGL context attributes
const EGLint ctxAttr[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
};


#if def0 // dashes but in world coords, resizes dashes with zoom
const char gVertexShader [] =
        "attribute vec2 vPosition;\n"
                "attribute vec2        texture_position; \n"
                "uniform mat4        mvproj;\n"
                "uniform mat2        invMvproj;\n"
                "uniform bool use_dashes;\n"
                "varying float v_use_dashes = 0.0;\n"
                "varying vec2 v_texture_position;\n"
                "varying vec2 position;\n"
                "uniform vec4  color;\n"
                "uniform int  zval;\n"
                "void main() {\n"
                "  v_texture_position=texture_position;\n"
                "  gl_Position = mvproj * vec4((vPosition.x), (vPosition.y) , zval, 1);\n"
                "  if (use_dashes){\n"
                "      v_use_dashes = 1.0;\n"
                "      position = invMvproj * vec2(gl_Position.x, gl_Position.y) ;\n"
                "  }\n"
                "}\n";

// met invMvproj moet er terug naar schermcoordinaten gerekend worde
// van de actuele positie
// bewerking op of met gl_position kan beter in de fragmentshader gebeuren,
// dat is daar ook beschikbaar !!!!
// of met gl_FragCoord werken ???
// http://www.shaderific.com/glsl-variables
//"precision mediump float;\n"

const char gFragmentShader [] =
        "precision highp float;\n"
                "uniform vec4  color;\n"
                "uniform sampler2D texture;\n"
                "uniform bool use_texture;\n"
                "varying float v_use_dashes;\n"
                "uniform vec2 sourcePoint;\n"
                "varying vec2 position;\n"
                "varying vec2 v_texture_position;\n"
                "void main() {\n"
                "   if (use_texture) {\n"
                "     gl_FragColor = texture2D(texture, v_texture_position);\n"
                "   }else if (v_use_dashes > 0.0){\n"
                "      if (cos(0.85 * abs(distance(sourcePoint.xy, position.xy))) - 0.3 > 0.0)\n"
                "         {\n"
                "             discard;\n"
                "         } else \n"
                "         {\n"
                "             gl_FragColor = color;\n"
                "         }\n"
                "   }else{\n"
                "     gl_FragColor = color;\n"
                "   }\n"
                "}\n";


const char gFragmentShader_NOdash [] =
        "precision mediump float;\n"
                "uniform vec4  color;\n"
                "uniform sampler2D texture;\n"
                "uniform bool use_texture;\n"
                "uniform bool use_dashes;\n"
                "varying vec2 v_texture_position;\n"
                "void main() {\n"
                "   if (use_texture) {\n"
                "     gl_FragColor = texture2D(texture, v_texture_position);\n"
                "   }else if (use_dashes){\n"
                "     discard;\n"
                "   }else{\n"
                "     gl_FragColor = color;\n"
                "   }\n"
                "}\n";
#endif

const char gVertexShader [] =
        "attribute vec2 vPosition;\n" // zo een vec4 maken waarbij 3 en 4 first point zijn en dan niet met line strip tekenen !!
                "attribute vec2        texture_position; \n"
                "uniform mat4        mvproj;\n"
                "uniform bool use_dashes;\n"
                "uniform vec2 sourcePoint;\n"
                "uniform float scaleX;"
                "uniform vec4  color;\n"
                "uniform int  zval;\n"
                "varying vec2 v_texture_position;\n"
                "varying float vDistanceFromSource;\n"
                "void main() {\n"
                "  v_texture_position=texture_position;\n"
                "  gl_Position = mvproj * vec4((vPosition.x), (vPosition.y) , zval, 1);\n"
                "  if (use_dashes){\n"
                "      vec4 position = mvproj * vec4(sourcePoint.x, sourcePoint.y, zval, 1) ;\n"
                "      vDistanceFromSource = distance(position.xy, gl_Position.xy) * scaleX;\n"
                "  }\n"
                "}\n";

const char gFragmentShader [] =
        "precision highp float;\n"
                "uniform vec4  color;\n"
                "uniform sampler2D texture;\n"
                "uniform bool use_texture;\n"
                "uniform bool use_dashes;\n"
                "varying float vDistanceFromSource;\n"
                "varying vec2 v_texture_position;\n"
                "void main() {\n"
                "   if (use_texture) {\n"
                "     gl_FragColor = texture2D(texture, v_texture_position);\n"
                "   }else if (use_dashes){\n"
                "      if (cos(0.65 * abs(vDistanceFromSource)) - 0.3 > 0.0)\n"
                "         {\n"
                "             discard;\n"
                "         } else \n"
                "         {\n"
                "             gl_FragColor = color;\n"
                "         }\n"
                "   }else{\n"
                "     gl_FragColor = color;\n"
                "   }\n"
                "}\n";


static void
printGLString(const char *name, GLenum s) {
	const char *v = (const char *) glGetString(s);
	dbg(0,"GL %s = %s\n", name, v);
}

static inline void
checkGlError(const char* op) {

//	for (GLint error = glGetError(); error; error = glGetError()) {
//		dbg(0,"after %s() glError (0x%x)\n", op, error);
//	}

}

GLuint
loadShader(GLenum shaderType, const char* pSource) {
	GLuint shader = glCreateShader(shaderType);
	if (shader) {
		glShaderSource(shader, 1, &pSource, NULL);
		glCompileShader(shader);
		GLint compiled = 0;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
		if (!compiled) {
			GLint infoLen = 0;
			glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
			if (infoLen) {
				char* buf = (char*) malloc(infoLen);
				if (buf) {
					glGetShaderInfoLog(shader, infoLen, NULL, buf);
					dbg(lvl_error,"Could not compile shader %d:\n%s\n",
						shaderType, buf);
					free(buf);
				}
				glDeleteShader(shader);
				shader = 0;
			}
		}
	}
	return shader;
}


GLuint
createProgram(const char* pVertexSource, const char* pFragmentSource)
{
	GLuint vertexShader = loadShader(GL_VERTEX_SHADER, pVertexSource);
	if (!vertexShader) {
		return 0;
	}

	GLuint pixelShader = loadShader(GL_FRAGMENT_SHADER, pFragmentSource);
	if (!pixelShader) {
		return 0;
	}

	GLuint program = glCreateProgram();
	if (program) {
		glAttachShader(program, vertexShader);
		checkGlError("glAttachShader");
		glAttachShader(program, pixelShader);
		checkGlError("glAttachShader");
		glLinkProgram(program);
		GLint linkStatus = GL_FALSE;
		glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
		if (linkStatus != GL_TRUE) {
			GLint bufLength = 0;
			glGetProgramiv(program, GL_INFO_LOG_LENGTH, &bufLength);
			if (bufLength) {
				char* buf = (char*) malloc(bufLength);
				if (buf) {
					glGetProgramInfoLog(program, bufLength, NULL, buf);
					dbg(lvl_error,"Could not link program:\n%s\n", buf);
					free(buf);
				}
			}
			glDeleteProgram(program);
			program = 0;
		}
	}
	return program;
}


void shutdownEGL() {
	eglMakeCurrent(eglDisp, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	eglDestroyContext(eglDisp, eglCtx);
	eglDestroySurface(eglDisp, eglSurface);
	eglTerminate(eglDisp);

	eglDisp = EGL_NO_DISPLAY;
	eglSurface = EGL_NO_SURFACE;
	eglCtx = EGL_NO_CONTEXT;
}

#if 0

// a version that draws with screenrotation
// NOT to be used for tiles !!!!!
// because tiles are never rotated
// suitable for drawing a screen with rotation

/**
 * Sets the transformation matrices for the shader(s).
 *
 * @param width of the bitmap
 * @param tr the current transformation of the screen
 */
void set_matrices(int width, struct transformation *tr)
{
    // allows rotation, not for tiles !!!
    GLfloat yawcos = (GLfloat)cos(-M_PI * transform_get_yaw(tr) / 180);
    GLfloat yawsin = (GLfloat)sin(-M_PI * transform_get_yaw(tr) / 180);

    GLfloat matrix[16];

    matrix[0]= (GLfloat)(yawcos *(2.0/width) / transform_get_scale(tr));
    matrix[1]= (GLfloat)(yawsin *(2.0/width) / transform_get_scale(tr));
    matrix[2]=0.0;
    matrix[3]=0.0;
    matrix[4]= matrix[1];
    matrix[5] = -matrix[0];
    matrix[6]=0.0;
    matrix[7]=0.0;
    matrix[8]=0.0;
    matrix[9]=0.0;
    matrix[10]=-0.0001f; // z multiplier
    matrix[11]=0.0;
    matrix[12]=0.0;
    matrix[13]=0.0;
    matrix[14]=0.0;
    matrix[15]=1.0;

    glUniformMatrix4fv(gvMvprojHandle, 1, GL_FALSE, matrix);
}
#endif

/**
 * Sets the transformation matrix for the shader(s).
 *
 * @param width of the bitmap
 * @param scale
 */
void set_matrix(int width, GLfloat scale)
{
    // tiles are never drawn with rotation !!!
    GLfloat yawcos = 1;
    GLfloat yawsin = 0;

    GLfloat matrix[16];

    matrix[0]= (GLfloat)(yawcos *(2.0/width) / scale);
    matrix[1]= (GLfloat)(yawsin *(2.0/width) / scale);
    matrix[2]=0.0;
    matrix[3]=0.0;
    matrix[4]= matrix[1];
    matrix[5] = -matrix[0];
    matrix[6]=0.0;
    matrix[7]=0.0;
    matrix[8]=0.0;
    matrix[9]=0.0;
    matrix[10]=-0.0001f; // z multiplier
    matrix[11]=0.0;
    matrix[12]=0.0;
    matrix[13]=0.0;
    matrix[14]=0.0;
    matrix[15]=1.0;

    glUniformMatrix4fv(gvMvprojHandle, 1, GL_FALSE, matrix);
}


/**
 * Sets the openGL color uniform
 * for the shaderprograms.
 *
 * @param color
 */
static inline void
set_color(struct color *color)
{
//	dbg(lvl_error,"set color r = %i, g = %i, b = %i, a = %i\n",color->r,color->g,color->b,color->a);
    GLfloat col[4];
    col[0]=(GLfloat)color->r/65535;
    col[1]=(GLfloat)color->g/65535;
    col[2]=(GLfloat)color->b/65535;
    col[3]=(GLfloat)1.0;
//	dbg(lvl_error,"set color r = %f, g = %f, b = %f, a = %f\n",col[0],col[1],col[2],col[3]);
    glUniform4fv(gvColorHandle, 1,col);
}

/**
 * draws any primitive with z
 *
 * use this if the points were not processed and
 * still have int coords
 *
 * @param p first of the points
 * @param count the number of points
 * @param z
 * @param mode the openGL draw mode
 */
static inline void
draw_array(struct point *p, int count, int z, GLenum mode)
{
    int i;
    GLshort pcoord[count*2]; //CPU handles mapcenter so this can be a GLshort

    for (i = 0 ; i < count ; i++) {
        pcoord[i*2]=(GLshort)(p[i].x - mapcenter_x);
        pcoord[i*2+1]=(GLshort)(p[i].y - mapcenter_y);
    }

    glVertexAttribPointer(gvPositionHandle, 2, GL_SHORT, GL_FALSE, 0, pcoord);
    //glEnableVertexAttribArray(gvPositionHandle);
    glUniform1i(gvZvalHandle,z);

    glDrawArrays(mode, 0, count);
}

/**
 * Draws the polygon elements of a hash entry with an optional outline.
 *
 * @param entry the hash entry derived from an itemgra
 * @param p first of the points
 * @param count the number of points
 * @return nothing
 */
void
drawStencil(struct openGL_hash_entry *entry, struct point *p, int count, int z_fix) {

    int i;
    GList *elements = entry->elements;
    struct element *element_data = elements->data;
    GLshort boundingbox[8];
    // topleft, topright, bottomright, bottomleft
    boundingbox[0] = boundingbox[2] = boundingbox[4] = boundingbox[6] = (GLshort)(p[0].x - mapcenter_x);
    boundingbox[1] = boundingbox[3] = boundingbox[5] = boundingbox[7] = (GLshort)(p[0].y - mapcenter_y);

    if (element_data->type == 2) {// is it actually a polygon ?
        struct color *color = &(element_data->color);
        set_color(color);
        GLshort x[count * 2];
        if (z_fix != 0) {
            //dbg(lvl_error, "draw z_fix = %i\n", z_fix);
            glUniform1i(gvZvalHandle, z_fix);
        } else {
            glUniform1i(gvZvalHandle, entry->z);
        }

        for (int i = 0 ; i < count ; i++) {
            x[i*2]=(GLshort)(p[i].x - mapcenter_x);
            x[i*2+1]=(GLshort)(p[i].y - mapcenter_y);
            if (x[i*2] < boundingbox[0]) // expand leftwards
            {
                boundingbox[0] = boundingbox[6] = x[i*2] ;
            } else {
                if (x[i*2] > boundingbox[2]) // expand to the right
                {
                    boundingbox[2] = boundingbox[4] = x[i*2];
                }
            }
            if (x[i*2+1] < boundingbox[1]) // expand upwards
            {
                boundingbox[1] = boundingbox[3] = x[i*2+1];
            } else {
                if (x[i*2+1] > boundingbox[5]) // expand downwards
                {
                    boundingbox[5] = boundingbox[7] = x[i*2+1];
                }
            }
        }

        glClear(GL_STENCIL_BUFFER_BIT);
        glEnable(GL_STENCIL_TEST);
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        glStencilFunc(GL_ALWAYS, 0, 1);
        glStencilOp(GL_KEEP, GL_KEEP, GL_INVERT);
        glStencilMask(1);
        glDisable(GL_DEPTH_TEST); // for some reason stencil does not work with depthtest enabled

        glVertexAttribPointer(gvPositionHandle, 2, GL_SHORT, GL_FALSE, 0, x);
        //glEnableVertexAttribArray(gvPositionHandle);

        glDrawArrays(GL_TRIANGLE_FAN, 0, count);
        checkGlError("glDrawArrays");

        if (elements->next) {
            struct element *element_next_data = elements->next->data;
            if (element_next_data->type == 1) // does the polygon have an outline ?
            {
                glLineWidth((GLfloat) 1.0);
                glStencilFunc(GL_ALWAYS, 2, 3);
                glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
                glStencilMask(3);
                // points are still up, can draw the outline right away
                glDrawArrays(GL_LINE_STRIP, 0, count);
                checkGlError("glDrawArrays");
            }
        }

        glEnable(GL_DEPTH_TEST); // reenable depthtest after stencil

        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glStencilFunc(GL_EQUAL, 1, 1);
        glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
        glVertexAttribPointer(gvPositionHandle, 2, GL_SHORT, GL_FALSE, 0, boundingbox);
        //glEnableVertexAttribArray(gvPositionHandle);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        if (elements->next) {
            struct element *element_next_data = elements->next->data;
            if (element_next_data->type == 1) // paint outline for polygon
            {
                struct color *color = &(element_next_data->color);
                set_color(color);
                glStencilFunc(GL_EQUAL, 2, 2);
                glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
            }
        }
        glDisable(GL_STENCIL_TEST);
    }
}

static inline void
setdashes(int enable)
{
    if (enable == TRUE) {
        glUniform1i(gvUseDashesHandle,TRUE);
    } else {
        glUniform1i(gvUseDashesHandle,FALSE);
    }
}


/**
 * Draws the line elements of a hash entry
 * version with GLshort and mapcenter handled by CPU
 *
 * @param entry the hash entry derived from an itemgra
 * @param p first of the points
 * @param count the number of points
 */
static void
draw_lines(struct openGL_hash_entry *entry, struct point *p, int count, int z_fix )
{
    //dbg(lvl_error,"enter\n");
    GList *elements = entry->elements;
    int z = entry->z;

    while(elements) {
        struct element *element_data = elements->data;
        //dbg(0, "draw element_data z  = %f\n", z);
        if (element_data->type == 1 && count > 1) {
            struct element_polyline *line = &(element_data->u);
            struct color *color = &(element_data->color);
            set_color(color);
            if (line->width <  1) {
                line->width = 1;
            }
            if (line->width <  5) { // let GPU draw them as thick lines
                                    // this is also the only way to draw dashed lines
                glLineWidth((GLfloat)line->width);
                if (z_fix != 0) {
                    //dbg(lvl_error, "draw thin lines z_fix = %i\n", z_fix);
                    glUniform1i(gvZvalHandle, z_fix);
                } else {
                    glUniform1i(gvZvalHandle, z);
                }
                if (line->dash_num) { // draw dashed line
                    setdashes(TRUE);

                    for(int i=0; i<(count-1); i++) {
                        GLfloat segcoord[4];
                        segcoord[0] = (GLfloat)(p[0 + i].x - mapcenter_x);
                        segcoord[1] = (GLfloat)(p[0 + i].y - mapcenter_y);
                        segcoord[2] = (GLfloat)(p[1 + i].x - mapcenter_x);
                        segcoord[3] = (GLfloat)(p[1 + i].y - mapcenter_y);

                        GLfloat sourcePoint[2];
                        sourcePoint[0] = segcoord[0];
                        sourcePoint[1] = segcoord[1];

                        // glUniform2iv gives errors
                        glUniform2fv(gvSourcePointHandle,1, segcoord);
                        checkGlError("glUniform2fv sourcePoint");

                        glVertexAttribPointer(gvPositionHandle, 2, GL_FLOAT, GL_FALSE, 0, segcoord);
                        checkGlError("in draw_array glVertexAttribPointer");

                        //glEnableVertexAttribArray(gvPositionHandle);
                        //checkGlError("in draw_array glEnableVertexAttribArray");

                        glDrawArrays(GL_LINES, 0, 2);
                        checkGlError("glDrawArrays");
                    }
                    setdashes(FALSE);
                } else { // moderate thickness line without dashes
                    if (z_fix != 0) {
                        //dbg(lvl_error, "draw thin lines z_fix = %i\n", z_fix);
                        draw_array(p, count, z_fix, GL_LINE_STRIP);
                    } else {
                        draw_array(p, count, z, GL_LINE_STRIP);
                    }
                }
            } else {   // construct boxes and an end and startcap
                count--; // 1 roadsegment less than number of points
                // 4 points per segment + one extra for start and end
                GLshort proad[(count * 8) + 4];
                // was before endcap hack
                int thickness = line->width;
                int capVertexCount = 2 * (int)(thickness / 4);

                //GLfloat proad[((count) * 8) + (2 * capVertexCount)]; // bottom line

                if (z_fix != 0) {
                    //dbg(lvl_error, "draw lines z_fix = %i\n", z_fix);
                    glUniform1i(gvZvalHandle, z_fix);
                } else {
                    glUniform1i(gvZvalHandle, z);
                }

                // keep track of the number of coords produced
                int ccounter = 0;
                for (int j = 0; j < count; j++) {
                    double dx = (p[1 + j].x - p[0 + j].x); //delta x
                    double dy = (p[1 + j].y - p[0 + j].y); //delta y
                    double linelength = sqrt(dx * dx + dy * dy);

                    // add start cap -- TODO use more points for thicker lines
                    if (ccounter == 0) {
                        int firstx = p[0 + j].x - (int)(dx * thickness/ ( 2 * linelength )) - mapcenter_x;
                        int firsty = p[0 + j].y - (int)(dy  * thickness/ (2 * linelength)) - mapcenter_y;
                        proad[ccounter] = (GLshort) firstx;
                        ccounter++;
                        proad[ccounter] = (GLshort) firsty;
                        ccounter++;
                    }

                    int lastx = 0;
                    int lasty = 0;
                    if (j == (count-1)) // remember these for the end cap
                    {
                        lastx = p[1 + j].x + (int) (dx * thickness / (2 * linelength))- mapcenter_x;
                        lasty = p[1 + j].y + (int) (dy * thickness / (2 * linelength))- mapcenter_y;
                    }

                    dx = dx / linelength;
                    dy = dy / linelength;
                    //perpendicular vector with length thickness * 0.5
                    const int px = (int)((thickness * (-dy)) / 2);
                    const int py = (int)((thickness * dx) / 2 );
                    //	dbg(lvl_error, "dx = %lf, dy = %lf, px = %lf, py = %lf\n", dx, dy, px, py);

                    proad[ccounter] = (GLshort)(p[0 + j].x - px - mapcenter_x);
                    ccounter ++;
                    proad[ccounter] = (GLshort)(p[0 + j].y - py - mapcenter_y);
                    ccounter ++;
                    proad[ccounter] = (GLshort)(p[0 + j].x + px - mapcenter_x);
                    ccounter ++;
                    proad[ccounter] = (GLshort)(p[0 + j].y + py - mapcenter_y);
                    ccounter ++;
                    proad[ccounter] = (GLshort)(p[1 + j].x - px - mapcenter_x);
                    ccounter ++;
                    proad[ccounter] = (GLshort)(p[1 + j].y - py - mapcenter_y);
                    ccounter ++;
                    proad[ccounter] = (GLshort)(p[1 + j].x + px - mapcenter_x);
                    ccounter ++;
                    proad[ccounter] = (GLshort)(p[1 + j].y + py - mapcenter_y);
                    ccounter ++;

                    // add end cap
                    if (j == (count-1)) {
                        proad[ccounter] = (GLshort) lastx;
                        ccounter++;
                        proad[ccounter] = (GLshort) lasty;
                        ccounter++;
                    }

                    //if (j == count -1) {// add endcap
                    //    double startangle = atan2((double) dx, (double) dy);
                    //    int k = 0;
                    //    for (int cV = (capVertexCount /2) -1 ; cV > -1; cV--) {
                    //        double anglestep = ((cV + 1) * M_PI / (capVertexCount + 1));
                    //        double angle = startangle + anglestep ;
                    //        double angleEnd = startangle + M_PI - anglestep;
                            //         dbg(lvl_error, "startangle = %lf, angle = %lf, angleEnd = %lf",
                            //			startangle * 360 / (2 * M_PI), angle * 360 / (2 * M_PI), angleEnd * 360 / (2 * M_PI));
                     //       proad[10 + k ] = lastx + (thickness * (float)sin(angle)/2);
                    //        proad[11 + k ] = lasty + (thickness  * (float)cos(angle)/2);

                    //        proad[12 + k ] = lastx - (thickness * (float)sin(angleEnd)/2);
                    //        proad[13 + k ] = lasty - (thickness  * (float)cos(angleEnd)/2);

                            //if (thickness_1 > 0) {
                            //    proad_1[10 + i + k ] = p[1 + j].x + (thickness_1 * (float)sin(angle)/2);
                            //    proad_1[11 + i + k ] = p[1 + j].y + (thickness_1  * (float)cos(angle)/2);

                            //    proad_1[12 + i + k ] = p[1 + j].x - (thickness_1 * (float)sin(angleEnd)/2);
                            //    proad_1[13 + i + k ] = p[1 + j].y - (thickness_1  * (float)cos(angleEnd)/2);
                            //}
                    //        k = k+4;
                    //        ccounter = ccounter + 4;
                    //    }
                    //}





                }
                glVertexAttribPointer(gvPositionHandle, 2, GL_SHORT, GL_FALSE, 0, proad);
                checkGlError("in draw_array glVertexAttribPointer");

                //glEnableVertexAttribArray(gvPositionHandle);
                //checkGlError("in draw_array glEnableVertexAttribArray");

                glDrawArrays(GL_TRIANGLE_STRIP, 0, (ccounter/2)); // 4 points for a segment + startpoint
                checkGlError("glDrawArrays");
#if def0
                // debug
                if (!elements->next) {
                    glUniform1i(gvZvalHandle, z + 1);
                    struct color *color2 = g_alloca(sizeof(struct color));
                    color2->r = 65000;
                    color2->b = 0;
                    color2->g = 0;
                    set_color(color2);
                    glLineWidth(1.0f);
                    glDrawArrays(GL_LINE_STRIP, 0, ccounter/2); // 4 points for a segment + startpoint
                    checkGlError("glDrawArrays");
                }
                // end debug
#endif
                count ++; // restore count to the number of points
            }
        }
        elements = elements->next;
        z ++;
        if (z_fix != 0) {
            z_fix++;
        }
    }
   // dbg(lvl_error,"leave\n");
}



/**
 * Draws the elements of a hash entry for the given points.
 *
 * @param entry the hash entry derived from an itemgra
 * @param p first of the points
 * @param count the number of points
 * @param triangles TRUE or FALSE (from an area triangulated in maptool)
 * @return nothing
 *
 */
void
draw_elements( struct openGL_hash_entry *entry ,struct point *points, int count, int triangles, int z_fix )
{
    //dbg(lvl_error,"enter\n");
    GList *elements = entry->elements;
    struct element *element = elements->data;
    // dbg(0,"elemnt ENTRY type = %i\n", element->type);

    // missing element arrows, text, circle and so on

    if (element->type == 1){ //polyline, can have another one as second element
       // dbg(lvl_error,"type 1\n");
        draw_lines(entry, points, count, z_fix);
    } else {
        if (element->type == 2) //polygon, can have an outline as second element
        {
            //dbg(lvl_error,"type 2\n");
            if (triangles == TRUE) { // no need to stecil pretriangulated polygons
                                     // and those have no outline either
                struct color *color = &(element->color);
                set_color(color);
                if (z_fix != 0) {
                    //dbg(lvl_error,"draw z_fix = %i\n", z_fix);
                    draw_array(points, count, z_fix, GL_TRIANGLE_STRIP);
                } else {
                    draw_array(points, count, entry->z, GL_TRIANGLE_STRIP);
                }

#if def0
                // some test for triangulated polygons
                // draws the triangles as red lines
                // above the polygon
                struct color *color2 = g_alloca(sizeof(struct color));
                color2->r = 65000;
                color2->b = 0;
                color2->g = 0;
                set_color(color2);
                glLineWidth(3.0f);
                draw_array(p, count, entry->z +1 , GL_LINES);
#endif
            } else {// not yet triangulated so use stencil and draw optional outline
                drawStencil(entry, points, count, z_fix);
            }
        } else {
              //  dbg(lvl_error,"NOT type 1 or 2\n");
        }
    }

#if def0  // dit loopje werkt !!!
    while (elements){ // deze lijkt eindelijk te werken
        dbg(lvl_error,"elemnt type = %i\n", element->type);
        elements = elements->next;
        if (elements) {
            dbg(lvl_error,"WEL next\n");
            element = elements->data;
        } else {
            dbg(lvl_error,"NO next\n");
        }

    }
#endif
    //dbg(lvl_error,"leave\n");
}

void
free_element( gpointer key, gpointer value, gpointer userData )
{
    g_free(value);
}


void
freehtab()
{
    if (htab){
        g_hash_table_foreach(htab, free_element, NULL);
        g_hash_table_destroy(htab);
    }
}

/**
 * Fills the hash (if needed) with the layout
 * elements first to appear based on the order.
 *
 * @param layout
 * @param order
 */
void
fillhash(const struct layout *layout, int order)
{
    dbg(lvl_error,"enter, order = %i\n", order);
    if (!htab || htab_order != order) { // check for changes in layout as well !!!!
        // what if a layer just gets deactivated by the user ?
        freehtab();
        htab = g_hash_table_new(g_str_hash, g_str_equal);
        htab_order = order;

        GList *current_layer = layout->layers;
        int z = 10;
        while (current_layer) {
            int todefer = FALSE;
            struct layer *layer = current_layer->data;
            //dbg(0, "layer %s\n", layer->name);
            if (layer->active && (!strstr(layer->name,"POI"))&& (!strstr(layer->name,"labels"))) {
                //if (strstr(layer->name,"streets") || strstr(layer->name,"polygons")) {
                //    dbg(lvl_error, "layer %s +++ DEFERRED\n", layer->name);
                //}

                if (strstr(layer->name,"streets") && z < 1000) {
                    z = 1050;
                }
                if (strstr(layer->name,"polylines") && z < 2049) {
                    z = 2050;
                }
                dbg(lvl_error, "layer %s +++ ACTIVE\n", layer->name);
                GList *current_itemgra = layer->itemgras;
                while (current_itemgra) {
                    struct itemgra *itemgra = current_itemgra->data;
                    int ordermin = itemgra->order.min;  // zou rechtstreeks kunnen gebruikt worden, was ander probleem
                    int ordermax = itemgra->order.max;
                    //dbg(lvl_error,"itemgra min = %i, max = %i",ordermin , ordermax);

                    GList *elements = itemgra->elements;
                    GList *type = itemgra->type;
                    struct element *element = elements->data;
                    dbg(lvl_error,"Z = %i\n",z);
                    if (ordermin <= order && ordermax >= order) {
                        while (type) {
                            //dbg(lvl_error,"item %i\n",element->type); // dit is lijn , polygon en zo
                            struct openGL_hash_entry *entry = g_new0(struct openGL_hash_entry, 1);
                            entry->z = z;
                            entry->elements = elements;
                            entry->defer = todefer;
                            if (!g_hash_table_lookup(htab, item_to_name(type->data))) {
                                //if (strstr(item_to_name(type->data),"building")) {
                                //    entry->defer = TRUE;
                                //}
                                g_hash_table_insert(htab, item_to_name(
                                        type->data), // anders blaast text er de polygons uit !!
                                                    entry); // nooit NULL toevoegen !!!!
                            }
                            //dbg(0, "inserted item %s\n",item_to_name(type->data));  //dit is item poly_park enzovoort
                            type = type->next;
                        }
                        z = z + 10;
                    }
                    current_itemgra = current_itemgra->next;
                }
            }
            //   else we have an inactive layer, do nothing
            current_layer = current_layer->next;
        }
    }
}


void
draw(GHashTable* htab)
{
    dbg(lvl_error,"draw htab\n");

}


// draw_tile is the old name, it is now used to draw a grid of tiles as well into
// one big image that is later cut into a bunch of seperate tiles
void
draw_tile(JNIEnv *env, jobject jTileProvider, struct coord_geo lu,  struct coord_geo rl, int width,
        int height, GLfloat scale, int zoom)
{
    dbg(lvl_error,"enter\n");
    struct layout  * layout;
    struct attr layout_attr;
    int order;

    // needs review !!!
    int order_corrected = zoom - 6;
    int order_corrected_2 = zoom -8; // + shift_order;

    struct coord left_upper;
    struct coord right_lower;

    transform_from_geo(projection_mg, &lu, &left_upper);
    transform_from_geo(projection_mg, &rl, &right_lower);

    struct item *item;
    struct map_rect *mr = NULL;
    struct mapset *mapset;
    struct mapset_handle *msh;
    struct map *map = NULL;
    struct attr map_name_attr;
    struct attr attr;

    struct map_selection sel;

    const int max = 1000;   // was 100, 600 fixes flooding fo river Dender near Ninove
                            // 1000 fixes its flooding south of Lessines
                            // maps from navit mapserver need lots more
    int count;

    int highest_count = 0;

    struct point *p = g_alloca(sizeof(struct point) * max);

    if (order_corrected_2 < 0) {
        order_corrected_2 = 0;
    }
    sel.next = NULL;
    sel.order = order_corrected_2;
    sel.range.min = type_none;
    sel.range.max = type_last;
    sel.u.c_rect.lu.x = left_upper.x;
    sel.u.c_rect.lu.y = left_upper.y;
    sel.u.c_rect.rl.x = right_lower.x;
    sel.u.c_rect.rl.y = right_lower.y;

    mapset = navit_get_mapset(this.nav);

    navit_get_attr(this.nav,attr_layout,&layout_attr,NULL);
    layout = layout_attr.u.layout;

    // set map background color
    // todo make it retreive the correct color from the layout
    glClearColor(((float)layout->color.r)/65535, ((float)layout->color.g)/65535 , ((float)layout->color.b)/65535, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // check the hash and (re)fill if needed
    fillhash(layout, order_corrected);

    set_matrix(width, scale);

    // inform the shader of the scale, used for dashes and such
    glUniform1f(gvScaleXHandle, (GLfloat)(width/2.0));

    msh = mapset_open(mapset);
    while (msh && (map = mapset_next(msh, 0))) {
        if (map_get_attr(map, attr_name, &map_name_attr, NULL)) { // what if no binfiles ?
            //dbg(lvl_error,"map = %s\n", map_name_attr.u.str);
            if (strstr(map_name_attr.u.str, ".bin")) {
                //dbg(lvl_error,"map is a binfile\n");
                mr = map_rect_new(map, &sel);
                if (mr) {
                    while ((item = map_rect_get_item(mr))) {
                    //while ((item = map_rect_get_item_direct(mr))) {
                        struct openGL_hash_entry *entry;
                        //dbg(lvl_error,"item navme = %s\n", item_to_name(item->type));
                        if (item_to_name(item->type)) { // otherwise maps from navit mapserver fail
                            // because of recently added multipolygons
                            entry = g_hash_table_lookup(htab, item_to_name(item->type));
                        }
                        if (entry) {
                            if(entry->defer == FALSE) {
                                count = item_coord_get_within_selection(item, (struct coord *) p,
                                        max, &sel);
                                //count = item_coord_get(item, (struct coord *) p,
                                //        max);
                                if (count > highest_count) {
                                    highest_count = count;
                                }

                                //if (count > 1500) {
                                //    dbg(lvl_error,"over 1500 coords %s\n", item_to_name(item->type));
                                //}
                                if (count && count < max) {
                                    //dbg(lvl_error,"got items count = %i\n", count);
                                    struct attr attr_77;
                                    struct attr attr_osmlayer;
                                    int z_fix = 0;
                                    if (item_attr_get(item, attr_osm_layer, &attr_osmlayer)) {
                                        if ( 0 > attr_osmlayer.u.num) {
                                            z_fix = 1025 + attr_osmlayer.u.num; // draw tunnels before all other roads
                                            // to verify is this goes below railroads and such as well
                                        } else if ( attr_osmlayer.u.num > 0) {
                                            z_fix = 1900 + attr_osmlayer.u.num;
                                        }
                                        //dbg(lvl_error, "osm layer = %li, z_fix = %i\n", attr_osmlayer.u.num, z_fix);
                                    }
                                    //dbg(0,"draw type %s\n",item_to_name(item->type));
                                    // if (!strstr(item_to_name(item->type), "triang"))
                                    // draw non-triangulated elements
                                    draw_elements(entry, p, count, FALSE, z_fix);
                                }
                            }
                        }
                    }
                    draw(htab);
                    map_rect_destroy(mr);
                }
            }
        }
    }
    dbg(lvl_error,"highest coord count = %i\n",highest_count);
    //g_free(p);
    mapset_close(msh);
    //dbg(lvl_error,"leave\n");
}



/**
 * Pulls a bitmap and sends it to Java
 *
 * */
void
sendBitmap(JNIEnv* jnienv, jobject jTileProvider, int width, int height, int x, int y, int zoom, int rows_cols){
    dbg(lvl_error,"enter\n");
    void * pPixels;

    if (TileClass == NULL){
        if (!android_find_class_global("org/navitproject/navit/NavitMapTileProvider", &TileClass)){
            TileClass = NULL;
            dbg(lvl_error,"no TileClass\n");
            return;
        } else {
            dbg(lvl_debug,"found The TileClass Yeah\n");
        }
    }

    if (Preview_set_bitmap == NULL){
        android_find_method(TileClass, "receiveTile", "(Landroid/graphics/Bitmap;IIII)V", &Preview_set_bitmap);
    }

    if (Preview_set_bitmap == NULL){
        dbg(lvl_error, "no method found for receiveTile \n");
        return;
    } else {
        dbg(lvl_debug, "method found for receiveTile\n");
    }

    jclass bitmapConfig = (*jnienv)->FindClass(jnienv,"android/graphics/Bitmap$Config");
    jfieldID argb8888FieldID = (*jnienv)->GetStaticFieldID(jnienv,bitmapConfig, "ARGB_8888",
                                                          "Landroid/graphics/Bitmap$Config;");
    jobject argb8888Obj = (*jnienv)->GetStaticObjectField(jnienv,bitmapConfig, argb8888FieldID);

    jclass bitmapClass = (*jnienv)->FindClass(jnienv,"android/graphics/Bitmap");
    jmethodID createBitmapMethodID = (*jnienv)->GetStaticMethodID(jnienv,bitmapClass,"createBitmap",
                                                                   "(IILandroid/graphics/Bitmap$Config;)Landroid/graphics/Bitmap;");

    jobject bitmapObj = (*jnienv)->CallStaticObjectMethod(jnienv,bitmapClass, createBitmapMethodID,
                                                           width * rows_cols, height * rows_cols, argb8888Obj);
    //dbg(lvl_debug, "start reading pixesl\n");
    AndroidBitmap_lockPixels(jnienv, bitmapObj, &pPixels);
    glReadPixels(0, 0, width * rows_cols, height * rows_cols, GL_RGBA, GL_UNSIGNED_BYTE, pPixels);
    AndroidBitmap_unlockPixels(jnienv, bitmapObj);
    //dbg(lvl_debug, "end reading pixesl\n");
    (*jnienv)->CallVoidMethod(jnienv,jTileProvider, Preview_set_bitmap, bitmapObj, x, y, zoom, rows_cols);
    //dbg(lvl_error, "leave\n");

}


void
initOpenGl2(int size, int rows_cols)
{

    dbg(lvl_error,"size = %i, rows_cols = %i\n", size, rows_cols);
    int width = size;
    int height = size;

    GLuint gProgram;
    GLfloat lineWidthRange[2];

    const EGLint surfaceAttr[] =
            {
                    EGL_WIDTH, width * rows_cols,
                    EGL_HEIGHT, height * rows_cols,
                    EGL_NONE
            };

    EGLint eglMajVers, eglMinVers;
    EGLint numConfigs;

    eglDisp = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(eglDisp, &eglMajVers, &eglMinVers);

    // choose the first config, i.e. best config
    eglChooseConfig(eglDisp, confAttr, &eglConf, 1, &numConfigs);

    eglCtx = eglCreateContext(eglDisp, eglConf, EGL_NO_CONTEXT, ctxAttr);

    // create a pixelbuffer surface
    eglSurface = eglCreatePbufferSurface(eglDisp, eglConf, surfaceAttr);

    eglMakeCurrent(eglDisp, eglSurface, eglSurface, eglCtx);

    // end setupEGL



    // setup openGL

    printGLString("Version", GL_VERSION);
    printGLString("Vendor", GL_VENDOR);
    printGLString("Renderer", GL_RENDERER);
    printGLString("Extensions", GL_EXTENSIONS);


    gProgram = createProgram(gVertexShader, gFragmentShader);
    if (!gProgram) {
        dbg(lvl_error, "Could not create program.");
    //    return 0;
        return;
    }

    glUseProgram(gProgram);
    checkGlError("glUseProgram");

    glEnable(GL_DEPTH_TEST); // needed if drawing with z

    // gives GLint
    gvPositionHandle = glGetAttribLocation(gProgram, "vPosition");
    checkGlError("glGetAttribLocation vPosition");
    // but this wants GLuint
    glEnableVertexAttribArray(gvPositionHandle); //always used, enable as default


    gvMvprojHandle = glGetUniformLocation(gProgram, "mvproj");
    checkGlError("glGetUniformLocation mvproj");

    // gvInvMvprojHandle = glGetUniformLocation(gProgram, "invMvproj");
    // checkGlError("glGetUniformLocation mvproj");

    gvColorHandle = glGetUniformLocation(gProgram, "color");
    checkGlError("glGetUniformLocation color");

    // on openGLes2 translation can not yet be done by GPU because of overflow of GLfloat
    // gvMapcenterHandle = glGetUniformLocation(gProgram, "mapcenter");
    // checkGlError("glGetUniformLocation mapcenter");


    gvScaleXHandle = glGetUniformLocation(gProgram, "scaleX");
    checkGlError("glGetUniformLocation scaleX");

    gvZvalHandle = glGetUniformLocation(gProgram, "zval");
    checkGlError("glGetUniformLocation zval");

    gvUseDashesHandle = glGetUniformLocation(gProgram, "use_dashes");
    checkGlError("glGetUniformLocation use_dashes");

    gvSourcePointHandle = glGetUniformLocation(gProgram, "sourcePoint");
    checkGlError("glGetUniformLocation sourcePoint");

    glViewport(0, 0, width * rows_cols, height * rows_cols);
    checkGlError("glViewport");

    glGetFloatv(GL_ALIASED_LINE_WIDTH_RANGE, lineWidthRange);
    checkGlError("glGetFloatv linewidthrange");
    glMaxLineWidth = lineWidthRange[1];

    // setup openGL END
}

JNIEXPORT int JNICALL
Java_org_navitproject_navit_NavitMapTileProvider_initOpenGL(JNIEnv* env, jobject jTileProvider, jint size,
                                                               jint rows_cols)
{
    dbg(lvl_error,"init size = %i, rows_cols = %i\n", size, rows_cols)
 //   initOpenGl2(size, rows_cols);
}


//https://wiki.openstreetmap.org/wiki/Slippy_map_tilenames#C.2FC.2B.2B
int
long2tilex(double lon, int z)
{
    return (int)(floor((lon + 180.0) / 360.0 * (1 << z)));
}

int
lat2tiley(double lat, int z)
{
    double latrad = lat * M_PI/180.0;
    return (int)(floor((1.0 - asinh(tan(latrad)) / M_PI) / 2.0 * (1 << z)));
}

double
tilex2long(int x, int z)
{
    return x / (double)(1 << z) * 360.0 - 180;
}

double
tiley2lat(int y, int z)
{
    double n = M_PI - 2.0 * M_PI * y / (double)(1 << z);
    return 180.0 / M_PI * atan(0.5 * (exp(n) - exp(-n)));
}

// returns the upper left coord of a grid
// of tiles for a given tile coord
// used when drawing more than one tile
// at once,
int
up_left(int tilecoord, int rows_cols)
{
    return (tilecoord / rows_cols) * rows_cols;
}

JNIEXPORT int JNICALL
Java_org_navitproject_navit_NavitMapTileProvider_getTileOpenGL(JNIEnv* env, jobject jTileProvider, jint size,
        jint zoom, jint x, jint y)
{

    dbg(lvl_error,"enter\n");
    int width = size;
    int height = size;

    // must be at least 1, otherwise zero tiles
    // and probably a crash here or there
    // rows_cols = 2 gives a grid of 2 x 2 tiles
    int rows_cols = 4;


    if (!eglSurface) { // maybe some more tests, takes 60ms on AVD sdk29
        dbg(lvl_error,"no eglSurface\n");
        initOpenGl2(size, rows_cols);
    }

    dbg(lvl_error,"asked x = %i, y = %i\n", x, y);

    x = up_left(x, rows_cols);
    y = up_left(y, rows_cols);

    dbg(lvl_error,"upleft x = %i, y = %i\n", x, y);

    struct coord_geo lu;
    struct coord_geo rl;

    lu.lat = tiley2lat(y, zoom);
    lu.lng = tilex2long(x, zoom);

    rl.lat = tiley2lat(y + rows_cols, zoom);
    rl.lng = tilex2long(x + rows_cols, zoom);

    struct coord cu;
    struct coord cl;
    struct coord_geo gu;
    struct coord_geo gl;

    gu.lat = lu.lat;
    gu.lng = lu.lng;
    gl.lat = rl.lat;
    gl.lng = rl.lng;

    transform_from_geo(projection_mg, &gu, &cu);
    transform_from_geo(projection_mg, &gl, &cl);

    GLfloat scale2 = ((GLfloat) abs(cu.x - cl.x)) / (size * rows_cols);

    dbg(lvl_debug,"scale2 = %f\n", scale2);

    enum projection pro = projection_mg; // desired projection
    struct coord_geo g;
    struct coord c;

    g.lng = (lu.lng + rl.lng)/2;
    g.lat = (lu.lat + rl.lat)/2;

    transform_from_geo(pro, &g, &c);

    mapcenter_x = c.x;
    mapcenter_y = c.y;

    dbg(lvl_debug,"mapcenter x = %i, y = %i\n", c.x, c.y);

    draw_tile(env, jTileProvider, lu,  rl, size * rows_cols, size * rows_cols, scale2, zoom);


    // send the bitmap to Java
    sendBitmap(env, jTileProvider, size, size, x, y, zoom, rows_cols); //////////// hier rows_cols meesturen ?
                                                            ///////// dit zal dan eerder een per een
                                                            //////// zijn per tile
     /// misschien de basecoordinaten meegeven en dan xcol en ycol van onze matrix ook meegeven

    // each time or never ????
	shutdownEGL();

    dbg(lvl_error,"leave\n");
    return 1;
}

void
tileprovider_init(struct navit *nav) {
    dbg(lvl_error,"new tileprovider\n");
    this.nav = nav;
}

void
tileprovider_new(struct navit *nav) {
    dbg(lvl_error,"init tileprovider ------- 666\n");
    this.nav = nav;
}


/**
 * @brief	The plugin entry point
 *
 * @return	nothing
 *
 * The plugin entry point
 *
 */
void
plugin_init(void)
{
    struct attr callback;
    struct attr navit;
    struct attr_iter *iter;
    dbg(lvl_error,"init plugin tileprovider\n");
    callback.type=attr_callback;
    callback.u.callback=callback_new_attr_0(callback_cast(tileprovider_new), attr_navit);
    dbg(lvl_error,"init plugin tileprovider -- 2\n");
    config_add_attr(config, &callback);
    iter=config_attr_iter_new();
    dbg(lvl_error,"init plugin tileprovider -- 3\n");
    while (config_get_attr(config, attr_navit, &navit, iter))
        tileprovider_init(navit.u.navit);
    config_attr_iter_destroy(iter);
    dbg(lvl_error,"init plugin tileprovider --4\n");
}