#version 130

#include "commondefines.glsl"
#include "sphere_renderer/inc/flags_snippet.inc.glsl"
#include "sphere_renderer/inc/vertex_attributes.inc.glsl"
#include "core/bitflags.inc.glsl"
#include "core/tflookup.inc.glsl"
#include "core/tfconvenience.inc.glsl"
#include "sphere_renderer/inc/sphere_flags_vertex_attributes.inc.glsl"
#include "sphere_renderer/inc/vertex_mainstart.inc.glsl"
#include "sphere_renderer/inc/sphere_flags_vertex_getflag.inc.glsl"
#include "sphere_renderer/inc/vertex_color.inc.glsl"
#include "sphere_renderer/inc/vertex_postrans.inc.glsl"
#include "sphere_renderer/inc/vertex_spheretouchplane.inc.glsl"
#include "sphere_renderer/inc/vertex_clipping.inc.glsl"
#include "sphere_renderer/inc/vertex_posout.inc.glsl"
#include "sphere_renderer/inc/vertex_mainend.inc.glsl"
