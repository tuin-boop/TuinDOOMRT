#ifndef NOMINMAX
    #define NOMINMAX
#endif

#include "i_mainwindow.h"
#include "i_time.h"
#include "m_argv.h"
#include "win32rtvideo.h"

#include "base_sysfb.h"
#include "c_dispatch.h"
#include "hw_renderstate.h"
#include "g_levellocals.h"
#include "r_utility.h"
#include "v_draw.h"
#include "flatvertices.h"
#include "hw_bonebuffer.h"
#include "hw_lightbuffer.h"
#include "hw_skydome.h"
#include "hw_viewpointbuffer.h"
#include "i_modelvertexbuffer.h"
#include "p_lnspec.h"
#include "r_sky.h"
#include "image.h"
#include "texturemanager.h"
#include "filesystem.h"

#include "rt_state.h"

#include <shellapi.h>

#include <filesystem>
#include <limits>
#include <span>
#include <variant>
#include <ranges>
#include <unordered_set>


//
//
//
//
//
//

#define RG_USE_SURFACE_WIN32
#include <RTGL1/RTGL1.h>

RgInterface rt      = {};
FRtState    rtstate = {};

bool g_isremix{ false };

extern bool rt_isdoom2;

//
//
//
//
//
//

// clang-format off
template< typename T >
using ValueToCVarRef = 
    std::conditional_t< std::is_same_v< T, bool  >, FBoolCVarRef,
    std::conditional_t< std::is_same_v< T, int   >, FIntCVarRef,
    std::conditional_t< std::is_same_v< T, float >, FFloatCVarRef,
    void > > >;

template< typename T >
constexpr ECVarType ValueToCVarType = 
    std::is_same_v< T, bool  > ? ECVarType::CVAR_Bool :
    std::is_same_v< T, int   > ? ECVarType::CVAR_Int :
    std::is_same_v< T, float > ? ECVarType::CVAR_Float :
                                 ECVarType::CVAR_Dummy;

#define RT_CVAR( name, default_value, description ) \
    ValueToCVarRef< decltype( default_value ) > name; \
    static FCVarDecl cvardecl_##name = { \
        &name, \
        ValueToCVarType< decltype( default_value ) >, \
        CVAR_GLOBALCONFIG | ( ( #name )[ 0 ] == '_' ? 0 : CVAR_ARCHIVE ), \
        #name, \
        CVarValue<ValueToCVarType< decltype( default_value ) >>( default_value ), \
        description, \
        nullptr, }; \
    extern FCVarDecl const *const cvardeclref_##name; \
    MSVC_VSEG FCVarDecl const *const cvardeclref_##name GCC_VSEG = &cvardecl_##name;

#define RT_CVAR_COLOR( name, default_value, description ) \
    CVARD( Color, ##name, default_value, CVAR_GLOBALCONFIG | CVAR_ARCHIVE, description )
// clang-format on


// clang-format off
namespace cvar
{
    // NOTE: if name start with '_' then the cvar won't be archived

    RT_CVAR( rt_cpu_cullmode,           0,      "[IMPACTS CPU PERFORMANCE HEAVILY] 0: BSP + all neighbor sectors of visible,  1 - original GZDoom's BSP/clip checks,  2: uploading whole map, no culling at all" )
    RT_CVAR( rt_cpu_nocullradius,       10.f,   "[IMPACTS CPU PERFORMANCE] Radius (in meters) in which culling must not be applied. Applicable with rt_cpu_cullmode=0" )

    RT_CVAR( rt_autoexport,             true,   "if true: if map's gltf doesn't exist on disk, export to gltf "
                                                "and process the map as if it's static (which improves performance / stability)" )
    RT_CVAR( rt_autoexport_light,       200.f,  "On auto export to gltf, apply this multiplier to the sector light intensities" ) 
    RT_CVAR( rt_stockscenes,            false,  "use bundled hand-authored Doom II scenes instead of the generic isolated scene path" )
    RT_CVAR( rt_ltp_liquids,            false,  "enable native ray-traced animation for Liquid Texture Pack materials" )

    RT_CVAR( rt_classic,                0.f,    "[0.0,1.0] what portion of the screen to render with a classic mode" )
    RT_CVAR( rt_classic_mus,            true,   "if true, apply high pass filter to music when classic mode is enabled" )
    RT_CVAR( rt_classic_white,          3.0f,   "white point for classic renderer" )
    RT_CVAR( rt_classic_llmin,          0.07f,  "min light level: remaps a gzdoom sector light level from [0.0,1.0] range to [rt_classic_llMIN,rt_classic_llMAX]" )
    RT_CVAR( rt_classic_llmax,          1.0f,   "max light level: remaps a gzdoom sector light level from [0.0,1.0] range to [rt_classic_llMIN,rt_classic_llMAX]" )
    RT_CVAR( rt_classic_llpow,          5.0f,   "power to apply to convert a gzdoom sector light level [0.0,1.0] to visible intensity" )

    RT_CVAR( rt_framegen,               0,      "enable frame generation via DirectX 12 and DXGI swapchain. DLSS3 if rt_upscale_dlss>0, FSR3 if rt_upscale_fsr2>0. "
                                                "Values:  0=off  1=on  -1=run frame generation logic, but skip presentation of the generated frame." )
    RT_CVAR( rt_dxgi,                   false,  "use DXGI (DirectX 12) swapchain to present to screen, better compatibility with Windows windowing system" )
    RT_CVAR( rt_vsync,                  false,  "vertical synchronization to prevent tearing" )
    RT_CVAR( rt_hdr,                    false,  "enable HDR output for display" )

    RT_CVAR( rt_fluid,                  true,   "enable fluid simulation (blood)" )
    RT_CVAR( rt_fluid_budget,         100000,   "(APPLIED ONLY after disabling rt_fluid) fluid simulation particle budget " )
    RT_CVAR( rt_fluid_pradius,          0.1f,   "(APPLIED ONLY after disabling rt_fluid) radis of one particle (in meters) for fluid simulation" )
    RT_CVAR( rt_fluid_gravity_x,        0.f,    "gravity vector for fluid (horizontal, X), in m/s^2" )
    RT_CVAR( rt_fluid_gravity_y,        0.f,    "gravity vector for fluid (horizontal, Y), in m/s^2" )
    RT_CVAR( rt_fluid_gravity_z,        -9.8f,  "gravity vector for fluid (vertical), in m/s^2" )
    RT_CVAR( rt_blood_color_r,          0.4f,   "color for blood fluid (Red)" )
    RT_CVAR( rt_blood_color_g,          0.0f,   "color for blood fluid (Green)" )
    RT_CVAR( rt_blood_color_b,          0.0f,   "color for blood fluid (Blue)" )
    
    RT_CVAR( rt_renderscale,            0.f,    "[0.2, 1.0] resolution scale")
    RT_CVAR( rt_upscale_dlss,           0,      "0 - off, 1 - quality, 2 - balanced, 3 - perf, 4 - ultra perf, 5 - DLSS with rt_renderscale, 6 - DLAA. "
                                                "This controls the DLSS upscaling (Super Resolution) but not the Frame Generation" )
    RT_CVAR( rt_upscale_fsr2,           0,      "0 - off, 1 - quality, 2 - balanced, 3 - perf, 4 - ultra perf, 5 - FSR2 with rt_renderscale, 6 - native. "
                                                "This controls the FSR3 / FSR2 upscaling (Super Resolution), but not the Frame Generation.")
    RT_CVAR( rt_sharpen,                0,      "image sharpening; 0 - auto, 1 - naive, 2 - AMD CAS, 3 - force disable" )

    RT_CVAR( rt_remix_rayreconstr,      false,  "[only for RTX Remix] DLSS Ray Reconstruction - denoise path tracing with AI" )
    RT_CVAR( rt_remix_reflex,           true,   "[only for RTX Remix] Reflex - reduce latency between inputs and visible results" )
    RT_CVAR( rt_remix_taa,              0,      "[only for RTX Remix] temporal anti aliasing. 0 - off, 1 - quality, 2 - balanced, 3 - perf, 4 - ultra perf, 5 - FSR2 with rt_renderscale, 6 - native" )

    RT_CVAR( rt_shadowrays,             4,      "max depth of shadow ray casts" )
    RT_CVAR( rt_withplayer,             true,   "enable player model for shadows, reflections etc" )
    RT_CVAR( rt_lerpmdlangle,           true,   "interpolate subtick rotation for replacements" )
    RT_CVAR( rt_spectre,                0,      "render spectres as: 0 - water, 1 - glass, 2 - mirror" )
    RT_CVAR( rt_spectre_invis1,         0,      "render first-person weapons, viewer invisibility as: 0 - water, 1 - glass, 2 - mirror" )
    RT_CVAR( rt_znear,                  0.07f,  "camera near plane (in meters); precision problems occur on a first-person weapons if too small (<=0.05)" )
    RT_CVAR( rt_zfar,                   2048.f, "camera far plane (in meters); precision problems occur on a first-person weapons if too large" )

    RT_CVAR( rt_normalmap_stren,        1.f,    "normal map influence" )
    RT_CVAR( rt_heightmap_stren,        1.f,    "height map influence" )
    RT_CVAR( rt_emis_mapboost,          200.f,  "indirect illumination emissiveness" )
    RT_CVAR( rt_emis_maxscrcolor,       8.f,    "burn on-screen emissive colors" )
    RT_CVAR( rt_emis_additive_dflt,     0.5f,   "emission value for objects with additive blending" )
    RT_CVAR( rt_smoothtextures,         false,  "enable linear texture filtering" )

    RT_CVAR( rt_tnmp_ev100_min,         2.f,    "min brightness for auto-exposure" )
    RT_CVAR( rt_tnmp_ev100_max,         7.7f,   "max brightness for auto-exposure" )
    RT_CVAR( rt_tnmp_saturation_r,      0.f,    "-1 desaturate, +1 over saturate" )
    RT_CVAR( rt_tnmp_saturation_g,      0.f,    "-1 desaturate, +1 over saturate" )
    RT_CVAR( rt_tnmp_saturation_b,      0.f,    "-1 desaturate, +1 over saturate" )
    RT_CVAR( rt_tnmp_crosstalk_r,       1.0f,   "how much to shift Red, when Green or Blue are intense; set one channel to 1.0, others to <= 1.0" )
    RT_CVAR( rt_tnmp_crosstalk_g,       0.7f,   "how much to shift Green, when Red or Blue are intense; set one channel to 1.0, others to <= 1.0" )
    RT_CVAR( rt_tnmp_crosstalk_b,       0.8f,   "how much to shift Blue, when Red or Green are intense; set one channel to 1.0, others to <= 1.0" )
    RT_CVAR( rt_tnmp_contrast,          0.1f,   "(only if rt_hdr is OFF) LDR contrast" )
    RT_CVAR( rt_hdr_contrast,           0.15f,  "(only if rt_hdr is ON) HDR contrast" )
    RT_CVAR( rt_hdr_saturation,         0.15f,  "(only if rt_hdr is ON) HDR saturation: -1 desaturate, +1 over saturate" )
    RT_CVAR( rt_hdr_brightness,         1.0f,   "(only if rt_hdr is ON) HDR brightess multiplier" )

    RT_CVAR( rt_sky,                    100.f,  "sky intensity")
    RT_CVAR( rt_sky_saturation,         1.f,    "sky saturation")
    RT_CVAR( rt_sky_stretch,            1.2f,   "how much to stretch the sky sphere along the vertical axis")
    RT_CVAR( rt_sky_always,             true,   "always submit sky geometry (even if it's not visible in primary view)")
    RT_CVAR( rt_doom_e1_realistic_lights, false, "use the brown Martian landscape and synchronized visible sun on stock E1M1-E1M8")
    RT_CVAR( rt_doom_e2_realistic_lights, false, "use the Deimos landscape and hidden red rift light on stock E2M1-E2M8")
    RT_CVAR( rt_doom_e3_realistic_lights, false, "use the Inferno landscape and hidden burning-horizon light on stock E3M1-E3M8")
    RT_CVAR( rt_doom2_realistic_lights, false, "use three authored skies and synchronized lighting on stock Doom II MAP01-MAP32")
    RT_CVAR( rt_doom_e1_sun_size,        8.0f,   "visible Episode 1 sun angular diameter in degrees")

    RT_CVAR( rt_decals,                 true,   "draw decals. NOTE: impacts CPU performance, as gzdoom requires a doom-wall to be fullyparsed to submit its decals :(")

    RT_CVAR( rt_lightlevel_min,            80,  "[replacements lights] min bound for translating gzdoom lightlevel to light intensity: if lightlevel below this, lights are multiplied by 0.0; must be >= 0" )
    RT_CVAR( rt_lightlevel_max,           230,  "[replacements lights] max bound for translating gzdoom lightlevel to light intensity: if lightlevel above this, lights are multiplied by 1.0; must be <= 255" )
    RT_CVAR( rt_lightlevel_exp,          2.0f,  "[replacements lights] exponent to apply when converting gzdoom lightlevel to light intensity" )
    RT_CVAR( rt_ceilinglights,            true, "add local ray-traced lights below common luminous ceiling textures" )
    RT_CVAR( rt_ceilinglight_intensity,   75.f, "intensity of automatic luminous ceiling lights" )
    RT_CVAR( rt_ceilinglight_radius,      0.3f, "source radius of automatic ceiling lights in meters" )
    RT_CVAR( rt_ceilinglight_maxcount,        4, "maximum automatic ceiling lights per luminous sector (1-8)" )
    RT_CVAR( rt_ceilingbeams,              true, "add downward volumetric spot beams below luminous ceilings" )
    RT_CVAR( rt_ceilingbeam_intensity,    180.f, "intensity of automatic ceiling spot beams" )
    RT_CVAR( rt_ceilingbeam_radius,       0.12f, "source radius of automatic ceiling spot beams in meters" )
    RT_CVAR( rt_ceilingbeam_angle,          24.f, "outer angle of automatic ceiling spot beams in degrees" )
    RT_CVAR( rt_ceilingbeam_maxcount,          2, "maximum volumetric spot beams per luminous sector (0-8)" )

    RT_CVAR( rt_flsh,                   false,  "flashlight enable")
    RT_CVAR( rt_flsh_intensity,         200.f,  "flashlight intensity")
    RT_CVAR( rt_flsh_volumetric,        true,   "use the flashlight as the active volumetric light source")
    RT_CVAR( rt_flsh_radius,            0.02f,  "flashlight source disk radius in meters")
    RT_CVAR( rt_flsh_angle,             35.f,   "flashlight width in degrees")
    RT_CVAR( rt_flsh_r,                 -0.3f,  "flashlight position offset - right (in meteres)")
    RT_CVAR( rt_flsh_u,                 -0.7f,  "flashlight position offset - up (in meteres)")
    RT_CVAR( rt_flsh_f,                 0.0f,   "flashlight position offset - forward (in meteres)")

    RT_CVAR( rt_sun,                    false,  "enable sun for debugging")
    RT_CVAR( rt_sun_intensity,           100.f, "sun intensity")
    RT_CVAR( rt_sun_a,                  45.f,   "[-90, 90] sun altitude angle; how high it is from the horizon")
    RT_CVAR( rt_sun_b,                  0.f,    "[0, 360] sun azimuth angle; hotizontal angle, counter-clockwise")
    RT_CVAR_COLOR( rt_sun_color,      0xFFFFFF, "sun color (hex)")

    RT_CVAR( rt_autosun,                true,   "automatically add deterministic daylight to maps with sky sectors")
    RT_CVAR( rt_autosun_intensity,       100.f, "base intensity of automatic outdoor sunlight")
    RT_CVAR( rt_autosun_minaltitude,     8.f,   "minimum automatic sun altitude in degrees")
    RT_CVAR( rt_autosun_maxaltitude,    30.f,   "maximum automatic sun altitude in degrees")
    RT_CVAR( rt_autosun_softness,       1.0f,   "automatic sun angular diameter in degrees; larger values make softer shadows")
    RT_CVAR( rt_autosun_seed,           0,      "change this value to choose another stable time of day for every level")

    RT_CVAR( rt_reflrefr_depth,         8,      "max depth of reflect/refract") 
    RT_CVAR( rt_refr_glass,             1.52f,  "glass index of refraction") 
    RT_CVAR( rt_refr_water,             1.33f,  "water index of refraction") 
    RT_CVAR( rt_refr_thinwidth,         0.0f,   "approx. width of thin media, e.g. thin glass (in meters)") 
    RT_CVAR( rt_refl_thresh,            0.0f,   "assume mirror if roughness is less than this value") 

    RT_CVAR( rt_mzlflsh,                true,   "enable muzzle flash light source (activated on extralight)" )
    RT_CVAR( rt_mzlflsh_intensity,      100.f,  "muzzle flash intensity" )
    RT_CVAR_COLOR( rt_mzlflsh_color,  0xFF8C52, "muzzle flash color (hex)" )
    RT_CVAR( rt_mzlflsh_radius,         0.02f,  "muzzle flash light sphere radius (in meters)")
    RT_CVAR( rt_mzlflsh_offset,         0.6f,   "[0.0, 1.0] muzzle flash offset from the hit point, so the light would not be in a wall")
    RT_CVAR( rt_mzlflsh_f,              3.0f,   "muzzle flash light offset - forward (in meteres)" )
    RT_CVAR( rt_mzlflsh_u,              -0.9f,  "muzzle flash light offset - up (in meteres)" )

    RT_CVAR( rt_volume_type,            1,      "0 - none, 1 - volumetric, 2 - distance based" )
    RT_CVAR( rt_volume_far,             30.f,   "max distance of scattering volume (in meteres)" )
    RT_CVAR( rt_volume_scatter,         1.f,    "density of media" )
    RT_CVAR( rt_volume_ambient,         0.2f,   "ambient term" )
    RT_CVAR( rt_volume_lintensity,      1.f,    "intensity of lights for scattering" )
    RT_CVAR( rt_volume_lassymetry,      0.5f,   "scaterring phase function assymetry" )
    RT_CVAR( rt_volume_history,         8.f,    "max history length for scaterring accumulation (in frames)" )
    RT_CVAR( rt_volume_local_lights,    true,   "allow local lights such as the flashlight and ceiling spots to illuminate volumetric fog" )

    RT_CVAR( rt_water_r,                255,    "water color Red [0,255]" )
    RT_CVAR( rt_water_g,                255,    "water color Green [0,255]" )
    RT_CVAR( rt_water_b,                255,    "water color Blue [0,255]" )
    RT_CVAR( rt_water_wavestren,        3.f,    "normal map strength for water" )

    RT_CVAR( rt_bloom,                  true,   "enable bloom" )
    RT_CVAR( rt_bloom_scale,            1.f,    "multiplier for a calculated bloom" )
    RT_CVAR( rt_bloom_ev,               6.f,    "EV offset for bloom calculation input" )
    RT_CVAR( rt_bloom_threshold,        16.f,   "brightness threshold for bloom calculation input" )
    RT_CVAR( rt_bloom_dirt,             true,   "lens dirt enable" )
    RT_CVAR( rt_bloom_dirt_scale,       1.5f,   "lens dirt multiplier" )
    
    RT_CVAR( rt_ef_crt,                 false,  "CRT-monitor filter" )
    RT_CVAR( rt_ef_chraber,             0.15f,  "chromatic aberration intensity" )
    RT_CVAR( rt_ef_vhs,                 0.f,    "VHS filter intensity" )
    RT_CVAR( rt_ef_dither,              0.f,    "dithering filter intensity" )
    RT_CVAR( rt_ef_vintage,             0,      "[0, 7] vintage effects, disabled if rt_renderscale>0" ) // look RT_VINTAGE_* enum
    RT_CVAR( rt_ef_water,               true,   "warp screen while under water" )

    RT_CVAR( rt_pw_lightamp,            0,      "light amplification powerup type: 0 - night vision, 1 - thermal camera, 2 - flashlight" )

    RT_CVAR( rt_melt_duration,          1.5f,   "screen melt effect duration" )

    RT_CVAR( rt_wall_nomv,              1,      "0: motion vectors always,  1: use pegging flags to determine wall motion vectors,  2: always force no motion vectors on walls. "
                                                "This option is needed to fix illumination motion artifacts on lifts / crashers" )

    RT_CVAR( hack_initialframesskip,    true,   "skip initial a couple of frames on game launch; if not skipped, there might be a distracting flashing of the main window" )

    RT_CVAR( _rt_showexportable,        false,  "internal variable; only in debug" )

	// default, so when user launches a game with CRT/Vintage,
	// and after that changes to dlss/fsr2, then this value will be set to the cvars;
	// 2 = balanced; non-archived
    RT_CVAR( _rt_cachedpreset,          2,      "internal variable for menu UX" )

    bool rt_available_dlss2   = false;
    bool rt_available_dlss3fg = false;
    bool rt_available_fsr2    = false;
    bool rt_available_fsr3fg  = false;
    bool rt_available_dxgi    = false;

    const char* rt_failreason_dlss2   = nullptr;
    const char* rt_failreason_dlss3fg = nullptr;
    const char* rt_failreason_fsr2    = nullptr;
    const char* rt_failreason_fsr3fg  = nullptr;
    const char* rt_failreason_dxgi    = nullptr;

    bool rt_hdr_available = false;
    bool rt_fluid_available = false;

    bool rt_firststart = false;
}
// clang-format on

EXTERN_CVAR( Float, blood_fade_scalar );
EXTERN_CVAR( Float, pickup_fade_scalar );

//
//
//
//
//
//

const char* g_rt_cutscenename        = nullptr;
bool        g_rt_showfirststartscene = false;
int         g_rt_skipinitframes      = -10; // to prevent flashing when starting the game
bool        g_rt_forcenofocuschange  = true;
int         rt_cullmode              = 2; // 0 -- balanced,  1 -- original gzdoom,  2 -- none

extern float RT_CutsceneTime();
extern void  RT_ForceIntroCutsceneMusicStop();

extern void RT_CloseLauncherWindow();

auto RT_MakeUpRightForwardVectors( const DRotator& rotation ) -> std::tuple< RgFloat3D, RgFloat3D, RgFloat3D >;

RgFloat3D g_rt_mainCameraPosition{};
RgFloat3D g_rt_mainCameraUp{};
RgFloat3D g_rt_mainCameraRight{};
bool      g_rt_mainCameraValid = false;

namespace
{

void RG_CHECK( RgResult r )
{
    assert( ( r ) == RG_RESULT_SUCCESS );
}

#define RG_TRANSFORM_IDENTITY              \
    {                                      \
        1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0 \
    }

constexpr auto ORIGINAL_DOOM_RESOLUTION_HEIGHT = 200;
constexpr auto ONEGAMEUNIT_IN_METERS           = 1.0f / 32.0f; // https://doomwiki.org/wiki/Map_unit

constexpr auto RG_PACKED_COLOR_WHITE = RgColor4DPacked32{ 0xFFFFFFFF };


enum
{
    RT_VINTAGE_OFF,
    RT_VINTAGE_CRT,
    RT_VINTAGE_VHS,
    RT_VINTAGE_VHS_CRT,
    RT_VINTAGE_200,
    RT_VINTAGE_200_DITHER,
    RT_VINTAGE_480,
    RT_VINTAGE_480_DITHER,
};


constexpr uint64_t FlashlightLightId  = 0xFFFFFFF + 0;
constexpr uint64_t SunLightId         = 0xFFFFFFF + 1;
constexpr uint64_t MuzzleFlashLightId = 0xFFFFFFF + 2;
constexpr uint64_t SectorLightId_Base = 0xFFFFFFF + 3;
constexpr uint64_t CeilingLightId_Base = 0x1FFFFFFF;
constexpr uint64_t CeilingLightId_Stride = 16;
constexpr uint64_t CeilingBeamId_Offset = 8;
constexpr uint64_t WeaponCompatProjectileLightSalt = 0x4A6F792D50524A31ull;



namespace
{
std::string RT_SanitizeSceneComponent( const char* value, size_t maxLength )
{
    std::string result;
    result.reserve( maxLength );

    if( !value )
    {
        return result;
    }

    bool previousWasSeparator = false;
    for( const unsigned char c : std::string_view{ value } )
    {
        if( result.size() >= maxLength )
        {
            break;
        }

        if( std::isalnum( c ) )
        {
            result.push_back( char( std::tolower( c ) ) );
            previousWasSeparator = false;
        }
        else if( !result.empty() && !previousWasSeparator )
        {
            result.push_back( '_' );
            previousWasSeparator = true;
        }
    }

    while( !result.empty() && result.back() == '_' )
    {
        result.pop_back();
    }
    return result;
}

std::string RT_GetResourceSceneComponent( int resourceIndex )
{
    const char* filename = fileSystem.GetResourceFileName( resourceIndex );
    std::string filenameWithoutExtension = filename ? filename : "custom";
    if( const size_t dot = filenameWithoutExtension.find_last_of( '.' );
        dot != std::string::npos )
    {
        filenameWithoutExtension.resize( dot );
    }

    std::string resource = RT_SanitizeSceneComponent( filenameWithoutExtension.c_str(), 48 );
    if( resource.empty() )
    {
        resource = "custom";
    }

    // FileSystem's resource hash is stable across moves and changes whenever
    // the archive layout changes. A suffix is enough to avoid same-name PWAD
    // collisions without creating excessively long scene directory names.
    if( const char* hash = fileSystem.GetResourceFileHash( resourceIndex ) )
    {
        std::string_view hashView{ hash };
        constexpr size_t HashSuffixLength = 12;
        if( hashView.size() > HashSuffixLength )
        {
            hashView.remove_prefix( hashView.size() - HashSuffixLength );
        }

        std::string hashSuffix = RT_SanitizeSceneComponent(
            std::string{ hashView }.c_str(), HashSuffixLength );
        if( !hashSuffix.empty() )
        {
            resource += '_';
            resource += hashSuffix;
        }
    }

    return resource;
}
} // namespace

const char* RT_GetMapName()
{
    if( g_rt_cutscenename && g_rt_cutscenename[ 0 ] != '\0' )
    {
        return g_rt_cutscenename;
    }

    if( primaryLevel && !primaryLevel->MapName.IsEmpty() )
    {
        static std::string sceneName;
        std::string mapName = RT_SanitizeSceneComponent(
            primaryLevel->MapName.GetChars(), 32 );

        const int mapResource = fileSystem.GetFileContainer( primaryLevel->lumpnum );
        const bool useStockDoom2Scene =
            cvar::rt_stockscenes && rt_isdoom2 && mapResource == fileSystem.GetIwadNum();

        if( useStockDoom2Scene )
        {
            // Preserve the original keys so the bundled hand-authored Doom II
            // scenes (map01, map02, ...) continue to load.
            sceneName = std::move( mapName );
        }
        else
        {
            sceneName = RT_GetResourceSceneComponent( mapResource );
            sceneName += '_';
            sceneName += mapName;
        }

        static std::string lastReportedSceneName;
        if( sceneName != lastReportedSceneName )
        {
            Printf( "RT scene key: %s\n", sceneName.c_str() );
            lastReportedSceneName = sceneName;
        }

        return sceneName.c_str();
    }

    if( g_rt_showfirststartscene )
    {
        // HACKHACK: do not show scene at the first frame: cutscene's firststart::draw is not called at that time :(
        static bool HACKHACK_firstframeskipped = false;
        if( !HACKHACK_firstframeskipped )
        {
            HACKHACK_firstframeskipped = true;
            return nullptr;
        }

        return "mainmenu";
    }

    return nullptr;
}

bool RT_UseDoomE1RealisticLights( const FLevelLocals* level )
{
    if( !bool{ cvar::rt_doom_e1_realistic_lights } || !level || rt_isdoom2 )
    {
        return false;
    }

    const std::string_view mapName{ level->MapName.GetChars() };
    if( mapName.size() != 4 || std::tolower( static_cast< unsigned char >( mapName[ 0 ] ) ) != 'e' ||
        mapName[ 1 ] != '1' || std::tolower( static_cast< unsigned char >( mapName[ 2 ] ) ) != 'm' ||
        mapName[ 3 ] < '1' || mapName[ 3 ] > '8' )
    {
        return false;
    }

    // Do not force the special sky onto PWAD replacements that happen to use an
    // Episode 1 map name. This mode is deliberately scoped to the stock maps.
    return fileSystem.GetFileContainer( level->lumpnum ) == fileSystem.GetIwadNum();
}

bool RT_UseDoomE2RealisticLights( const FLevelLocals* level )
{
    if( !bool{ cvar::rt_doom_e2_realistic_lights } || !level || rt_isdoom2 )
    {
        return false;
    }

    const std::string_view mapName{ level->MapName.GetChars() };
    if( mapName.size() != 4 || std::tolower( static_cast< unsigned char >( mapName[ 0 ] ) ) != 'e' ||
        mapName[ 1 ] != '2' || std::tolower( static_cast< unsigned char >( mapName[ 2 ] ) ) != 'm' ||
        mapName[ 3 ] < '1' || mapName[ 3 ] > '8' )
    {
        return false;
    }

    // Keep the authored panorama limited to the original Episode 2 maps.
    return fileSystem.GetFileContainer( level->lumpnum ) == fileSystem.GetIwadNum();
}

bool RT_UseDoomE3RealisticLights( const FLevelLocals* level )
{
    if( !bool{ cvar::rt_doom_e3_realistic_lights } || !level || rt_isdoom2 )
    {
        return false;
    }

    const std::string_view mapName{ level->MapName.GetChars() };
    if( mapName.size() != 4 || std::tolower( static_cast< unsigned char >( mapName[ 0 ] ) ) != 'e' ||
        mapName[ 1 ] != '3' || std::tolower( static_cast< unsigned char >( mapName[ 2 ] ) ) != 'm' ||
        mapName[ 3 ] < '1' || mapName[ 3 ] > '8' )
    {
        return false;
    }

    return fileSystem.GetFileContainer( level->lumpnum ) == fileSystem.GetIwadNum();
}

int RT_GetDoom2RealisticGroup( const FLevelLocals* level )
{
    if( !bool{ cvar::rt_doom2_realistic_lights } || !level || !rt_isdoom2 )
    {
        return 0;
    }

    const std::string_view mapName{ level->MapName.GetChars() };
    if( mapName.size() != 5 ||
        std::tolower( static_cast< unsigned char >( mapName[ 0 ] ) ) != 'm' ||
        std::tolower( static_cast< unsigned char >( mapName[ 1 ] ) ) != 'a' ||
        std::tolower( static_cast< unsigned char >( mapName[ 2 ] ) ) != 'p' ||
        mapName[ 3 ] < '0' || mapName[ 3 ] > '9' ||
        mapName[ 4 ] < '0' || mapName[ 4 ] > '9' )
    {
        return 0;
    }

    const int mapNumber = ( mapName[ 3 ] - '0' ) * 10 + ( mapName[ 4 ] - '0' );
    if( mapNumber < 1 || mapNumber > 32 ||
        fileSystem.GetFileContainer( level->lumpnum ) != fileSystem.GetIwadNum() )
    {
        return 0;
    }

    return mapNumber <= 11 ? 1 : ( mapNumber <= 20 ? 2 : 3 );
}

namespace
{
struct DoomE1SunPreset
{
    float altitude;
    float azimuth;
};

DoomE1SunPreset RT_GetDoomE1SunPreset( int seed )
{
    // With the old Mars body removed, the visible sun can travel around the
    // complete horizon instead of being restricted to two clear arcs.
    constexpr DoomE1SunPreset presets[] = {
        { 52.f,   0.f }, { 44.f,  30.f }, { 58.f,  60.f },
        { 40.f,  90.f }, { 54.f, 120.f }, { 46.f, 150.f },
        { 60.f, 180.f }, { 42.f, 210.f }, { 56.f, 240.f },
        { 45.f, 270.f }, { 58.f, 300.f }, { 48.f, 330.f },
    };
    constexpr int presetCount = int( std::size( presets ) );
    return presets[ ( ( seed % presetCount ) + presetCount ) % presetCount ];
}

struct DoomE2RiftLightPreset
{
    float altitude;
    float azimuth;
};

DoomE2RiftLightPreset RT_GetDoomE2RiftLightPreset( int seed )
{
    // These high, soft directions represent different bright regions of the
    // overhead Hell rift. There is deliberately no visible sun disc.
    constexpr DoomE2RiftLightPreset presets[] = {
        { 68.f, 210.f },
        { 62.f, 250.f },
        { 72.f, 170.f },
        { 65.f, 120.f },
        { 58.f, 300.f },
        { 70.f, 35.f },
    };
    constexpr int presetCount = int( std::size( presets ) );
    return presets[ ( ( seed % presetCount ) + presetCount ) % presetCount ];
}

struct DoomE3HorizonLightPreset
{
    float altitude;
    float azimuth;
};

DoomE3HorizonLightPreset RT_GetDoomE3HorizonLightPreset( int seed )
{
    // A low burning horizon supplies long shadows without drawing a sun body.
    constexpr DoomE3HorizonLightPreset presets[] = {
        { 16.f, 205.f },
        { 12.f, 250.f },
        { 20.f, 165.f },
        { 14.f, 115.f },
        { 24.f, 305.f },
        { 18.f, 35.f },
    };
    constexpr int presetCount = int( std::size( presets ) );
    return presets[ ( ( seed % presetCount ) + presetCount ) % presetCount ];
}

struct Doom2ChapterLightPreset
{
    float altitude;
    float azimuth;
};

Doom2ChapterLightPreset RT_GetDoom2ChapterLightPreset( int group, int seed )
{
    constexpr Doom2ChapterLightPreset wasteland[] = {
        { 32.f,   0.f }, { 24.f,  30.f }, { 38.f,  60.f },
        { 28.f,  90.f }, { 35.f, 120.f }, { 22.f, 150.f },
        { 36.f, 180.f }, { 26.f, 210.f }, { 40.f, 240.f },
        { 25.f, 270.f }, { 34.f, 300.f }, { 29.f, 330.f },
    };
    constexpr Doom2ChapterLightPreset city[] = {
        { 42.f,   0.f }, { 34.f,  30.f }, { 48.f,  60.f },
        { 38.f,  90.f }, { 45.f, 120.f }, { 32.f, 150.f },
        { 46.f, 180.f }, { 36.f, 210.f }, { 50.f, 240.f },
        { 35.f, 270.f }, { 44.f, 300.f }, { 39.f, 330.f },
    };
    constexpr Doom2ChapterLightPreset hell[] = {
        { 14.f, 205.f }, { 10.f, 250.f }, { 18.f, 165.f },
        { 12.f, 115.f }, { 20.f, 305.f }, { 16.f, 35.f },
    };

    const auto pick = [ seed ]( const auto& presets ) {
        const int count = int( std::size( presets ) );
        return presets[ ( ( seed % count ) + count ) % count ];
    };
    return group == 1 ? pick( wasteland ) : ( group == 2 ? pick( city ) : pick( hell ) );
}

bool RT_EnsureDoomE1BillboardTexture( const char* sourceName,
                                     const char* runtimeName,
                                     const char* label,
                                     bool& uploaded )
{
    if( uploaded )
    {
        return true;
    }

    const FTextureID id = TexMan.CheckForTexture(
        sourceName, ETextureType::Any, FTextureManager::TEXMAN_TryAny );
    FGameTexture* gameTexture = id.Exists() ? TexMan.GetGameTexture( id, false ) : nullptr;
    FTexture* texture = gameTexture ? gameTexture->GetTexture() : nullptr;
    if( !texture )
    {
        return false;
    }

    auto buffer = texture->CreateTexBuffer( 0, CTF_ProcessData );
    if( !buffer.mBuffer || buffer.mWidth <= 0 || buffer.mHeight <= 0 )
    {
        return false;
    }

    auto details = RgOriginalTextureDetailsEXT{
        .sType  = RG_STRUCTURE_TYPE_ORIGINAL_TEXTURE_DETAILS_EXT,
        .pNext  = nullptr,
        .flags  = 0u,
        .format = RG_FORMAT_B8G8R8A8_SRGB,
    };
    auto info = RgOriginalTextureInfo{
        .sType        = RG_STRUCTURE_TYPE_ORIGINAL_TEXTURE_INFO,
        .pNext        = &details,
        .pTextureName = runtimeName,
        .pPixels      = buffer.mBuffer,
        .size         = { static_cast< uint32_t >( buffer.mWidth ),
                          static_cast< uint32_t >( buffer.mHeight ) },
        .filter       = RG_SAMPLER_FILTER_LINEAR,
        .addressModeU = RG_SAMPLER_ADDRESS_MODE_CLAMP,
        .addressModeV = RG_SAMPLER_ADDRESS_MODE_CLAMP,
    };

    if( rt.rgProvideOriginalTexture( &info ) != RG_RESULT_SUCCESS )
    {
        return false;
    }

    uploaded = true;
    Printf( "RT Doom Episode 1 %s texture ready (%dx%d)\n",
            label, buffer.mWidth, buffer.mHeight );
    return true;
}

bool RT_EnsureDoomE1SunTexture()
{
    static bool uploaded = false;
    return RT_EnsureDoomE1BillboardTexture(
        "textures/tuindoom/sun-bright.png", "tuindoom/e1_sun", "sun", uploaded );
}

void RT_UploadDoomE1SkyBillboard( const RgFloat3D& directionFromObject,
                                  float angularDiameter,
                                  const char* textureName )
{
    constexpr float distance = 200.f;
    constexpr float DegreesToRadians = 0.017453292519943295f;
    const float halfSize =
        std::tan( angularDiameter * DegreesToRadians * 0.5f ) * distance;

    RgFloat3D center{};
    for( int axis = 0; axis < 3; ++axis )
    {
        center.data[ axis ] = g_rt_mainCameraPosition.data[ axis ] -
                              directionFromObject.data[ axis ] * distance;
    }

    const auto makeVertex = [ & ]( float rightScale, float upScale, float u, float v ) {
        RgPrimitiveVertex vertex{};
        for( int axis = 0; axis < 3; ++axis )
        {
            vertex.position[ axis ] = center.data[ axis ] +
                                      g_rt_mainCameraRight.data[ axis ] * rightScale +
                                      g_rt_mainCameraUp.data[ axis ] * upScale;
        }
        vertex.texCoord[ 0 ] = u;
        vertex.texCoord[ 1 ] = v;
        vertex.color = rt.rgUtilPackColorByte4D( 255, 255, 255, 255 );
        return vertex;
    };

    constexpr uint32_t indices[] = { 0, 1, 2, 2, 3, 0 };
    constexpr int OcclusionTiles = 12;
    for( int tileY = 0; tileY < OcclusionTiles; ++tileY )
    {
        for( int tileX = 0; tileX < OcclusionTiles; ++tileX )
        {
            const float u0 = float( tileX ) / float( OcclusionTiles );
            const float u1 = float( tileX + 1 ) / float( OcclusionTiles );
            const float v0 = float( tileY ) / float( OcclusionTiles );
            const float v1 = float( tileY + 1 ) / float( OcclusionTiles );
            const float right0 = std::lerp( -halfSize, halfSize, u0 );
            const float right1 = std::lerp( -halfSize, halfSize, u1 );
            const float up0 = std::lerp( halfSize, -halfSize, v0 );
            const float up1 = std::lerp( halfSize, -halfSize, v1 );
            const RgPrimitiveVertex vertices[] = {
                makeVertex( right0, up0, u0, v0 ),
                makeVertex( right1, up0, u1, v0 ),
                makeVertex( right1, up1, u1, v1 ),
                makeVertex( right0, up1, u0, v1 ),
            };

            RgFloat3D pointToCheck{};
            const float tileRight = ( right0 + right1 ) * 0.5f;
            const float tileUp = ( up0 + up1 ) * 0.5f;
            for( int axis = 0; axis < 3; ++axis )
            {
                pointToCheck.data[ axis ] = center.data[ axis ] +
                                            g_rt_mainCameraRight.data[ axis ] * tileRight +
                                            g_rt_mainCameraUp.data[ axis ] * tileUp;
            }

            auto tile = RgLensFlareInfo{
                .sType        = RG_STRUCTURE_TYPE_LENS_FLARE_INFO,
                .pNext        = nullptr,
                .vertexCount  = static_cast< uint32_t >( std::size( vertices ) ),
                .pVertices    = vertices,
                .indexCount   = static_cast< uint32_t >( std::size( indices ) ),
                .pIndices     = indices,
                .pTextureName = textureName,
                .pointToCheck = pointToCheck,
            };
            RG_CHECK( rt.rgUploadLensFlare( &tile ) );
        }
    }
}

} // namespace

CCMD( rt_printscenekey )
{
    const char* sceneName = RT_GetMapName();
    Printf( "RT scene key: %s\n", sceneName ? sceneName : "(none)" );
}

static void RT_ApplySunCycleAngle( int seed )
{
    if( RT_UseDoomE1RealisticLights( primaryLevel ) )
    {
        const DoomE1SunPreset preset = RT_GetDoomE1SunPreset( seed );
        cvar::rt_sun           = true;
        cvar::rt_sun_a         = preset.altitude;
        cvar::rt_sun_b         = preset.azimuth;
        Printf( "RT Episode 1 sun position %d: altitude %.1f, azimuth %.1f, intensity %.0f\n",
                seed, preset.altitude, preset.azimuth, float{ cvar::rt_sun_intensity } );
        return;
    }

    if( RT_UseDoomE2RealisticLights( primaryLevel ) )
    {
        const DoomE2RiftLightPreset preset = RT_GetDoomE2RiftLightPreset( seed );
        cvar::rt_sun           = true;
        cvar::rt_sun_a         = preset.altitude;
        cvar::rt_sun_b         = preset.azimuth;
        Printf( "RT Episode 2 rift-light position %d: altitude %.1f, azimuth %.1f, intensity %.0f\n",
                seed, preset.altitude, preset.azimuth, float{ cvar::rt_sun_intensity } );
        return;
    }

    if( RT_UseDoomE3RealisticLights( primaryLevel ) )
    {
        const DoomE3HorizonLightPreset preset = RT_GetDoomE3HorizonLightPreset( seed );
        cvar::rt_sun           = true;
        cvar::rt_sun_a         = preset.altitude;
        cvar::rt_sun_b         = preset.azimuth;
        Printf( "RT Episode 3 horizon-light position %d: altitude %.1f, azimuth %.1f, intensity %.0f\n",
                seed, preset.altitude, preset.azimuth, float{ cvar::rt_sun_intensity } );
        return;
    }

    if( const int group = RT_GetDoom2RealisticGroup( primaryLevel ); group != 0 )
    {
        const Doom2ChapterLightPreset preset = RT_GetDoom2ChapterLightPreset( group, seed );
        cvar::rt_sun   = true;
        cvar::rt_sun_a = preset.altitude;
        cvar::rt_sun_b = preset.azimuth;
        Printf( "RT Doom II chapter %d light position %d: altitude %.1f, azimuth %.1f, intensity %.0f\n",
                group, seed, preset.altitude, preset.azimuth, float{ cvar::rt_sun_intensity } );
        return;
    }

    uint32_t randomState = 2166136261u;
    if( const char* sceneName = RT_GetMapName() )
    {
        for( const unsigned char* p = reinterpret_cast< const unsigned char* >( sceneName ); *p; ++p )
        {
            randomState ^= *p;
            randomState *= 16777619u;
        }
    }
    randomState ^= uint32_t( seed ) * 0x9E3779B9u;

    auto nextRandom = [ &randomState ]() {
        randomState ^= randomState << 13;
        randomState ^= randomState >> 17;
        randomState ^= randomState << 5;
        return float( randomState & 0x00FFFFFFu ) / float( 0x01000000u );
    };

    const float altitude = std::lerp( 8.f, 30.f, nextRandom() );
    const float azimuth  = nextRandom() * 360.f;
    const float intensity = std::lerp( 75.f, 500.f, nextRandom() );

    cvar::rt_sun           = true;
    cvar::rt_sun_a         = altitude;
    cvar::rt_sun_b         = azimuth;
    cvar::rt_sun_intensity = intensity;

    Printf( "RT sun position %d: altitude %.1f, azimuth %.1f, intensity %.0f\n",
            seed, altitude, azimuth, intensity );
}

CCMD( rt_autosun_next )
{
    cvar::rt_autosun_seed = int{ cvar::rt_autosun_seed } + 1;
    RT_ApplySunCycleAngle( int{ cvar::rt_autosun_seed } );
}

CCMD( rt_autosun_previous )
{
    cvar::rt_autosun_seed = int{ cvar::rt_autosun_seed } - 1;
    RT_ApplySunCycleAngle( int{ cvar::rt_autosun_seed } );
}

bool RT_ForceNoClassicMode()
{
    if( g_rt_cutscenename && g_rt_cutscenename[ 0 ] != '\0' )
    {
        return true;
    }
    if( g_rt_showfirststartscene )
    {
        return true;
    }
    return false;
}



constexpr auto RT_BIT( uint32_t b )
{
    return 1u << b;
}
enum rt_powerupflag_t
{
    RT_POWERUP_FLAG_BONUS_BIT          = RT_BIT( 1 ),
    RT_POWERUP_FLAG_BERSERK_BIT        = RT_BIT( 2 ),
    RT_POWERUP_FLAG_RADIATIONSUIT_BIT  = RT_BIT( 3 ),
    RT_POWERUP_FLAG_INVUNERABILITY_BIT = RT_BIT( 4 ),
    RT_POWERUP_FLAG_INVISIBILITY_BIT   = RT_BIT( 5 ),
    RT_POWERUP_FLAG_NIGHTVISION_BIT    = RT_BIT( 6 ),
    RT_POWERUP_FLAG_THERMALVISION_BIT  = RT_BIT( 7 ),
    RT_POWERUP_FLAG_FLASHLIGHT_BIT     = RT_BIT( 8 ),
};
uint32_t RT_CalcPowerupFlags();



constexpr float pi()
{
    return pi::pif();
}

constexpr float to_rad( float degrees )
{
    return degrees * ( pi() / 180.0f );
}

constexpr FVector3 gzvec3(const RgFloat3D &v)
{
    return { v.data[ 0 ], v.data[ 1 ], v.data[ 2 ] };
}

constexpr DVector3 gzvec3d(const RgFloat3D &v)
{
    return { v.data[ 0 ], v.data[ 1 ], v.data[ 2 ] };
}

template< typename T >
auto applygamma( T x ) = delete;
template<>
auto applygamma( float x )
{
    return std::clamp( x * x, 0.f, 1.f );
}
template<>
auto applygamma( uint8_t x )
{
    return static_cast< uint8_t >( applygamma( float( x ) / 255.f ) * 255.f );
}

auto rtcolor( const PalEntry& e ) -> RgColor4DPacked32
{
    return rt.rgUtilPackColorByte4D( e.r, e.g, e.b, e.a );
}

auto rtcolor( const FVector4PalEntry& e ) -> RgColor4DPacked32
{
    return rt.rgUtilPackColorFloat4D( e.r, e.g, e.b, e.a );
}

auto cvarcolor_to_rtcolor( const FColorCVarRef& cvarcolor ) -> RgColor4DPacked32
{
    uint32_t ba = *( cvarcolor );

    int r = RPART( ba );
    int g = GPART( ba );
    int b = BPART( ba );

    return rt.rgUtilPackColorByte4D( r, g, b, 255 );
}

float lightlevel_to_classic( bool isui, float lightlevel )
{
    if( isui )
    {
        return 1.0f;
    }

    if( lightlevel < 0.0f )
    {
        return 1.0f;
    }

    float lmin = std::max( float( cvar::rt_classic_llmin ), 0.0f );
    float lmax = std::min( float( cvar::rt_classic_llmax ), 1.0f );

    float lrange = std::max( lmax - lmin, 0.0f );
    if( lrange < 0.001f )
    {
        lmin   = 0.0f;
        lmax   = 1.0f;
        lrange = 1.0f;
    }

    float t01 = std::clamp( lightlevel, 0.0f, 1.0f );
    t01       = std::pow( t01, float( cvar::rt_classic_llpow ) );
    
    return lmin + t01 * lrange;
}

auto rtcolor_multiply( const FVector4PalEntry& e, const FVector4& b, bool forcealpha1 ) -> RgColor4DPacked32
{
    return rt.rgUtilPackColorFloat4D( e.r * b[ 0 ], //
                                      e.g * b[ 1 ],
                                      e.b * b[ 2 ],
                                      forcealpha1 ? 1.0f : e.a * b[ 3 ] );
}

auto rtcolor_bgr_alphagamma( const PalEntry& e ) -> RgColor4DPacked32
{
    return rt.rgUtilPackColorByte4D( e.b, e.g, e.r, applygamma( e.a ) );
}



class RTRenderState;

class RTFrameBuffer : public SystemBaseFrameBuffer
{
    using Super = SystemBaseFrameBuffer;

public:
    RTFrameBuffer( void* hMonitor, bool fullscreen );
    ~RTFrameBuffer() override;
    void InitializeState() override;
    void BeginFrame() override
    {
        SetViewportRects( nullptr );
        RT_BeginFrame();
        Super::BeginFrame();
    }
    void Update() override
    {
        this->Draw2D();
        twod->Clear();
        RT_DrawFrame();
        Super::Update();
    }
    void FirstEye() override;

    FRenderState*     RenderState() override;
    IVertexBuffer*    CreateVertexBuffer() override;
    IIndexBuffer*     CreateIndexBuffer() override;
    IDataBuffer*      CreateDataBuffer( int bindingpoint, bool ssbo, bool needsresize ) override;
    IHardwareTexture* CreateHardwareTexture( int numchannels ) override;

    void SetVSync( bool vsync ) override { m_vsync = vsync; }
    void SetTextureFilterMode() override {}
    void SetLevelMesh( hwrenderer::LevelMesh* mesh ) override {}

    void Draw2D() override;

public:
    void RT_MarkWasSky() { m_wassky = true; }

private:
    void RT_BeginFrame();
    void RT_DrawFrame();

private:
    RTRenderState* m_state{ nullptr };
    bool           m_vsync{ false };
    bool           m_wassky{ false };
};



class VectorAsBuffer : virtual public IBuffer
{
public:
    ~VectorAsBuffer() override = default;

    void SetSubData( size_t offset, size_t size, const void* data ) override
    {
        if( offset + size > m_buffer.size() )
        {
            m_buffer.resize( offset + size );
        }

        if( data )
        {
            memcpy( &m_buffer[ offset ], data, size );
        }

        buffersize = m_buffer.size();
        if( map )
        {
            map = m_buffer.data();
        }
    }
    void SetData( size_t size, const void* data, BufferUsageType type ) override
    {
        SetSubData( 0, size, data );
    }
    void* Lock( unsigned size ) override
    {
        SetSubData( 0, size, nullptr );
        return m_buffer.data();
    }
    void Unlock() override {}
    void Resize( size_t newsize ) override { m_buffer.resize( newsize ); }
    void Upload( size_t start, size_t size ) override {}
    void Map() override { map = m_buffer.data(); }
    void Unmap() override { map = nullptr; }
    void GPUDropSync() override {}
    void GPUWaitSync() override {}

protected:
    auto AccessBuffer() const { return std::span{ m_buffer }; }

private:
    std::vector< uint8_t > m_buffer;
};

class RTVertexBuffer
    : public IVertexBuffer
    , public VectorAsBuffer
{
    using Super            = VectorAsBuffer;
    using VertexTypeHolder = std::
        variant< std::monostate, FSkyVertex, FModelVertex, FFlatVertex, F2DDrawer::TwoDVertex >;

public:
    void SetFormat( int                           numBindingPoints,
                    int                           numAttributes,
                    size_t                        stride,
                    const FVertexBufferAttribute* attrs ) override
    {
        static_assert( sizeof( FSkyVertex ) != sizeof( FModelVertex ) );
        static_assert( sizeof( FSkyVertex ) != sizeof( FFlatVertex ) );
        static_assert( sizeof( FSkyVertex ) != sizeof( F2DDrawer::TwoDVertex ) );
        static_assert( sizeof( FModelVertex ) != sizeof( FFlatVertex ) );
        static_assert( sizeof( FModelVertex ) != sizeof( F2DDrawer::TwoDVertex ) );
        static_assert( sizeof( FFlatVertex ) != sizeof( F2DDrawer::TwoDVertex ) );

        if( numBindingPoints == 1 && numAttributes == 4 && stride == sizeof( FSkyVertex ) )
        {
            m_vertextype = FSkyVertex{};
        }
        else if( numBindingPoints == 2 && numAttributes == 8 && stride == sizeof( FModelVertex ) )
        {
            m_vertextype = FModelVertex{};
        }
        else if( numBindingPoints == 1 && numAttributes == 3 && stride == sizeof( FFlatVertex ) )
        {
            m_vertextype = FFlatVertex{};
        }
        else if( numBindingPoints == 1 && numAttributes == 3 &&
                 stride == sizeof( F2DDrawer::TwoDVertex ) )
        {
            m_vertextype = F2DDrawer::TwoDVertex{};
        }
        else
        {
            assert( 0 );
            m_vertextype = std::monostate{};
        }
        m_formatted.clear();
    }

    static void MakeFormatted( std::vector< RgPrimitiveVertex >& dst,
                               size_t                            targetCount,
                               std::span< const uint8_t >        srcbuf,
                               const VertexTypeHolder&           vertextype )
    {
        // TODO: mStreamData.uVertexColor for lightstyled?


        static auto gz_unpacknormal_x = []( uint32_t packedNormal ) -> float {
            int inx = ( packedNormal & 1023 );
            return float( inx ) / 512.0f;
        };
        static auto gz_unpacknormal_y = []( uint32_t packedNormal ) -> float {
            int iny = ( ( packedNormal >> 10 ) & 1023 );
            return float( iny ) / 512.0f;
        };
        static auto gz_unpacknormal_z = []( uint32_t packedNormal ) -> float {
            int inz = ( ( packedNormal >> 20 ) & 1023 );
            return float( inz ) / 512.0f;
        };

        static auto rg_packednormal_fallback = rt.rgUtilPackNormal( 0, 1, 0 );

        // make by type
        std::visit(
            [ & ]< typename T >( const T& ) {
                assert( srcbuf.size_bytes() % sizeof( T ) == 0 );

                dst.reserve( targetCount );
                for( size_t i = dst.size(); i < targetCount; i++ )
                {
                    static_assert( sizeof( decltype( srcbuf )::value_type ) == 1 );
                    const auto* ptr = &srcbuf[ i * sizeof( T ) ];

                    if constexpr( std::is_same_v< T, FSkyVertex > )
                    {
                        auto src = reinterpret_cast< const FSkyVertex* >( ptr );

                        dst.push_back( RgPrimitiveVertex{
                            .position     = { src->x * ONEGAMEUNIT_IN_METERS,
                                              src->y * ONEGAMEUNIT_IN_METERS,
                                              src->z * ONEGAMEUNIT_IN_METERS },
                            .normalPacked = rg_packednormal_fallback,
                            .texCoord     = { src->u, src->v },
                            .color        = rtcolor( src->color ),
                        } );
                    }
                    else if constexpr( std::is_same_v< T, FModelVertex > )
                    {
                        auto src = reinterpret_cast< const FModelVertex* >( ptr );

                        dst.push_back( RgPrimitiveVertex{
                            .position = { src->x * ONEGAMEUNIT_IN_METERS,
                                          src->y * ONEGAMEUNIT_IN_METERS,
                                          src->z * ONEGAMEUNIT_IN_METERS },
                            .normalPacked =
                                rt.rgUtilPackNormal( gz_unpacknormal_x( src->packedNormal ),
                                                     gz_unpacknormal_y( src->packedNormal ),
                                                     gz_unpacknormal_z( src->packedNormal ) ),
                            .texCoord = { src->u, src->v },
                            .color    = RG_PACKED_COLOR_WHITE,
                        } );
                    }
                    else if constexpr( std::is_same_v< T, FFlatVertex > )
                    {
                        auto src = reinterpret_cast< const FFlatVertex* >( ptr );

                        dst.push_back( RgPrimitiveVertex{
                            .position     = { src->x * ONEGAMEUNIT_IN_METERS,
                                              src->y * ONEGAMEUNIT_IN_METERS,
                                              src->z * ONEGAMEUNIT_IN_METERS },
                            .normalPacked = rg_packednormal_fallback,
                            .texCoord     = { src->u, src->v },
                            .color        = RG_PACKED_COLOR_WHITE,
                        } );
                    }
                    else if constexpr( std::is_same_v< T, F2DDrawer::TwoDVertex > )
                    {
                        auto src = reinterpret_cast< const F2DDrawer::TwoDVertex* >( ptr );

                        dst.push_back( RgPrimitiveVertex{
                            .position     = { src->x, src->y, src->z },
                            .normalPacked = rg_packednormal_fallback,
                            .texCoord     = { src->u, src->v },
                            .color        = rtcolor_bgr_alphagamma( src->color0 ),
                        } );
                    }
                    else
                    {
                        assert( 0 );
                    }
                }
            },
            vertextype );
    }

    auto AccessFormatted( uint32_t first, uint32_t count ) -> std::span< const RgPrimitiveVertex >
    {
        if( std::holds_alternative< std::monostate >( m_vertextype ) )
        {
            return {};
        }

        if( first + count > m_formatted.size() )
        {
            MakeFormatted( m_formatted, first + count, AccessBuffer(), m_vertextype );
        }

        assert( first + count <= m_formatted.size() );

        return std::span{
            &m_formatted[ first ],
            count,
        };
    }

    void SetData( size_t size, const void* data, BufferUsageType type ) override
    {
        m_formatted.clear();
        Super::SetData( size, data, type );
    }

    void SetSubData( size_t offset, size_t size, const void* data ) override
    {
        m_formatted.clear();
        Super::SetSubData( offset, size, data );
    }

    void Unmap() override
    {
        m_formatted.clear();
        Super::Unmap();
    }

    bool IsSky() const { return std::holds_alternative< FSkyVertex >( m_vertextype ); }
    bool IsUI() const { return std::holds_alternative< F2DDrawer::TwoDVertex >( m_vertextype ); }

private:
    VertexTypeHolder m_vertextype;

    std::vector< RgPrimitiveVertex > m_formatted;
};

class RTIndexBuffer
    : public IIndexBuffer
    , public VectorAsBuffer
{
    using IndexType = uint32_t;

public:
    auto AccessFormatted( uint32_t first, uint32_t count )
    {
        const auto rawbuf = AccessBuffer();
        // loose type check
        assert( rawbuf.size_bytes() % sizeof( IndexType ) == 0 );
        // alignment
        assert( uint64_t( rawbuf.data() ) % sizeof( IndexType ) == 0 );
        // overflow
        assert( sizeof( IndexType ) * ( first + count ) <= rawbuf.size_bytes() );

        return std::span{
            reinterpret_cast< const IndexType* >( rawbuf.data() ) + first,
            count,
        };
    }

    static auto CalcFirstVertexAndVertexCount( std::span< const IndexType > indices )
    {
        uint32_t imin = std::numeric_limits< uint32_t >::max();
        uint32_t imax = std::numeric_limits< uint32_t >::lowest();
        for( const auto& i : indices )
        {
            imin = std::min( imin, i );
            imax = std::max( imax, i );
        }
        return std::pair{
            imax > imin ? imin : 0,
            imax > imin ? imax - imin + 1 : 0,
        };
    }

    auto MakeWithNewFirstIndex( std::span< const IndexType > indices, IndexType newFirst )
    {
        m_cache.clear();
        m_cache.reserve( indices.size() );

        for( const auto& i : indices )
        {
            assert( i >= newFirst );
            m_cache.push_back( i - newFirst );
        }

        return m_cache;
    }

private:
    std::vector< IndexType > m_cache;
};



enum class LtpLiquidKind
{
    None,
    Water,
    Blood,
    Slime,
    Toxic,
    Lava,
    WaterFall,
    BloodFall,
    SlimeFall,
    ToxicFall,
    LavaFall,
};

static bool LtpNameStartsWith( const char* name, const char* prefix )
{
    return name && strnicmp( name, prefix, strlen( prefix ) ) == 0;
}

static LtpLiquidKind GetLtpLiquidKind( const char* name )
{
    // Check falls before flats: the LP* editor names share their flat prefixes.
    if( LtpNameStartsWith( name, "LWFALL" ) || LtpNameStartsWith( name, "WFALL" ) ||
        LtpNameStartsWith( name, "LPWATERF" ) )
        return LtpLiquidKind::WaterFall;
    if( LtpNameStartsWith( name, "LBFALL" ) || LtpNameStartsWith( name, "BFALL" ) ||
        LtpNameStartsWith( name, "LPBLOODF" ) )
        return LtpLiquidKind::BloodFall;
    if( LtpNameStartsWith( name, "LSFALL" ) || LtpNameStartsWith( name, "LPSLIMEF" ) )
        return LtpLiquidKind::SlimeFall;
    if( LtpNameStartsWith( name, "LTFALL" ) || LtpNameStartsWith( name, "SFALL" ) ||
        LtpNameStartsWith( name, "LPNUKEF" ) )
        return LtpLiquidKind::ToxicFall;
    if( LtpNameStartsWith( name, "LLFALL" ) || LtpNameStartsWith( name, "LPLAVAF" ) )
        return LtpLiquidKind::LavaFall;

    if( LtpNameStartsWith( name, "LWFLAT" ) || LtpNameStartsWith( name, "FWATER" ) ||
        LtpNameStartsWith( name, "LPWATER" ) )
        return LtpLiquidKind::Water;
    if( LtpNameStartsWith( name, "LBFLAT" ) || LtpNameStartsWith( name, "BLOOD" ) ||
        LtpNameStartsWith( name, "LPBLOOD" ) )
        return LtpLiquidKind::Blood;
    if( LtpNameStartsWith( name, "LSFLAT" ) || LtpNameStartsWith( name, "SLIME" ) ||
        LtpNameStartsWith( name, "LPSLIME" ) )
        return LtpLiquidKind::Slime;
    if( LtpNameStartsWith( name, "LTFLAT" ) || LtpNameStartsWith( name, "NUKAGE" ) ||
        LtpNameStartsWith( name, "LPNUKE" ) )
        return LtpLiquidKind::Toxic;
    if( LtpNameStartsWith( name, "LLFLAT" ) || LtpNameStartsWith( name, "LAVA" ) ||
        LtpNameStartsWith( name, "LPLAVA" ) )
        return LtpLiquidKind::Lava;
    return LtpLiquidKind::None;
}

static bool IsLtpFall( LtpLiquidKind kind )
{
    return kind >= LtpLiquidKind::WaterFall;
}

class RTHardwareTexture : public IHardwareTexture
{
public:
    // Empty, as it's only used for software renderer
    uint32_t CreateTexture( uint8_t*, int, int, int, bool, const char* ) override { return 0; }
    void     AllocateBuffer( int, int, int ) override {}
    uint8_t* MapBuffer() override { return nullptr; }

    void CreateIfWasnt( FGameTexture&       src,
                        int                 clampmode,
                        int                 translation,
                        int                 flags,
                        const FRenderStyle& renderStyle )
    {
        auto rtclamp_x = []( int clampmode ) {
            switch( clampmode )
            {
                case CLAMP_X:
                case CLAMP_XY:
                case CLAMP_XY_NOMIP:
                case CLAMP_NOFILTER_X:
                case CLAMP_NOFILTER_XY:
                case CLAMP_CAMTEX: return RG_SAMPLER_ADDRESS_MODE_CLAMP;
                default: return RG_SAMPLER_ADDRESS_MODE_REPEAT;
            }
        };
        auto rtclamp_y = []( int clampmode ) {
            switch( clampmode )
            {
                case CLAMP_Y:
                case CLAMP_XY:
                case CLAMP_XY_NOMIP:
                case CLAMP_NOFILTER_Y:
                case CLAMP_NOFILTER_XY:
                case CLAMP_CAMTEX: return RG_SAMPLER_ADDRESS_MODE_CLAMP;
                default: return RG_SAMPLER_ADDRESS_MODE_REPEAT;
            }
        };
        auto desaturateIfNeed = []( FTextureBuffer& data, int flags, const char* lumpname ) {
            // special case for the SmallFont...
            const bool isSTCFNFont = !( flags & CTF_Indexed ) && lumpname &&
                                     strlen( lumpname ) == 8 &&
                                     strncmp( lumpname, "STCFN", 5 ) == 0;
            if( isSTCFNFont )
            {
                for( int i = 0; i < data.mWidth; i++ )
                {
                    for( int j = 0; j < data.mHeight; j++ )
                    {
                        uint8_t* pix =
                            &data.mBuffer[ 4 *
                                           ( i * static_cast< uint64_t >( data.mHeight ) + j ) ];
                        const uint8_t gray = std::max( pix[ 0 ], std::max( pix[ 1 ], pix[ 2 ] ) );
                        pix[ 0 ] = pix[ 1 ] = pix[ 2 ] = gray;
                    }
                }
            }
        };
        auto calculateAlphaIfNeed = []( FTextureBuffer& data, bool redIsAlpha ) {
            if( redIsAlpha )
            {
                for( int i = 0; i < data.mWidth; i++ )
                {
                    for( int j = 0; j < data.mHeight; j++ )
                    {
                        uint8_t* pix =
                            &data.mBuffer[ 4 *
                                           ( i * static_cast< uint64_t >( data.mHeight ) + j ) ];

                        // alpha = red
                        pix[ 3 ] = pix[ 0 ];
                    }
                }
            }
        };

        if( m_name.empty() )
        {
            m_name = MakeTextureName( src );
        }

        if( m_name.empty() || !src.GetTexture() )
        {
            assert( 0 );
            return;
        }

        // LTP normally animates its liquids in a custom raster fragment shader. The RT
        // renderer cannot execute that shader, so refresh the small source material at render
        // rate and deform its pixels instead. This preserves LTP's actual artwork and avoids
        // visibly sliding the whole texture across the floor.
        // RTGL texture replacement is deliberately static. Liquid motion is supplied by
        // native RTGL material layers and animated UVs in InternalDraw; deleting and
        // re-uploading textures during play causes severe frame-time stalls.
        const bool animateLtp = false;
        // Toxic composition is considerably more expensive than the small fallback warp.
        // It is precomputed into an RTGL-resident frame set below; only the inexpensive
        // fallback liquid path still replaces a texture while the game is running.
        const bool     toxicLtp       = IsLtpToxicFlat( m_name.c_str() );
        const uint64_t animationFrame =
            screen ? screen->FrameTime / ( toxicLtp ? 50 : 33 ) : 0;

        if( m_created &&
            ( !animateLtp || toxicLtp || animationFrame == m_lastLtpAnimationFrame ) )
        {
            return;
        }

        auto texbuffer = src.GetTexture()->CreateTexBuffer( translation, flags | CTF_ProcessData );
        desaturateIfNeed( texbuffer, flags, fileSystem.GetFileShortName( src.GetSourceLump() ) );
        calculateAlphaIfNeed( texbuffer, renderStyle.Flags & STYLEF_RedIsAlpha );

        if( texbuffer.mWidth <= 0 || texbuffer.mHeight <= 0 )
        {
            assert( 0 );
            return;
        }

        if( animateLtp && toxicLtp )
        {
            // Replacing a texture in RTGL every few frames stalls the path tracer. Prepare a
            // compact animation once instead, upload every frame under a stable name, and let
            // GetRTName() switch between already-resident textures during play.
            constexpr uint32_t ToxicFrameCount = 24;
            m_ltpFrameNames.clear();
            m_ltpFrameNames.reserve( ToxicFrameCount );

            for( uint32_t frameIndex = 0; frameIndex < ToxicFrameCount; frameIndex++ )
            {
                auto frameBuffer = src.GetTexture()->CreateTexBuffer(
                    translation, flags | CTF_ProcessData );
                desaturateIfNeed( frameBuffer, flags,
                                  fileSystem.GetFileShortName( src.GetSourceLump() ) );
                calculateAlphaIfNeed( frameBuffer,
                                      renderStyle.Flags & STYLEF_RedIsAlpha );

                const float phase = static_cast< float >( frameIndex ) /
                                    static_cast< float >( ToxicFrameCount ) * 6.28318530718f;
                AnimateLtpToxic( frameBuffer, phase );

                m_ltpFrameNames.push_back( m_name + "_rtltp_" +
                                           std::to_string( frameIndex ) );

                auto frameDetails = RgOriginalTextureDetailsEXT{
                    .sType  = RG_STRUCTURE_TYPE_ORIGINAL_TEXTURE_DETAILS_EXT,
                    .pNext  = nullptr,
                    .flags  = 0u,
                    .format = RG_FORMAT_B8G8R8A8_SRGB,
                };
                auto frameInfo = RgOriginalTextureInfo{
                    .sType        = RG_STRUCTURE_TYPE_ORIGINAL_TEXTURE_INFO,
                    .pNext        = &frameDetails,
                    .pTextureName = m_ltpFrameNames.back().c_str(),
                    .pPixels      = frameBuffer.mBuffer,
                    .size         = { static_cast< uint32_t >( frameBuffer.mWidth ),
                                      static_cast< uint32_t >( frameBuffer.mHeight ) },
                    .filter       = RG_SAMPLER_FILTER_AUTO,
                    .addressModeU = RG_SAMPLER_ADDRESS_MODE_REPEAT,
                    .addressModeV = RG_SAMPLER_ADDRESS_MODE_REPEAT,
                };
                RgResult frameResult = rt.rgProvideOriginalTexture( &frameInfo );
                RG_CHECK( frameResult );
            }

            m_created = true;
            return;
        }
        else if( animateLtp )
        {
            AnimateLtpLiquid( texbuffer, static_cast< float >( screen->FrameTime ) * 0.001f );
        }

        const bool exportseparately = m_name.starts_with( "vx_" );

        auto details = RgOriginalTextureDetailsEXT{
            .sType  = RG_STRUCTURE_TYPE_ORIGINAL_TEXTURE_DETAILS_EXT,
            .pNext  = nullptr,
            .flags  = exportseparately ? RG_ORIGINAL_TEXTURE_INFO_FORCE_EXPORT_AS_EXTERNAL : 0u,
            .format = flags & CTF_Indexed ? RG_FORMAT_R8_SRGB : RG_FORMAT_B8G8R8A8_SRGB,
        };

        auto info = RgOriginalTextureInfo{
            .sType        = RG_STRUCTURE_TYPE_ORIGINAL_TEXTURE_INFO,
            .pNext        = &details,
            .pTextureName = m_name.c_str(),
            .pPixels      = texbuffer.mBuffer,
            .size         = { static_cast< uint32_t >( texbuffer.mWidth ),
                              static_cast< uint32_t >( texbuffer.mHeight ) },
            .filter       = RG_SAMPLER_FILTER_AUTO,
            .addressModeU = RG_SAMPLER_ADDRESS_MODE_REPEAT, //  rtclamp_x( clampmode ),
            .addressModeV = RG_SAMPLER_ADDRESS_MODE_REPEAT, //  rtclamp_y( clampmode ),
        };

        if( m_created )
        {
            RgResult r = rt.rgMarkOriginalTextureAsDeleted( m_name.c_str() );
            RG_CHECK( r );
        }

        RgResult r = rt.rgProvideOriginalTexture( &info );
        RG_CHECK( r );

        m_created = true;
        if( animateLtp )
        {
            m_lastLtpAnimationFrame = animationFrame;
        }
    }

    ~RTHardwareTexture() override
    {
// HACKHACK: TODO: why this is being called only on Release? (and destroying actually used textures)
#if 0
        RgResult r = rt.rgMarkOriginalTextureAsDeleted( m_name.c_str() );
        RG_CHECK( r );
#endif
    }

    auto GetRTName() const -> const char*
    {
        if( m_created && !m_ltpFrameNames.empty() )
        {
            const uint64_t frame = screen ? screen->FrameTime / 50 : 0;
            return m_ltpFrameNames[ frame % m_ltpFrameNames.size() ].c_str();
        }
        return m_created && !m_name.empty() ? m_name.c_str() : nullptr;
    }

    static void CreateLtpMaterialTexturesIfNeeded()
    {
        static bool created = false;
        if( created )
        {
            return;
        }

        struct Source
        {
            const char* gzName;
            const char* rtName;
            float       brightness;
        };
        constexpr Source sources[] = {
            { "textures/Materials/diffuse/Waterflat.png", "rt_ltp_water_diffuse", 0.72f },
            { "textures/Materials/Specular/Waterspec.png", "rt_ltp_water_spec", 0.22f },
            { "textures/Materials/diffuse/Bloodflat.png", "rt_ltp_blood_diffuse", 0.62f },
            { "textures/Materials/Specular/Bloodspec.png", "rt_ltp_blood_spec", 0.18f },
            { "textures/Materials/diffuse/Slimeflat.png", "rt_ltp_slime_diffuse", 0.62f },
            { "textures/Materials/Specular/Slimespec.png", "rt_ltp_slime_spec", 0.18f },
            { "textures/Materials/diffuse/toxic.png", "rt_ltp_toxic_diffuse", 1.0f },
            { "textures/Materials/Diffuselayer/toxiclayer.png", "rt_ltp_toxic_layer", 1.0f },
            { "textures/Materials/Diffuselayermask/Toxicmask.png", "rt_ltp_toxic_mask", 1.0f },
            { "textures/Materials/diffuse/lava.png", "rt_ltp_lava_diffuse", 1.0f },
            { "textures/Materials/Diffuselayer/lavaslag.png", "rt_ltp_lava_slag", 1.0f },
            { "textures/Materials/Diffuselayermask/lavarockmask.png", "rt_ltp_lava_mask", 1.0f },
            { "textures/Materials/Parallax/WaterD.png", "rt_ltp_liquid_displacement", 1.0f },
            { "textures/Materials/Diffuselayer/LFfoam.png", "rt_ltp_fall_foam", 0.30f },
            { "textures/Materials/Diffuselayermask/Toxicfallmask.png", "rt_ltp_toxic_fall_mask", 0.55f },
            { "textures/Materials/Diffuselayermask/lavafallrockmask.png", "rt_ltp_lava_fall_mask", 0.48f },
        };

        for( const Source& source : sources )
        {
            LtpImage image = LoadLtpImage( source.gzName );
            if( !image )
            {
                continue;
            }

            for( size_t i = 0; i < image.pixels.size(); i += 4 )
            {
                image.pixels[ i + 0 ] = static_cast< uint8_t >( image.pixels[ i + 0 ] * source.brightness );
                image.pixels[ i + 1 ] = static_cast< uint8_t >( image.pixels[ i + 1 ] * source.brightness );
                image.pixels[ i + 2 ] = static_cast< uint8_t >( image.pixels[ i + 2 ] * source.brightness );
            }

            auto details = RgOriginalTextureDetailsEXT{
                .sType  = RG_STRUCTURE_TYPE_ORIGINAL_TEXTURE_DETAILS_EXT,
                .pNext  = nullptr,
                .flags  = 0u,
                .format = RG_FORMAT_B8G8R8A8_SRGB,
            };
            auto info = RgOriginalTextureInfo{
                .sType        = RG_STRUCTURE_TYPE_ORIGINAL_TEXTURE_INFO,
                .pNext        = &details,
                .pTextureName = source.rtName,
                .pPixels      = image.pixels.data(),
                .size         = { static_cast< uint32_t >( image.width ),
                                  static_cast< uint32_t >( image.height ) },
                .filter       = RG_SAMPLER_FILTER_LINEAR,
                .addressModeU = RG_SAMPLER_ADDRESS_MODE_REPEAT,
                .addressModeV = RG_SAMPLER_ADDRESS_MODE_REPEAT,
            };
            RgResult result = rt.rgProvideOriginalTexture( &info );
            RG_CHECK( result );
        }

        // Lavaflat.fp uses lavarockmask to put the fine lavaslag texture over large
        // dark crust islands, leaving lava.png visible only through the cracks. RTGL
        // texture layers cannot multiply one layer by another, so bake that static
        // material relationship once at startup. Animation remains GPU-side through UVs.
        const LtpImage lava = LoadLtpImage( "textures/Materials/diffuse/lava.png" );
        const LtpImage slag = LoadLtpImage( "textures/Materials/Diffuselayer/lavaslag.png" );
        const LtpImage mask = LoadLtpImage( "textures/Materials/Diffuselayermask/lavarockmask.png" );
        if( lava && slag && mask )
        {
            auto sample = []( const LtpImage& image, float u, float v, int channel ) {
                int x = static_cast< int >( std::floor( u * image.width ) ) % image.width;
                int y = static_cast< int >( std::floor( v * image.height ) ) % image.height;
                if( x < 0 ) x += image.width;
                if( y < 0 ) y += image.height;
                return image.pixels[ ( static_cast< size_t >( y ) * image.width + x ) * 4 +
                                     channel ];
            };

            LtpImage composite;
            LtpImage cracks;
            composite.width = cracks.width = mask.width;
            composite.height = cracks.height = mask.height;
            composite.pixels.resize( static_cast< size_t >( mask.width ) * mask.height * 4 );
            cracks.pixels.resize( composite.pixels.size() );
            for( int y = 0; y < mask.height; y++ )
            {
                for( int x = 0; x < mask.width; x++ )
                {
                    const float u = static_cast< float >( x ) / mask.width;
                    const float v = static_cast< float >( y ) / mask.height;
                    const float maskLum = ( sample( mask, u, v, 0 ) + sample( mask, u, v, 1 ) +
                                            sample( mask, u, v, 2 ) ) /
                                          ( 3.0f * 255.0f );
                    const float rock = std::clamp( ( maskLum - 0.045f ) * 1.42f, 0.0f, 1.0f );
                    const size_t dst = ( static_cast< size_t >( y ) * mask.width + x ) * 4;
                    for( int c = 0; c < 3; c++ )
                    {
                        const float molten = sample( lava, u * 5.33f, v * 5.33f, c ) * 0.70f;
                        const float tint = c == 2 ? 22.0f : c == 1 ? 4.0f : 2.0f;
                        const float crust = sample( slag, u * 20.0f, v * 20.0f, c ) *
                                                ( c == 2 ? 0.78f : 0.58f ) +
                                            tint;
                        composite.pixels[ dst + c ] = static_cast< uint8_t >(
                            std::clamp( molten * ( 1.0f - rock ) + crust * rock, 0.0f, 255.0f ) );
                        cracks.pixels[ dst + c ] = static_cast< uint8_t >(
                            std::clamp( molten * ( 1.0f - rock ) * 0.32f, 0.0f, 255.0f ) );
                    }
                    composite.pixels[ dst + 3 ] = cracks.pixels[ dst + 3 ] = 255;
                }
            }

            auto provide = []( const char* name, const LtpImage& image ) {
                auto details = RgOriginalTextureDetailsEXT{
                    .sType = RG_STRUCTURE_TYPE_ORIGINAL_TEXTURE_DETAILS_EXT,
                    .pNext = nullptr,
                    .flags = 0u,
                    .format = RG_FORMAT_B8G8R8A8_SRGB,
                };
                auto info = RgOriginalTextureInfo{
                    .sType = RG_STRUCTURE_TYPE_ORIGINAL_TEXTURE_INFO,
                    .pNext = &details,
                    .pTextureName = name,
                    .pPixels = image.pixels.data(),
                    .size = { static_cast< uint32_t >( image.width ),
                              static_cast< uint32_t >( image.height ) },
                    .filter = RG_SAMPLER_FILTER_LINEAR,
                    .addressModeU = RG_SAMPLER_ADDRESS_MODE_REPEAT,
                    .addressModeV = RG_SAMPLER_ADDRESS_MODE_REPEAT,
                };
                RgResult result = rt.rgProvideOriginalTexture( &info );
                RG_CHECK( result );
            };
            provide( "rt_ltp_lava_composite", composite );
            provide( "rt_ltp_lava_cracks", cracks );
        }

        // Toxicflat.fp uses Toxicmask at 0.15 of the editor-flat UV to separate a
        // broad, nearly black under-surface from the green top material. The shader
        // then samples toxic.png at 0.25/0.11 and toxiclayer.png at 2.5. Preserve
        // those exact relative scales in one masked RT texture instead of blending
        // the three source images over the entire primitive (which makes a flat,
        // uniformly green sheet in RTGL).
        const LtpImage toxic = LoadLtpImage( "textures/Materials/diffuse/toxic.png" );
        const LtpImage toxicLayer =
            LoadLtpImage( "textures/Materials/Diffuselayer/toxiclayer.png" );
        const LtpImage toxicMask =
            LoadLtpImage( "textures/Materials/Diffuselayermask/Toxicmask.png" );
        if( toxic && toxicLayer && toxicMask )
        {
            auto sample = []( const LtpImage& image, float u, float v, int channel ) {
                int x = static_cast< int >( std::floor( u * image.width ) ) % image.width;
                int y = static_cast< int >( std::floor( v * image.height ) ) % image.height;
                if( x < 0 ) x += image.width;
                if( y < 0 ) y += image.height;
                return image.pixels[ ( static_cast< size_t >( y ) * image.width + x ) * 4 +
                                     channel ];
            };

            LtpImage composite;
            LtpImage motion;
            composite.width = motion.width = toxicMask.width;
            composite.height = motion.height = toxicMask.height;
            composite.pixels.resize(
                static_cast< size_t >( toxicMask.width ) * toxicMask.height * 4 );
            motion.pixels.resize( composite.pixels.size() );
            for( int y = 0; y < toxicMask.height; y++ )
            {
                for( int x = 0; x < toxicMask.width; x++ )
                {
                    const float u = static_cast< float >( x ) / toxicMask.width;
                    const float v = static_cast< float >( y ) / toxicMask.height;
                    const float maskLum =
                        ( sample( toxicMask, u, v, 0 ) + sample( toxicMask, u, v, 1 ) +
                          sample( toxicMask, u, v, 2 ) ) /
                        ( 3.0f * 255.0f );
                    // Toxicflat.fp treats even the soft grey rim as top material.
                    const float top = std::clamp( ( maskLum - 0.025f ) * 7.0f, 0.0f, 1.0f );
                    const size_t dst =
                        ( static_cast< size_t >( y ) * toxicMask.width + x ) * 4;
                    for( int c = 0; c < 3; c++ )
                    {
                        // Shader scales relative to its 0.15 mask scale:
                        // 0.25 / 0.15, 0.11 / 0.15, and 2.5 / 0.15.
                        const float broadA = sample( toxic, u * 1.6667f, v * 1.6667f, c );
                        const float broadB = sample( toxic, u * 0.7333f, v * 0.7333f, c );
                        const float under = std::clamp( broadA * 0.72f + broadB * 0.48f,
                                                        0.0f, 255.0f );
                        const float fine =
                            sample( toxicLayer, u * 16.6667f, v * 16.6667f, c );

                        // Black mask pockets retain the dark toxic fissures; white/grey
                        // areas receive the green top layer and under-glow from the shader.
                        const float bottom = under * ( c == 1 ? 0.68f : 0.48f );
                        const float island = fine * 0.48f + under * 0.82f;
                        composite.pixels[ dst + c ] = static_cast< uint8_t >(
                            std::clamp( bottom * ( 1.0f - top ) + island * top,
                                        0.0f, 255.0f ) );
                        motion.pixels[ dst + c ] = static_cast< uint8_t >(
                            std::clamp( under * top * ( c == 1 ? 0.34f : 0.18f ),
                                        0.0f, 255.0f ) );
                    }
                    composite.pixels[ dst + 3 ] = motion.pixels[ dst + 3 ] = 255;
                }
            }

            auto provide = []( const char* name, const LtpImage& image ) {
                auto details = RgOriginalTextureDetailsEXT{
                    .sType = RG_STRUCTURE_TYPE_ORIGINAL_TEXTURE_DETAILS_EXT,
                    .pNext = nullptr,
                    .flags = 0u,
                    .format = RG_FORMAT_B8G8R8A8_SRGB,
                };
                auto info = RgOriginalTextureInfo{
                    .sType = RG_STRUCTURE_TYPE_ORIGINAL_TEXTURE_INFO,
                    .pNext = &details,
                    .pTextureName = name,
                    .pPixels = image.pixels.data(),
                    .size = { static_cast< uint32_t >( image.width ),
                              static_cast< uint32_t >( image.height ) },
                    .filter = RG_SAMPLER_FILTER_LINEAR,
                    .addressModeU = RG_SAMPLER_ADDRESS_MODE_REPEAT,
                    .addressModeV = RG_SAMPLER_ADDRESS_MODE_REPEAT,
                };
                RgResult result = rt.rgProvideOriginalTexture( &info );
                RG_CHECK( result );
            };
            provide( "rt_ltp_toxic_composite", composite );
            provide( "rt_ltp_toxic_motion", motion );
        }

        // RTGL does not execute GZDoom's custom ProcessMaterial fragment shaders.  Feeding
        // Toxicflat/Lavaflat to it as three independent texture layers exposes the mask as
        // grey or pitch-black cut-outs.  Instead, evaluate the complete LTP material into a
        // small looping frame set once.  Runtime animation is then only a texture-name
        // selection: no texture uploads, shader recompiles, or per-frame CPU work.
        constexpr uint32_t FrameCount = 64;
        auto provideFrames = []( bool lavaMaterial ) {
            auto& names = GetLtpMaterialFrameNames( lavaMaterial );
            names.clear();
            names.reserve( FrameCount );

            for( uint32_t frameIndex = 0; frameIndex < FrameCount; frameIndex++ )
            {
                FTextureBuffer frame;
                const float phase = static_cast< float >( frameIndex ) /
                                    static_cast< float >( FrameCount ) * 6.28318530718f;
                if( lavaMaterial )
                {
                    AnimateLtpLava( frame, phase );
                }
                else
                {
                    AnimateLtpToxic( frame, phase );
                }

                if( !frame.mBuffer || frame.mWidth <= 0 || frame.mHeight <= 0 )
                {
                    continue;
                }

                names.push_back( std::string( lavaMaterial ? "rt_ltp_lava_frame_"
                                                            : "rt_ltp_toxic_frame_" ) +
                                 std::to_string( frameIndex ) );
                auto details = RgOriginalTextureDetailsEXT{
                    .sType  = RG_STRUCTURE_TYPE_ORIGINAL_TEXTURE_DETAILS_EXT,
                    .pNext  = nullptr,
                    .flags  = 0u,
                    .format = RG_FORMAT_B8G8R8A8_SRGB,
                };
                auto info = RgOriginalTextureInfo{
                    .sType        = RG_STRUCTURE_TYPE_ORIGINAL_TEXTURE_INFO,
                    .pNext        = &details,
                    .pTextureName = names.back().c_str(),
                    .pPixels      = frame.mBuffer,
                    .size         = { static_cast< uint32_t >( frame.mWidth ),
                                      static_cast< uint32_t >( frame.mHeight ) },
                    .filter       = RG_SAMPLER_FILTER_LINEAR,
                    .addressModeU = RG_SAMPLER_ADDRESS_MODE_REPEAT,
                    .addressModeV = RG_SAMPLER_ADDRESS_MODE_REPEAT,
                };
                RgResult result = rt.rgProvideOriginalTexture( &info );
                RG_CHECK( result );
            }
        };

        provideFrames( false );
        provideFrames( true );
        created = true;
    }

    static const char* GetLtpMaterialFrameName( bool lavaMaterial )
    {
        const auto& names = GetLtpMaterialFrameNames( lavaMaterial );
        if( names.empty() )
        {
            return lavaMaterial ? "rt_ltp_lava_diffuse" : "rt_ltp_toxic_diffuse";
        }

        // A full cycle lasts 12.8 seconds.  This is deliberately slower than stock LTP,
        // while 200 ms between 64 closely-spaced frames keeps the motion calm and fluid.
        const uint64_t frame = screen ? screen->FrameTime / 200 : 0;
        return names[ frame % names.size() ].c_str();
    }

private:
    struct LtpImage
    {
        std::vector< uint8_t > pixels{};
        int                    width{};
        int                    height{};

        explicit operator bool() const
        {
            return !pixels.empty() && width > 0 && height > 0;
        }
    };

    static std::vector< std::string >& GetLtpMaterialFrameNames( bool lavaMaterial )
    {
        static std::vector< std::string > toxicFrames;
        static std::vector< std::string > lavaFrames;
        return lavaMaterial ? lavaFrames : toxicFrames;
    }

    static bool IsLtpToxicFlat( const char* name )
    {
        return name && ( strnicmp( name, "LTFLAT", 6 ) == 0 ||
                         strnicmp( name, "NUKAGE", 6 ) == 0 );
    }

    static bool IsLtpLiquidTexture( const char* name )
    {
        if( !name )
        {
            return false;
        }

        return strnicmp( name, "LWFLAT", 6 ) == 0 || strnicmp( name, "LBFLAT", 6 ) == 0 ||
               strnicmp( name, "LSFLAT", 6 ) == 0 || strnicmp( name, "LTFLAT", 6 ) == 0 ||
               strnicmp( name, "LLFLAT", 6 ) == 0 || strnicmp( name, "LWFALL", 6 ) == 0 ||
               strnicmp( name, "LBFALL", 6 ) == 0 || strnicmp( name, "LSFALL", 6 ) == 0 ||
               strnicmp( name, "LTFALL", 6 ) == 0 || strnicmp( name, "LLFALL", 6 ) == 0 ||
               strnicmp( name, "FWATER", 6 ) == 0 || strnicmp( name, "BLOOD", 5 ) == 0 ||
               strnicmp( name, "SLIME", 5 ) == 0 || strnicmp( name, "NUKAGE", 6 ) == 0 ||
               strnicmp( name, "LAVA", 4 ) == 0 || strnicmp( name, "WFALL", 5 ) == 0 ||
               strnicmp( name, "BFALL", 5 ) == 0 || strnicmp( name, "SFALL", 5 ) == 0;
    }

    static LtpImage LoadLtpImage( const char* name )
    {
        const FTextureID id = TexMan.CheckForTexture( name, ETextureType::Any,
                                                       FTextureManager::TEXMAN_TryAny );
        FGameTexture* gameTexture = id.Exists() ? TexMan.GetGameTexture( id, false ) : nullptr;
        FTexture*     texture     = gameTexture ? gameTexture->GetTexture() : nullptr;
        if( !texture )
        {
            return {};
        }

        auto buffer = texture->CreateTexBuffer( 0, CTF_ProcessData );
        if( !buffer.mBuffer || buffer.mWidth <= 0 || buffer.mHeight <= 0 )
        {
            return {};
        }

        LtpImage result;
        result.width  = buffer.mWidth;
        result.height = buffer.mHeight;
        result.pixels.assign( buffer.mBuffer,
                              buffer.mBuffer + static_cast< size_t >( result.width ) *
                                                   result.height * 4 );
        return result;
    }

    static void AnimateLtpLava( FTextureBuffer& data, float phase )
    {
        static const LtpImage diffuse =
            LoadLtpImage( "textures/Materials/diffuse/lava.png" );
        static const LtpImage layer =
            LoadLtpImage( "textures/Materials/Diffuselayer/lavaslag.png" );
        static const LtpImage mask =
            LoadLtpImage( "textures/Materials/Diffuselayermask/lavarockmask.png" );
        static const LtpImage distortion =
            LoadLtpImage( "textures/Materials/normal/LavalayerN.png" );

        if( !diffuse || !layer || !mask || !distortion )
        {
            return;
        }

        constexpr int OutputSize = 256;
        auto wrap01 = []( float value ) { return value - std::floor( value ); };
        auto sample = [ & ]( const LtpImage& image, float u, float v, int channel ) {
            const float sx = wrap01( u ) * image.width;
            const float sy = wrap01( v ) * image.height;
            const int x0 = static_cast< int >( std::floor( sx ) ) % image.width;
            const int y0 = static_cast< int >( std::floor( sy ) ) % image.height;
            const int x1 = ( x0 + 1 ) % image.width;
            const int y1 = ( y0 + 1 ) % image.height;
            const float fx = sx - std::floor( sx );
            const float fy = sy - std::floor( sy );
            auto at = [ & ]( int x, int y ) {
                return image.pixels[ ( static_cast< size_t >( y ) * image.width + x ) * 4 +
                                     channel ] /
                       255.0f;
            };
            const float a = at( x0, y0 ) * ( 1.0f - fx ) + at( x1, y0 ) * fx;
            const float b = at( x0, y1 ) * ( 1.0f - fx ) + at( x1, y1 ) * fx;
            return a * ( 1.0f - fy ) + b * fy;
        };
        auto luminance = [ & ]( const LtpImage& image, float u, float v ) {
            return sample( image, u, v, 2 ) * 0.299f +
                   sample( image, u, v, 1 ) * 0.587f +
                   sample( image, u, v, 0 ) * 0.114f;
        };
        auto smoothstep = []( float lo, float hi, float value ) {
            float t = std::clamp( ( value - lo ) / ( hi - lo ), 0.0f, 1.0f );
            return t * t * ( 3.0f - 2.0f * t );
        };

        delete[] data.mBuffer;
        data.mWidth = data.mHeight = OutputSize;
        data.mBuffer = new uint8_t[ static_cast< size_t >( OutputSize ) * OutputSize * 4 ];

        const float moveX = std::sin( phase ) * 0.10f;
        const float moveY = std::cos( phase ) * 0.10f;
        for( int y = 0; y < OutputSize; y++ )
        {
            for( int x = 0; x < OutputSize; x++ )
            {
                constexpr float BakePeriod = 8.0f;
                float u = ( x + 0.5f ) / OutputSize * BakePeriod;
                float v = ( y + 0.5f ) / OutputSize * BakePeriod;

                // LTP combines two opposed LavalayerN samples before looking up lava.png.
                // A looping offset gives the same rolling motion without the original's
                // rather fast one-way scroll.
                const float nx = luminance( distortion, u * 10.0f + moveX,
                                            v * 10.0f ) - 0.5f;
                const float ny = luminance( distortion, u * 8.0f - moveX,
                                            v * 8.0f + moveY ) - 0.5f;
                const float du = ( nx + ny ) * 0.075f;
                const float dv = ( nx - ny ) * 0.075f;
                const float pu = u + du;
                const float pv = v + dv;

                const float maskValue = luminance( mask, pu * 0.75f, pv * 0.75f );
                // Soft coverage retains the organic outline, but the crust itself comes from
                // lavaslag (never from a flat black shader layer).
                const float crust = smoothstep( 0.020f, 0.105f, maskValue );
                const size_t out = ( static_cast< size_t >( y ) * OutputSize + x ) * 4;

                for( int channel = 0; channel < 3; channel++ )
                {
                    const float a = sample( diffuse, pu * 6.0f,
                                           pv * 6.0f + moveY * 0.7f, channel );
                    const float b = sample( diffuse, pu * 4.0f,
                                           pv * 4.0f - moveY * 0.7f, channel );
                    const float c = sample( diffuse, pu * 1.5f + moveX * 0.5f,
                                           pv * 1.5f, channel );
                    const float molten = std::clamp( a * 0.62f + b * 0.46f + c * 0.25f,
                                                     0.0f, 1.0f );
                    const float slag = sample( layer, pu * 15.0f, pv * 15.0f, channel );
                    // Preserve dark red/brown texture in the plates instead of producing
                    // featureless black holes.  Channels are BGRA here.
                    const float crustTint = channel == 2 ? 0.055f : channel == 1 ? 0.016f
                                                                                : 0.010f;
                    const float crustValue = std::clamp( slag * 0.72f + crustTint,
                                                         0.0f, 0.28f );
                    const float value = molten * ( 1.0f - crust ) + crustValue * crust;
                    data.mBuffer[ out + channel ] = static_cast< uint8_t >(
                        std::round( std::clamp( value, 0.0f, 1.0f ) * 255.0f ) );
                }
                data.mBuffer[ out + 3 ] = 255;
            }
        }
    }

    static void AnimateLtpToxic( FTextureBuffer& data, float phase )
    {
        // These are the four source images used by Toxicflat.fp. GZDoom's raster shader
        // cannot run in RTGL, so compose an animated, high-resolution approximation on the
        // CPU. The output covers five editor-texture tiles and is then mapped at 0.2x below,
        // making adjacent subsectors read as one continuous liquid surface.
        static const LtpImage diffuse =
            LoadLtpImage( "textures/Materials/diffuse/toxic.png" );
        static const LtpImage layer =
            LoadLtpImage( "textures/Materials/Diffuselayer/toxiclayer.png" );
        static const LtpImage mask =
            LoadLtpImage( "textures/Materials/Diffuselayermask/Toxicmask.png" );
        static const LtpImage displacement =
            LoadLtpImage( "textures/Materials/Parallax/WaterD.png" );

        if( !diffuse || !layer || !mask || !displacement )
        {
            AnimateLtpLiquid( data, phase );
            return;
        }

        constexpr int   OutputSize = 256;
        constexpr float Tau        = 6.28318530718f;

        auto wrap01 = []( float v ) {
            return v - std::floor( v );
        };
        auto sample = [ & ]( const LtpImage& image, float u, float v, int channel ) {
            u = wrap01( u ) * image.width;
            v = wrap01( v ) * image.height;
            const int x0 = static_cast< int >( std::floor( u ) ) % image.width;
            const int y0 = static_cast< int >( std::floor( v ) ) % image.height;
            const int x1 = ( x0 + 1 ) % image.width;
            const int y1 = ( y0 + 1 ) % image.height;
            const float fx = u - std::floor( u );
            const float fy = v - std::floor( v );
            auto at = [ & ]( int x, int y ) {
                return image.pixels[ 4 * ( static_cast< size_t >( y ) * image.width + x ) +
                                     channel ] /
                       255.0f;
            };
            const float a = at( x0, y0 ) * ( 1.0f - fx ) + at( x1, y0 ) * fx;
            const float b = at( x0, y1 ) * ( 1.0f - fx ) + at( x1, y1 ) * fx;
            return a * ( 1.0f - fy ) + b * fy;
        };
        auto luminance = [ & ]( const LtpImage& image, float u, float v ) {
            return sample( image, u, v, 2 ) * 0.299f + sample( image, u, v, 1 ) * 0.587f +
                   sample( image, u, v, 0 ) * 0.114f;
        };

        delete[] data.mBuffer;
        data.mWidth  = OutputSize;
        data.mHeight = OutputSize;
        data.mBuffer = new uint8_t[ static_cast< size_t >( OutputSize ) * OutputSize * 4 ];

        for( int y = 0; y < OutputSize; y++ )
        {
            for( int x = 0; x < OutputSize; x++ )
            {
                // Bake eight shader-coordinate periods into one RT texture. Eight is the
                // common period of the 0.75, 12.5, 5.0, 1.5 and 0.25 layer scales below,
                // which makes opposite image edges meet at exactly the same phase. Without
                // this, RTGL's texture wrapping exposes rectangular tile boundaries.
                constexpr float BakePeriod = 8.0f;
                const float u = ( x + 0.5f ) / OutputSize * BakePeriod;
                const float v = ( y + 0.5f ) / OutputSize * BakePeriod;

                // Toxicflat.fp first scales map coordinates to 0.2, adds two broad waves,
                // then scrolls its parallax map. Sampling WaterD here supplies the slow
                // rising/falling refraction seen in the original LTP liquid.
                float pu = u + std::sin( v * 3.14159265f + phase ) * 0.05f;
                float pv = v + std::sin( u * 3.14159265f + phase ) * 0.05f;
                const float height =
                    luminance( displacement, pu + std::sin( phase ) * 0.08f,
                               pv + std::cos( phase ) * 0.08f );
                const float rise = ( height - 0.5f ) * 0.075f;
                pu += rise;
                pv += rise;

                const float layerMask = luminance( mask, pu * 0.75f, pv * 0.75f );
                const float topU = pu * 12.5f;
                const float topV = pv * 12.5f;
                const float baseU = pu * 5.0f;
                const float baseV = pv * 5.0f;

                for( int channel = 0; channel < 3; channel++ )
                {
                    const float normalWarp =
                        ( sample( displacement, baseU * 1.5f + std::sin( phase ) * 0.10f,
                                  baseV * 1.5f, channel ) -
                          0.5f ) *
                        0.08f;
                    const float d0 = sample( diffuse,
                                             ( baseU * 1.5f + std::sin( phase ) * 0.15f + normalWarp ) *
                                                 0.25f,
                                             baseV * 1.5f * 0.25f, channel );
                    const float d1 = sample( diffuse,
                                             ( baseU - std::sin( phase ) * 0.15f - normalWarp ) * 0.25f,
                                             baseV * 0.25f, channel );
                    const float d2 = sample( diffuse,
                                             ( baseU * 1.5f + std::cos( phase ) * 0.15f ) * 0.10f,
                                             baseV * 1.5f * 0.10f, channel );
                    const float d3 = sample( diffuse,
                                             ( baseU - std::cos( phase ) * 0.15f ) * 0.10f,
                                             baseV * 0.10f, channel );
                    const float base = std::clamp( d0 + d1 + d2 + d3 -
                                                       std::clamp( layerMask * 15.0f, 0.0f,
                                                                   1.0f ),
                                                   0.0f, 1.0f );
                    const float top = sample( layer, topU, topV, channel ) *
                                      std::clamp( layerMask * 100.0f, 0.0f, 1.0f );
                    const float glow =
                        std::clamp( d0 + d3 * 0.5f, 0.0f, 1.0f ) * layerMask;
                    // Keep the toxic material dark like LTP's raster shader. In the path
                    // tracer, values near white are also interpreted as extremely strong
                    // emissive sources, so cap and curve them instead of washing the pool out.
                    const float combined = std::clamp( base + top * 0.5f + glow, 0.0f, 1.0f );
                    // Toxicflat's visible identity is a nearly black base with saturated green
                    // organic islands.  Keep that grading in the baked material; RT lighting
                    // can then add highlights without turning the mask into grey paint.
                    const float grade = channel == 1 ? 1.38f : channel == 2 ? 0.34f : 0.24f;
                    const float final = std::min( std::pow( combined, 1.34f ) * grade, 0.78f );

                    data.mBuffer[ 4 * ( static_cast< size_t >( y ) * OutputSize + x ) +
                                  channel ] =
                        static_cast< uint8_t >( std::round( final * 255.0f ) );
                }
                data.mBuffer[ 4 * ( static_cast< size_t >( y ) * OutputSize + x ) + 3 ] = 255;
            }
        }
    }

    static void AnimateLtpLiquid( FTextureBuffer& data, float seconds )
    {
        const int width  = data.mWidth;
        const int height = data.mHeight;
        if( width < 2 || height < 2 )
        {
            return;
        }

        const size_t byteCount = static_cast< size_t >( width ) * height * 4;
        std::vector< uint8_t > source( data.mBuffer, data.mBuffer + byteCount );

        auto wrap = []( int value, int size ) {
            value %= size;
            return value < 0 ? value + size : value;
        };
        auto sourcePixel = [ & ]( int x, int y ) {
            x = wrap( x, width );
            y = wrap( y, height );
            return &source[ 4 * ( x * static_cast< size_t >( height ) + y ) ];
        };

        constexpr float Tau = 6.28318530718f;
        for( int x = 0; x < width; x++ )
        {
            for( int y = 0; y < height; y++ )
            {
                const float nx = static_cast< float >( x ) / width;
                const float ny = static_cast< float >( y ) / height;

                // Several opposing waves create local circulation and a gentle rise/fall
                // impression. There is deliberately no ever-increasing UV offset here.
                const float dx = std::sin( ny * Tau * 2.0f + seconds * 1.10f ) * 2.7f +
                                 std::sin( ( nx + ny ) * Tau - seconds * 0.63f ) * 1.5f;
                const float dy = std::sin( nx * Tau * 2.0f - seconds * 0.96f ) * 2.7f +
                                 std::cos( ( nx - ny ) * Tau + seconds * 0.57f ) * 1.5f;

                const uint8_t* a = sourcePixel( static_cast< int >( std::round( x + dx ) ),
                                                static_cast< int >( std::round( y + dy ) ) );
                const uint8_t* b = sourcePixel( static_cast< int >( std::round( x - dy * 0.55f ) ),
                                                static_cast< int >( std::round( y + dx * 0.55f ) ) );
                uint8_t* out = &data.mBuffer[ 4 *
                                              ( x * static_cast< size_t >( height ) + y ) ];
                for( int channel = 0; channel < 4; channel++ )
                {
                    out[ channel ] = static_cast< uint8_t >(
                        ( static_cast< unsigned >( a[ channel ] ) * 3u + b[ channel ] ) / 4u );
                }
            }
        }
    }

    static auto MakeTextureName( FGameTexture& fgametex ) -> std::string
    {
        // highest priority: FGameTexture name
        if( !fgametex.GetName().IsEmpty() )
        {
            return fgametex.GetName().GetChars();
        }

        // if no lump name, stringify the image ID;
        // this is undesirable for textures that require a replacement
        // (which are found by texname; and because ID is assigned at runtime,
        // replacements can't be found correctly)
        if( FTexture* ftex = fgametex.GetTexture() )
        {
            if( FImageSource* imgsrc = ftex->GetImage() )
            {
                // MSVC's std::string has 16 chars inlined,
                // so no allocation should happen
                return std::to_string( imgsrc->GetId() );
            }
        }

        assert( 0 );
        return {};
    }

private:
    bool        m_created{ false };
    std::string m_name{};
    std::vector< std::string > m_ltpFrameNames{};
    uint64_t    m_lastLtpAnimationFrame{ ~uint64_t{ 0 } };
};


template< typename T >
void ApplyMat33ToVec3_row( const T row_mat[ 3 ][ 3 ], float ( &v )[ 3 ] )
{
    RgFloat3D r;
    for( int i = 0; i < 3; i++ )
    {
        r.data[ i ] = row_mat[ i ][ 0 ] * T( v[ 0 ] ) + row_mat[ i ][ 1 ] * T( v[ 1 ] ) +
                      row_mat[ i ][ 2 ] * T( v[ 2 ] );
    }
    v[ 0 ] = r.data[ 0 ];
    v[ 1 ] = r.data[ 1 ];
    v[ 2 ] = r.data[ 2 ];
}

template< typename T >
RgFloat4D ApplyMat44ToVec4( const T column_mat[ 4 ][ 4 ], const RgFloat4D& vs )
{
    const auto* v = vs.data;
    RgFloat4D   r;
    for( int i = 0; i < 4; i++ )
    {
        r.data[ i ] = column_mat[ 0 ][ i ] * T( v[ 0 ] ) + column_mat[ 1 ][ i ] * T( v[ 1 ] ) +
                      column_mat[ 2 ][ i ] * T( v[ 2 ] ) + column_mat[ 3 ][ i ] * T( v[ 3 ] );
    }
    return r;
}

template< typename T >
RgFloat4D ApplyMat44ToVec4( const T* column_mat, const RgFloat4D& vs )
{
    return ApplyMat44ToVec4< T >( reinterpret_cast< const T( * )[ 4 ] >( column_mat ), vs );
}

RgFloat3D FromHomogeneous( const RgFloat4D& v )
{
    return RgFloat3D{ v.data[ 0 ] / v.data[ 3 ],
                      v.data[ 1 ] / v.data[ 3 ],
                      v.data[ 2 ] / v.data[ 3 ] };
}

class RTRenderState : public FRenderState
{
public:
    explicit RTRenderState( RTFrameBuffer* parent ) : m_fb( parent ) {}
    virtual ~RTRenderState() = default;

    void RT_BeginFrame()
    {
        rtstate.reset();
        m_weaponDrawCallIndex = 0;
    }

    bool IsCurrentDrawIgnored() const
    {
        return rtstate.is< RtPrim::Ignored >() || mTextureMode == TM_FOGLAYER;
    }

    bool IsSpectre() const
    {
        switch( mRenderStyle.BlendOp )
        {
            case STYLEOP_Fuzz:
            case STYLEOP_FuzzOrAdd:
            case STYLEOP_FuzzOrSub:
            case STYLEOP_FuzzOrRevSub:
            case STYLEOP_Shadow: return true;
            default: return false;
        }
    }

    void Draw( int dt, int index, int count, bool apply = true ) override
    {
        if( IsCurrentDrawIgnored() )
        {
            return;
        }

        assert( count > 0 );

        const uint32_t* pIndices   = nullptr;
        uint32_t        indexCount = 0;

        bool islines = false;

        switch( dt )
        {
            case DT_Points: assert( 0 ); return;
            case DT_Lines: islines = true; break;
            case DT_Triangles:
                // indices are sequential, just use vertex array
                break;
            case DT_TriangleFan:
                rt.rgUtilScratchGetIndices(
                    RG_UTIL_IM_SCRATCH_TOPOLOGY_TRIANGLE_FAN, count, &pIndices, &indexCount );
                break;
            case DT_TriangleStrip:
                rt.rgUtilScratchGetIndices(
                    RG_UTIL_IM_SCRATCH_TOPOLOGY_TRIANGLE_STRIP, count, &pIndices, &indexCount );
                break;
            default: break;
        }

        auto vb = static_cast< RTVertexBuffer* >( mVertexBuffer );
        if( !vb )
        {
            assert( 0 );
            return;
        }
        assert( rtstate.is< RtPrim::Sky >() == vb->IsSky() );

        InternalDraw( vb->AccessFormatted( mVertexOffsets[ 0 ] + index, count ),
                      std::span{ pIndices, indexCount },
                      vb->IsUI(),
                      islines );
    }

    void DrawIndexed( int dt, int index, int count, bool apply = true ) override
    {
        if( IsCurrentDrawIgnored() )
        {
            return;
        }

        assert( dt == DT_Triangles );
        if( count <= 0 )
        {
            // E3M2 fails
            return;
        }

        auto vb = static_cast< RTVertexBuffer* >( mVertexBuffer );
        if( !vb )
        {
            assert( 0 );
            return;
        }
        assert( rtstate.is< RtPrim::Sky >() == vb->IsSky() );

        auto ib = static_cast< RTIndexBuffer* >( mIndexBuffer );
        if( !ib )
        {
            assert( 0 );
            return;
        }

        auto indices = ib->AccessFormatted( index, count );

        auto [ vertFirst, vertCount ] = RTIndexBuffer::CalcFirstVertexAndVertexCount( indices );

        InternalDraw( vb->AccessFormatted( mVertexOffsets[ 0 ] + vertFirst, vertCount ),
                      ib->MakeWithNewFirstIndex( indices, vertFirst ),
                      vb->IsUI() );
    }

    void ClearScreen() override {}
    bool SetDepthClamp( bool on ) override { return on; }
    void SetDepthMask( bool on ) override {}
    void SetDepthFunc( int func ) override {}
    void SetDepthRange( float min, float max ) override {}
    void SetColorMask( bool r, bool g, bool b, bool a ) override {}
    void SetStencil( int offs, int op, int flags = -1 ) override {}
    void SetCulling( int mode ) override {}
    void EnableClipDistance( int num, bool state ) override {}
    void Clear( int targets ) override {}
    void EnableStencil( bool on ) override {}
    void SetScissor( int x, int y, int w, int h ) override {}
    void SetViewport( int x, int y, int w, int h ) override
    {
        m_viewport = RgViewport{
            .x        = float( x ),
            .y        = float( y ),
            .width    = float( w ),
            .height   = float( h ),
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        };
    }
    void EnableDepthTest( bool on ) override {}
    void EnableMultisampling( bool on ) override {}
    void EnableLineSmooth( bool on ) override {}
    void EnableDrawBuffers( int count, bool apply ) override {}

private:
    static bool IsPerspectiveMatrix( const float* m );
    static bool IsLikeIdentity( const float* m );
    static bool IsLikeIdentity( const double* m );

    // If need to calculate a transform at the sprite's bottom.
    bool RequiresTrueTransform() const
    {
        if( rtstate.is< RtPrim::ExportInstance >() )
        {
            // need to make a true one, since gzdoom doesn't provide a world transform
            return !mModelMatrixEnabled;
        }
        return false;
    }

    auto CalculateTrueTransformAndItsVerts( std::span< const RgPrimitiveVertex > originalVerts )
        -> std::pair< RgTransform, std::span< const RgPrimitiveVertex > >
    {
        assert( RequiresTrueTransform() );
        assert( originalVerts.size() == 4 ); // to find a non-sprite without model matrix
        assert( !mModelMatrixEnabled );      // means that vert positions are in a metric space

        // need to offset a bit, to prevent clipping with floor (for glass spectres)
        constexpr float CLIP_FIX_OFFSET = 0.005f;

        const float pivot[] = {
            rtstate.m_lastthingposition.X * ONEGAMEUNIT_IN_METERS,
            rtstate.m_lastthingposition.Y * ONEGAMEUNIT_IN_METERS,
            rtstate.m_lastthingposition.Z * ONEGAMEUNIT_IN_METERS + CLIP_FIX_OFFSET,
        };

        m_tempverts.clear();
        m_tempverts.assign( originalVerts.begin(), originalVerts.end() );

        // make relative to pivot
        for( uint32_t v = 0; v < originalVerts.size(); v++ )
        {
            m_tempverts[ v ].position[ 0 ] -= pivot[ 0 ];
            m_tempverts[ v ].position[ 1 ] -= pivot[ 1 ];
            m_tempverts[ v ].position[ 2 ] -= pivot[ 2 ];
        }

        // un-rotate the angle
        const auto [ pitch, yaw ] = rtstate.get_spriterotation();
        
#if 0 // reference
        Matrix3x4 m;
        m.MakeIdentity();
        m.Rotate( 0, 0, 1, to_deg( yaw ) );
        m.Rotate( 0, 1, 0, to_deg( pitch ) );
#else
        const float cos_pitch = std::cos( pitch );
        const float sin_pitch = std::sin( pitch );
        const float cos_yaw   = std::cos( yaw );
        const float sin_yaw   = std::sin( yaw );

        //     |  cos_pitch, 0, sin_pitch |   | cos_yaw, -sin_yaw, 0 |
        // m = |          0, 1,         0 | x | sin_yaw,  cos_yaw, 0 |
        //     | -sin_pitch, 0, cos_pitch |   |       0,       0,  1 |

        float m[ 3 ][ 3 ] = {
            { cos_yaw * cos_pitch, -sin_yaw, cos_yaw * sin_pitch },
            { sin_yaw * cos_pitch, cos_yaw, sin_yaw * sin_pitch },
            { -sin_pitch, 0, cos_pitch },
        };
#endif
        const float m_inv[ 3 ][ 3 ] = {
            { m[ 0 ][ 0 ], m[ 1 ][ 0 ], m[ 2 ][ 0 ] },
            { m[ 0 ][ 1 ], m[ 1 ][ 1 ], m[ 2 ][ 1 ] },
            { m[ 0 ][ 2 ], m[ 1 ][ 2 ], m[ 2 ][ 2 ] },
        };
        for( auto& v : m_tempverts )
        {
            ApplyMat33ToVec3_row( m_inv, v.position );
        }

        return {
            RgTransform{ {
                { m[ 0 ][ 0 ], m[ 0 ][ 1 ], m[ 0 ][ 2 ], pivot[ 0 ] },
                { m[ 1 ][ 0 ], m[ 1 ][ 1 ], m[ 1 ][ 2 ], pivot[ 1 ] },
                { m[ 2 ][ 0 ], m[ 2 ][ 1 ], m[ 2 ][ 2 ], pivot[ 2 ] },
            } },
            std::span{ m_tempverts },
        };
    }

    auto MakeTransform( bool isSky ) const -> RgTransform
    {
        assert( !RequiresTrueTransform() );

        // also converts to metric
        auto fromGzMatrix = []( const float* m ) {
            return RgTransform{ {
                { m[ 0 ], m[ 4 ], m[ 8 ], m[ 12 ] * ONEGAMEUNIT_IN_METERS },
                { m[ 1 ], m[ 5 ], m[ 9 ], m[ 13 ] * ONEGAMEUNIT_IN_METERS },
                { m[ 2 ], m[ 6 ], m[ 10 ], m[ 14 ] * ONEGAMEUNIT_IN_METERS },
            } };
        };

        // sky has view matrix that is different from main camera, apply it
        if( isSky )
        {
            auto l_unit = []( float f ) {
                return f > +0.5f   ? +1.0f //
                       : f < -0.5f ? -1.0f //
                                   : 0.0f;
            };

            auto skyToMainCameraIrregular =
                VSMatrix::smultMatrix( m_mainCameraView_Inverse, m_view );

            const float* irr = skyToMainCameraIrregular.get();

            const float skyToMainCamera[ 16 ] = {
                l_unit( irr[ 0 ] ), l_unit( irr[ 1 ] ), l_unit( irr[ 2 ] ),  0,
                l_unit( irr[ 4 ] ), l_unit( irr[ 5 ] ), l_unit( irr[ 6 ] ),  0,
                l_unit( irr[ 8 ] ), l_unit( irr[ 9 ] ), l_unit( irr[ 10 ] ), 0,
                irr[ 12 ],          irr[ 13 ],          irr[ 14 ],           1,
            };

            auto skyTransform = mModelMatrix;
            skyTransform.scale( 1, cvar::rt_sky_stretch, 1 );

            auto t = VSMatrix::smultMatrix( skyToMainCamera, skyTransform.get() );
            return fromGzMatrix( t.get() );
        }

        if( mModelMatrixEnabled )
        {
            return fromGzMatrix( mModelMatrix.get() );
        }

        return RG_TRANSFORM_IDENTITY;
    }

    auto MapLightLevel( int lightlevel ) -> float
    {
        assert( lightlevel <= 255 );
        int lmin = std::max< int >( cvar::rt_lightlevel_min, 0 );
        int lmax = std::min< int >( cvar::rt_lightlevel_max, 255 );

        if( lmin >= lmax )
        {
            return 0.0f;
        }
        if( lightlevel <= lmin )
        {
            return 0.0f;
        }
        if( lightlevel >= lmax )
        {
            return 1.0f;
        }
        float t = float( lightlevel - lmin ) / float( lmax - lmin );

        if( std::abs( cvar::rt_lightlevel_exp - 2.f ) < 0.01f )
        {
            return t * t;
        }
        if( std::abs( cvar::rt_lightlevel_exp - 1.f ) < 0.01f )
        {
            return t;
        }
        return std::powf( t, cvar::rt_lightlevel_exp );
    }

    auto MakeFirstPersonQuadInWorldSpace( std::span< const RgPrimitiveVertex > verts )
        -> std::pair< RgTransform, std::span< const RgPrimitiveVertex > >
    {
        if( verts.size() != 4 )
        {
            // assert( 0 );
            return { RgTransform{ RG_TRANSFORM_IDENTITY }, verts };
        }

        const auto  priority = m_weaponDrawCallIndex++;
        const float z        = 0.1f / float( 1 + priority );

        auto toPix = []( const RgPrimitiveVertex& vert ) {
            // because of MakeFormatted...
            return RgFloat2D{
                vert.position[ 0 ] / ONEGAMEUNIT_IN_METERS,
                vert.position[ 2 ] / ONEGAMEUNIT_IN_METERS,
            };
        };

        auto applyViewport = []( const RgViewport& vp, const RgFloat2D& vert ) {
            return RgFloat2D{
                vert.data[ 0 ] / float( vp.width ),
                vert.data[ 1 ] / float( vp.height ),
            };
        };

        // screen space [0,1]
        RgFloat2D scr01[] = {
            applyViewport( m_viewport, toPix( verts[ 0 ] ) ),
            applyViewport( m_viewport, toPix( verts[ 1 ] ) ),
            applyViewport( m_viewport, toPix( verts[ 2 ] ) ),
            applyViewport( m_viewport, toPix( verts[ 3 ] ) ),
        };

        // remap [0,1] to [-1,1] clip space
        RgFloat4D clipspace[] = {
            RgFloat4D{ scr01[ 0 ].data[ 0 ] * 2 - 1, scr01[ 0 ].data[ 1 ] * 2 - 1, z, 1.0f },
            RgFloat4D{ scr01[ 1 ].data[ 0 ] * 2 - 1, scr01[ 1 ].data[ 1 ] * 2 - 1, z, 1.0f },
            RgFloat4D{ scr01[ 2 ].data[ 0 ] * 2 - 1, scr01[ 2 ].data[ 1 ] * 2 - 1, z, 1.0f },
            RgFloat4D{ scr01[ 3 ].data[ 0 ] * 2 - 1, scr01[ 3 ].data[ 1 ] * 2 - 1, z, 1.0f },
        };

        // inverse projection to transform clip space -> view space
        RgFloat4D viewspace[] = {
            ApplyMat44ToVec4( m_mainCameraProjection_Inverse, clipspace[ 0 ] ),
            ApplyMat44ToVec4( m_mainCameraProjection_Inverse, clipspace[ 1 ] ),
            ApplyMat44ToVec4( m_mainCameraProjection_Inverse, clipspace[ 2 ] ),
            ApplyMat44ToVec4( m_mainCameraProjection_Inverse, clipspace[ 3 ] ),
        };

#if 0
        // inverse view to transform view space -> world space
        RgFloat3D worldspace[] = {
            FromHomogeneous( ApplyMat44ToVec4( m_mainCameraView_Inverse, viewspace[ 0 ] ) ),
            FromHomogeneous( ApplyMat44ToVec4( m_mainCameraView_Inverse, viewspace[ 1 ] ) ),
            FromHomogeneous( ApplyMat44ToVec4( m_mainCameraView_Inverse, viewspace[ 2 ] ) ),
            FromHomogeneous( ApplyMat44ToVec4( m_mainCameraView_Inverse, viewspace[ 3 ] ) ),
        };

        m_tempverts.clear();
        m_tempverts.assign( verts.begin(), verts.end() );
        for( uint32_t i = 0; i < std::size( worldspace ); i++ )
        {
            // because of m_mainCameraView_Inverse, m_mainCameraProjection_Inverse,
            // vi_world already have ONEGAMEUNIT_IN_METERS applied
            m_tempverts[ i ].position[ 0 ] = worldspace[ i ].data[ 0 ];
            m_tempverts[ i ].position[ 1 ] = worldspace[ i ].data[ 1 ];
            m_tempverts[ i ].position[ 2 ] = worldspace[ i ].data[ 2 ];
        }
        return m_tempverts;
#else

        // treat m_mainCameraView_Inverse as the transform
        const float* t = m_mainCameraView_Inverse;
        
        auto transform = RgTransform{ {
            { t[ 0 ], t[ 4 ], t[ 8 ], t[ 12 ] },
            { t[ 1 ], t[ 5 ], t[ 9 ], t[ 13 ] },
            { t[ 2 ], t[ 6 ], t[ 10 ], t[ 14 ] },
        } };

        m_tempverts.clear();
        m_tempverts.assign( verts.begin(), verts.end() );
        for( uint32_t i = 0; i < std::size( viewspace ); i++ )
        {
            double w = viewspace[ i ].data[ 3 ];
            w        = std::max( w, 0.00000001 );

            // because of m_mainCameraView_Inverse, m_mainCameraProjection_Inverse,
            // vi_world already have ONEGAMEUNIT_IN_METERS applied
            m_tempverts[ i ].position[ 0 ] = float( viewspace[ i ].data[ 0 ] / w );
            m_tempverts[ i ].position[ 1 ] = float( viewspace[ i ].data[ 1 ] / w );
            m_tempverts[ i ].position[ 2 ] = float( viewspace[ i ].data[ 2 ] / w );
        }
        return { transform, m_tempverts };
#endif
    }

    void InternalDraw( std::span< const RgPrimitiveVertex > verts,
                       std::span< const uint32_t >          indices,
                       const bool                           isUI,
                       const bool                           islines = false )
    {
        assert( RG_PACKED_COLOR_WHITE == rt.rgUtilPackColorByte4D( 255, 255, 255, 255 ) );

        if( islines && !isUI )
        {
            assert( 0 );
            return;
        }

        if( verts.empty() )
        {
            assert( 0 );
            return;
        }

        const char* texname = nullptr;
        if( mTextureEnabled && mMaterial.mMaterial )
        {
            if( FGameTexture* gametex = mMaterial.mMaterial->sourcetex )
            {
                if( FTexture* base = gametex->GetTexture() )
                {
                    if( auto hwtex = static_cast< RTHardwareTexture* >( base->GetHardwareTexture(
                            mMaterial.mTranslation, mMaterial.mMaterial->GetScaleFlags() ) ) )
                    {
                        hwtex->CreateIfWasnt( *gametex,
                                              mMaterial.mClampMode,
                                              mMaterial.mTranslation,
                                              mMaterial.mMaterial->GetScaleFlags(),
                                              mRenderStyle );
                        texname = hwtex->GetRTName();
                    }
                }
            }
        }

        if( !texname && !isUI && !rtstate.is< RtPrim::Sky >() &&
            !rtstate.is< RtPrim::SkyVisibility >() )
        {
            // assert( 0 );
        }

        const LtpLiquidKind ltpKind = cvar::rt_ltp_liquids
                                          ? GetLtpLiquidKind( texname )
                                          : LtpLiquidKind::None;
        const bool isLtpLiquid = ltpKind != LtpLiquidKind::None;
        const bool isLtpFall   = IsLtpFall( ltpKind );
        if( isLtpLiquid )
        {
            RTHardwareTexture::CreateLtpMaterialTexturesIfNeeded();
            switch( ltpKind )
            {
                case LtpLiquidKind::Water:
                case LtpLiquidKind::WaterFall: texname = "rt_ltp_water_diffuse"; break;
                case LtpLiquidKind::Blood:
                case LtpLiquidKind::BloodFall: texname = "rt_ltp_blood_diffuse"; break;
                case LtpLiquidKind::Slime:
                case LtpLiquidKind::SlimeFall: texname = "rt_ltp_slime_diffuse"; break;
                case LtpLiquidKind::Toxic:
                    texname = RTHardwareTexture::GetLtpMaterialFrameName( false );
                    break;
                case LtpLiquidKind::ToxicFall: texname = "rt_ltp_toxic_diffuse"; break;
                case LtpLiquidKind::Lava:
                    texname = RTHardwareTexture::GetLtpMaterialFrameName( true );
                    break;
                case LtpLiquidKind::LavaFall: texname = "rt_ltp_lava_diffuse"; break;
                default: break;
            }
        }

        // TODO: apply texture matrix on gpu
        if( mTextureMatrixEnabled )
        {
            m_tempverts.clear();
            m_tempverts.assign( verts.begin(), verts.end() );

            auto applyTexMatrix = [ & ]( float u, float v ) {
                auto m = [ & ]( int i, int j ) {
                    return mTextureMatrix.get()[ i + j * 4 ];
                };

                return std::pair{
                    m( 0, 0 ) * u + m( 1, 0 ) * v,
                    m( 0, 1 ) * u + m( 1, 1 ) * v,
                };
            };

            for( RgPrimitiveVertex& v : m_tempverts )
            {
                std::tie( v.texCoord[ 0 ], v.texCoord[ 1 ] ) =
                    applyTexMatrix( v.texCoord[ 0 ], v.texCoord[ 1 ] );
            }

            verts = m_tempverts;
        }

        // Doom can restart flat UVs at subsector boundaries. That is normally harmless for a
        // repeating 64x64 flat, but becomes visible after LTP's 0.2 macro scale is applied.
        // Rebuild the coordinates from absolute map position so every subsector samples one
        // continuous liquid sheet. The vertical travel mirrors Toxicflat.fp's main scroll.
        std::vector< RgPrimitiveVertex > ltpScaledVerts;
        std::vector< RgFloat2D >          ltpDetailCoords;
        std::vector< RgFloat2D >          ltpMaskCoords;
        if( isLtpLiquid )
        {
            ltpScaledVerts.assign( verts.begin(), verts.end() );
            ltpDetailCoords.reserve( verts.size() );
            ltpMaskCoords.reserve( verts.size() );
            const float seconds = screen ? static_cast< float >( screen->FrameTime ) * 0.001f
                                         : 0.0f;
            constexpr float FlatWorldPeriodMeters = 64.0f * ONEGAMEUNIT_IN_METERS;
            const float* model = mModelMatrixEnabled ? mModelMatrix.get() : nullptr;
            for( size_t i = 0; i < ltpScaledVerts.size(); i++ )
            {
                auto& vertex = ltpScaledVerts[ i ];
                const float originalU = vertex.texCoord[ 0 ];
                const float originalV = vertex.texCoord[ 1 ];

                if( isLtpFall )
                {
                    // Falls need their wall UVs: world X/Y collapses on vertical geometry.
                    // Scroll continuously downward and let the foam/mask layer travel faster.
                    vertex.texCoord[ 0 ] = originalU * 0.25f;
                    vertex.texCoord[ 1 ] = originalV * 0.25f - seconds * 0.18f;
                    ltpDetailCoords.push_back(
                        RgFloat2D{ { originalU * 0.72f + seconds * 0.025f,
                                     originalV * 0.72f - seconds * 0.42f } } );
                    ltpMaskCoords.push_back(
                        RgFloat2D{ { originalU * 0.18f - seconds * 0.012f,
                                     originalV * 0.18f - seconds * 0.11f } } );
                    continue;
                }

                // FFlatVertex positions are metric, but can still be local to a GZDoom model
                // matrix. Include that transform so adjacent draw calls/subsectors agree on
                // one absolute map coordinate.
                const float worldX = model ? model[ 0 ] * vertex.position[ 0 ] +
                                                 model[ 4 ] * vertex.position[ 1 ] +
                                                 model[ 8 ] * vertex.position[ 2 ] +
                                                 model[ 12 ] * ONEGAMEUNIT_IN_METERS
                                           : vertex.position[ 0 ];
                const float worldY = model ? model[ 1 ] * vertex.position[ 0 ] +
                                                 model[ 5 ] * vertex.position[ 1 ] +
                                                 model[ 9 ] * vertex.position[ 2 ] +
                                                 model[ 13 ] * ONEGAMEUNIT_IN_METERS
                                           : vertex.position[ 1 ];

                const float mapU = worldX / FlatWorldPeriodMeters;
                const float mapV = worldY / FlatWorldPeriodMeters;
                const bool  isLava = ltpKind == LtpLiquidKind::Lava;
                const bool  isToxic = ltpKind == LtpLiquidKind::Toxic;
                const bool  isMaskedMaterial = isLava || isToxic;
                // Lava has no RTGL refractive-water flag, so make its molten layers visibly
                // heave in texture space instead. Toxic/water retain their native RT waves.
                const float bob = std::sin( seconds * ( isLava ? 0.85f : 1.35f ) ) *
                                  ( isLava ? 0.12f : 0.025f );

                // Every horizontal liquid is sampled in absolute map space. This removes
                // subsector seams while retaining LTP's broad, slow rolling motion.
                // Lavaflat.fp derives its diffuse at roughly 0.8 of the editor-flat UV,
                // substantially denser than toxic's 0.25 macro layer.
                // The baked toxic/lava frame contains eight shader-coordinate periods.
                // Mapping it at 1/8 scale restores the original material's world-space size
                // and keeps every subsector on one continuous map-aligned surface.
                const float baseScale = isMaskedMaterial ? 0.125f : 0.25f;
                const float baseSpeed = isLava ? 0.012f : isToxic ? 0.018f : 0.030f;
                const float detailScale = isMaskedMaterial ? 0.15f : 1.75f;
                vertex.texCoord[ 0 ] = mapU * baseScale + ( isMaskedMaterial ? 0.0f : seconds * baseSpeed );
                vertex.texCoord[ 1 ] = mapV * baseScale + ( isMaskedMaterial ? 0.0f : bob );
                ltpDetailCoords.push_back(
                    RgFloat2D{ { isMaskedMaterial ? mapU : mapU * detailScale - seconds * 0.070f,
                                 isMaskedMaterial ? mapV : mapV * detailScale - bob * 1.6f } } );
                ltpMaskCoords.push_back(
                    RgFloat2D{ { isMaskedMaterial ? mapU : mapU * 0.15f + seconds * 0.008f,
                                 isMaskedMaterial ? mapV : mapV * 0.15f + bob * 0.5f } } );
            }
            verts = ltpScaledVerts;
        }

        if( rtstate.is< RtPrim::Sky >() && texname )
        {
            m_fb->RT_MarkWasSky();
        }

        RgTransform transform;
        if( rtstate.is< RtPrim::FirstPerson >() )
        {
            std::tie( transform, verts ) = MakeFirstPersonQuadInWorldSpace( verts );
        }
        else if( RequiresTrueTransform() )
        {
            std::tie( transform, verts ) = CalculateTrueTransformAndItsVerts( verts );
        }
        else
        {
            transform = MakeTransform( rtstate.is< RtPrim::Sky >() );
        }

        auto ui = RgMeshPrimitiveSwapchainedEXT{
            .sType       = RG_STRUCTURE_TYPE_MESH_PRIMITIVE_SWAPCHAINED_EXT,
            .pNext       = nullptr,
            .flags       = islines ? uint32_t{ RG_MESH_PRIMITIVE_SWAPCHAINED_DRAW_AS_LINES } : 0,
            .pViewport   = &m_viewport,
            .pView       = m_view,
            .pProjection = m_projection,
            .pViewProjection = nullptr,
        };

        auto l_makeInstanceFlags = [ & ]() -> RgMeshInfoFlags {
            if( rtstate.is< RtPrim::FirstPersonViewer >() )
            {
                return RG_MESH_FIRST_PERSON_VIEWER;
            }
            if( rtstate.is< RtPrim::FirstPerson >() )
            {
                return RG_MESH_FIRST_PERSON;
            }
            return 0;
        };

        auto l_makeSpectreFlags = [ & ]() -> RgMeshInfoFlags {
            if( IsSpectre() )
            {
                bool firstperson = rtstate.is< RtPrim::FirstPersonViewer >() ||
                                   rtstate.is< RtPrim::FirstPerson >();

                // suppress inter-reflection on spectres
                RgMeshInfoFlags fs = firstperson ? 0 : RG_MESH_FORCE_IGNORE_REFRACT_AFTER;

                int mode = firstperson ? *cvar::rt_spectre_invis1 : *cvar::rt_spectre;
                switch( mode )
                {
                    case 1: return fs | RG_MESH_FORCE_GLASS;
                    case 2: return fs | RG_MESH_FORCE_MIRROR;
                    default: return fs | RG_MESH_FORCE_WATER;
                }
            }
            return 0;
        };

        auto mesh = RgMeshInfo{
            .sType = RG_STRUCTURE_TYPE_MESH_INFO,
            .pNext = nullptr,
            .flags =
                l_makeInstanceFlags() | l_makeSpectreFlags() |
                ( rtstate.is< RtPrim::ExportInstance >() ? RG_MESH_EXPORT_AS_SEPARATE_FILE : 0 ),
            .uniqueObjectID = rtstate.get_uniqueid(),
            .pMeshName      = rtstate.is< RtPrim::ExportMap >() ? RT_GetMapName()
                              : rtstate.is< RtPrim::ExportInstance >()
                                  ? rtstate.get_exportinstance_name()
                                  : nullptr,
            .transform      = transform,
            .isExportable =
                rtstate.is< RtPrim::ExportMap >() || rtstate.is< RtPrim::ExportInstance >(),
            .animationTime        = 0.0f,
            .localLightsIntensity = MapLightLevel( rtstate.m_lightlevel ),
        };

        auto makePrimFlags = [ this, &verts, ltpKind ]( bool isUI ) -> RgMeshPrimitiveFlags {
            if( isUI )
            {
                return RG_MESH_PRIMITIVE_TRANSLUCENT;
            }
            if( rtstate.is< RtPrim::Decal >() )
            {
                assert( verts.size() == 4 );
                return RG_MESH_PRIMITIVE_DECAL;
            }
            if( rtstate.is< RtPrim::SkyVisibility >() )
            {
                return RG_MESH_PRIMITIVE_SKY_VISIBILITY;
            }
            if( rtstate.is< RtPrim::Sky >() )
            {
                return RG_MESH_PRIMITIVE_SKY | RG_MESH_PRIMITIVE_TRANSLUCENT;
            }
            if( rtstate.is< RtPrim::Particle >() )
            {
                return RG_MESH_PRIMITIVE_TRANSLUCENT;
            }
            if( rtstate.is< RtPrim::Mirror >() )
            {
                return RG_MESH_PRIMITIVE_MIRROR;
            }
            if( rtstate.is< RtPrim::Glass >() )
            {
                return RG_MESH_PRIMITIVE_GLASS;
            }

            RgMeshPrimitiveFlags add;
            switch( int( cvar::rt_wall_nomv ) )
            {
                case 0: add = 0; break;
                case 2: add = RG_MESH_PRIMITIVE_NO_MOTION_VECTORS; break;
                default:
                    add = rtstate.is< RtPrim::NoMotionVectors >()
                              ? RG_MESH_PRIMITIVE_NO_MOTION_VECTORS
                              : 0;
                    break;
            }

            // View-weapon PNGs from modern weapon packs commonly carry transparent pixels
            // without setting GZDoom's alpha-threshold field. Treating that quad as opaque
            // exposes its black image rectangle in the RT renderer.
            if( rtstate.is< RtPrim::FirstPerson >() )
            {
                return RG_MESH_PRIMITIVE_ALPHA_TESTED | RG_MESH_PRIMITIVE_NO_SHADOW | add;
            }

            // RenderStyle Shaded uses the texture's red channel as continuously varying alpha.
            // LTP's splash PNGs are opaque RGB images with black backgrounds, so treating these
            // actors as ordinary opaque sprites exposes their rectangular image bounds.
            if( mRenderStyle.Flags & STYLEF_RedIsAlpha )
            {
                // RT translucency uses stochastic sampling and makes LTP's fine splash sheets
                // look like coarse voxels. Alpha testing preserves their clean silhouette and
                // the uploaded red-as-alpha mask without exposing the black rectangle.
                return RG_MESH_PRIMITIVE_ALPHA_TESTED | RG_MESH_PRIMITIVE_NO_SHADOW | add;
            }

            RgMeshPrimitiveFlags liquid = 0;
            if( ltpKind == LtpLiquidKind::Water || ltpKind == LtpLiquidKind::Blood )
            {
                liquid = RG_MESH_PRIMITIVE_WATER;
            }
            else if( ltpKind == LtpLiquidKind::Slime )
            {
                liquid = RG_MESH_PRIMITIVE_ACID;
            }
            return ( mAlphaThreshold > 0 ? RG_MESH_PRIMITIVE_ALPHA_TESTED : 0 ) | add |
                   liquid;
        };

        // HACKHACK: replacements are ignored if a prim is rasterized, force alpha=1.0
        const bool forcealpha1 = ( mesh.flags & RG_MESH_FORCE_GLASS ) ||
                                 ( mesh.flags & RG_MESH_FORCE_MIRROR ) ||
                                 ( mesh.flags & RG_MESH_FORCE_WATER );

        const char* ltpLayer1Name = nullptr;
        const char* ltpLayer2Name = nullptr;
        uint8_t     ltpLayer1R = 150, ltpLayer1G = 180, ltpLayer1B = 150, ltpLayer1A = 72;
        uint8_t     ltpLayer2R = 150, ltpLayer2G = 180, ltpLayer2B = 150, ltpLayer2A = 150;
        switch( ltpKind )
        {
            case LtpLiquidKind::Water:
                ltpLayer1Name = "rt_ltp_water_spec";
                ltpLayer1R = 145; ltpLayer1G = 170; ltpLayer1B = 190; ltpLayer1A = 60;
                break;
            case LtpLiquidKind::Blood:
                ltpLayer1Name = "rt_ltp_blood_spec";
                ltpLayer1R = 155; ltpLayer1G = 65; ltpLayer1B = 55; ltpLayer1A = 56;
                break;
            case LtpLiquidKind::Slime:
                ltpLayer1Name = "rt_ltp_slime_spec";
                ltpLayer1R = 125; ltpLayer1G = 145; ltpLayer1B = 75; ltpLayer1A = 54;
                break;
            case LtpLiquidKind::Toxic:
            case LtpLiquidKind::Lava:
                // These are complete animated LTP material frames.  Do not expose their
                // source mask/detail images as RTGL layers as that recreates grey/black blobs.
                break;
            case LtpLiquidKind::WaterFall:
            case LtpLiquidKind::BloodFall:
            case LtpLiquidKind::SlimeFall:
                ltpLayer1Name = "rt_ltp_fall_foam";
                ltpLayer1R = ltpKind == LtpLiquidKind::BloodFall ? 145 : 155;
                ltpLayer1G = ltpKind == LtpLiquidKind::BloodFall ? 65 : 175;
                ltpLayer1B = ltpKind == LtpLiquidKind::SlimeFall ? 80 : 180;
                ltpLayer1A = 70;
                break;
            case LtpLiquidKind::ToxicFall:
                ltpLayer1Name = "rt_ltp_toxic_layer";
                ltpLayer2Name = "rt_ltp_toxic_fall_mask";
                ltpLayer1R = 145; ltpLayer1G = 185; ltpLayer1B = 140; ltpLayer1A = 76;
                break;
            case LtpLiquidKind::LavaFall:
                ltpLayer1Name = "rt_ltp_lava_slag";
                ltpLayer2Name = "rt_ltp_lava_fall_mask";
                ltpLayer1R = 185; ltpLayer1G = 95; ltpLayer1B = 38; ltpLayer1A = 64;
                ltpLayer2R = 165; ltpLayer2G = 105; ltpLayer2B = 65; ltpLayer2A = 130;
                break;
            default: break;
        }

        auto ltpDetailLayer = RgTextureLayer{
            .pTexCoord    = ltpDetailCoords.empty() ? nullptr : ltpDetailCoords.data(),
            .pTextureName = ltpLayer1Name,
            .blend        = RG_TEXTURE_LAYER_BLEND_TYPE_ADD,
            .color        = rt.rgUtilPackColorByte4D( ltpLayer1R, ltpLayer1G, ltpLayer1B,
                                                      ltpLayer1A ),
        };
        auto ltpMaskLayer = RgTextureLayer{
            .pTexCoord    = ltpMaskCoords.empty() ? nullptr : ltpMaskCoords.data(),
            .pTextureName = ltpLayer2Name,
            .blend        = RG_TEXTURE_LAYER_BLEND_TYPE_SHADE,
            .color        = rt.rgUtilPackColorByte4D( ltpLayer2R, ltpLayer2G, ltpLayer2B,
                                                      ltpLayer2A ),
        };
        auto ltpPbr = RgMeshPrimitivePBREXT{
            .sType            = RG_STRUCTURE_TYPE_MESH_PRIMITIVE_PBR_EXT,
            .pNext            = nullptr,
            .metallicDefault  = 0.0f,
            .roughnessDefault = ltpKind == LtpLiquidKind::Lava ? 0.82f
                                : ltpKind == LtpLiquidKind::Toxic ? 0.42f
                                : isLtpFall ? 0.34f : 0.18f,
        };
        auto ltpLayers = RgMeshPrimitiveTextureLayersEXT{
            .sType          = RG_STRUCTURE_TYPE_MESH_PRIMITIVE_TEXTURE_LAYERS_EXT,
            .pNext          = &ltpPbr,
            .baseLayerBlend = RG_TEXTURE_LAYER_BLEND_TYPE_OPAQUE,
            .pLayer1        = ltpLayer1Name ? &ltpDetailLayer : nullptr,
            .pLayer2        = ltpLayer2Name ? &ltpMaskLayer : nullptr,
            // RTGL 1.6.3's layer-3 CPU path contains a null-layer dereference.
            // Toxic and lava derive their smooth displacement from their own
            // distinct masks in HitInfo.inl, so keep this unsafe slot unused.
            .pLayer3        = nullptr,
        };

        auto prim = RgMeshPrimitiveInfo{
            .sType = RG_STRUCTURE_TYPE_MESH_PRIMITIVE_INFO,
            .pNext = isLtpLiquid ? static_cast< void* >( &ltpLayers )
                                : isUI ? static_cast< void* >( &ui ) : nullptr,
            .flags = makePrimFlags( isUI ) | RG_MESH_PRIMITIVE_FORCE_EXACT_NORMALS |
                     ( rtstate.is< RtPrim::ExportInvertNormals >()
                           ? RG_MESH_PRIMITIVE_EXPORT_INVERT_NORMALS
                           : 0 ),
            .primitiveIndexInMesh = rtstate.next_primitiveindex(),
            .pVertices            = verts.data(),
            .vertexCount          = static_cast< uint32_t >( verts.size() ),
            .pIndices             = indices.empty() ? nullptr : indices.data(),
            .indexCount           = static_cast< uint32_t >( indices.size() ),
            .pTextureName         = texname,
            .textureFrame         = 0,
            .color =
                rtcolor_multiply( mStreamData.uObjectColor, mStreamData.uVertexColor, forcealpha1 ),
            .emissive = ltpKind == LtpLiquidKind::Lava || ltpKind == LtpLiquidKind::LavaFall
                            ? 0.08f
                            : ( mRenderStyle.BlendOp == STYLEOP_Add &&
                                mRenderStyle.DestAlpha == STYLEALPHA_One )
                                  ? cvar::rt_emis_additive_dflt
                                  : 0.f,
            .classicLight = lightlevel_to_classic( isUI, mLightParms[ 3 ] ),
        };

#ifndef NDEBUG
        if( cvar::_rt_showexportable )
        {
            if( !rtstate.is< RtPrim::ExportMap >() && !isUI )
            {
                return;
            }
        }
#endif

        RgResult r = rt.rgUploadMeshPrimitive( &mesh, &prim );
        RG_CHECK( r );

        // Actor replacements in weapon packs use their own projectile classes, so the
        // bundled Rocket/PlasmaBall replacements (and their lights) never run. Attach a
        // small RT light to the recognizable custom projectile sprites instead. This is
        // deliberately renderer-only: the mod keeps full ownership of projectile behavior.
        if( Args != nullptr && Args->CheckParm( "-rtweaponcompat" ) && texname != nullptr &&
            !rtstate.is< RtPrim::FirstPerson >() && !rtstate.is< RtPrim::FirstPersonViewer >() )
        {
            const bool isRocket = strnicmp( texname, "MSLE", 4 ) == 0;
            const bool isPlasma = strnicmp( texname, "PLZS", 4 ) == 0;
            if( isRocket || isPlasma )
            {
                const auto projectileColor = isRocket
                    ? rt.rgUtilPackColorByte4D( 255, 108, 35, 255 )
                    : rt.rgUtilPackColorByte4D( 95, 145, 255, 255 );
                auto projectileSphere = RgLightSphericalEXT{
                    .sType     = RG_STRUCTURE_TYPE_LIGHT_SPHERICAL_EXT,
                    .pNext     = nullptr,
                    .color     = projectileColor,
                    .intensity = isRocket ? 480.0f : 300.0f,
                    .position  = {
                        float( rtstate.m_lastthingposition.X ) * ONEGAMEUNIT_IN_METERS,
                        float( rtstate.m_lastthingposition.Y ) * ONEGAMEUNIT_IN_METERS,
                        float( rtstate.m_lastthingposition.Z ) * ONEGAMEUNIT_IN_METERS + 0.08f,
                    },
                    .radius = isRocket ? 0.14f : 0.10f,
                };
                auto projectileLight = RgLightInfo{
                    .sType        = RG_STRUCTURE_TYPE_LIGHT_INFO,
                    .pNext        = &projectileSphere,
                    .uniqueID     = rtstate.get_uniqueid() ^ WeaponCompatProjectileLightSalt,
                    .isExportable = false,
                };
                r = rt.rgUploadLight( &projectileLight );
                RG_CHECK( r );
            }
        }
    }

public:
    void RT_SetMatrices( const VSMatrix& view, const VSMatrix& proj )
    {
        // TODO: only calculate when UI mode;
        //       can those UI elements be with perspective matrix?

        // clang-format off
        constexpr static float vkcorrection[] = {
            1,  0,    0, 0,
            0, -1,    0, 0,
            0,  0, 0.5f, 0,
            0,  0, 0.5f, 1,
        };
        // clang-format on

        auto correctedProj = VSMatrix::smultMatrix( vkcorrection, proj.get() );
        memcpy( m_projection, correctedProj.get(), sizeof( float ) * 16 );
        memcpy( m_view, view.get(), sizeof( float ) * 16 );
    }

    void RT_AddMainCamera( const FRenderViewpoint& viewpoint )
    {
        const auto [ up, right, forward ] = RT_MakeUpRightForwardVectors( viewpoint.Angles );

        const float pixelstretch =
            viewpoint.ViewLevel ? viewpoint.ViewLevel->info->pixelstretch : 1.0f;

        const auto aspectRatio = r_viewwindow.WidescreenRatio;
        const auto fovRatio    = r_viewwindow.WidescreenRatio >= 1.3f ? 1.333333f : aspectRatio;

        const auto fovy = static_cast< float >(
            2.0 * std::atan( std::tan( viewpoint.FieldOfView.Radians() / 2.0 ) /
                             static_cast< double >( fovRatio ) ) );


        auto readback = RgCameraInfoReadbackEXT{
            .sType = RG_STRUCTURE_TYPE_CAMERA_INFO_READ_BACK_EXT,
        };

        auto info = RgCameraInfo{
            .sType       = RG_STRUCTURE_TYPE_CAMERA_INFO,
            .pNext       = &readback,
            .flags       = 0,
            .position    = { float( viewpoint.Pos.X ) * ONEGAMEUNIT_IN_METERS,
                             float( viewpoint.Pos.Y ) * ONEGAMEUNIT_IN_METERS,
                             float( viewpoint.Pos.Z ) * ONEGAMEUNIT_IN_METERS },
            .up          = up,
            .right       = right,
            .fovYRadians = fovy,
            .aspect      = aspectRatio * pixelstretch,
            .cameraNear  = cvar::rt_znear,
            .cameraFar   = cvar::rt_zfar,
        };

        g_rt_mainCameraPosition = info.position;
        g_rt_mainCameraUp       = info.up;
        g_rt_mainCameraRight    = info.right;
        g_rt_mainCameraValid    = true;

        RgResult r = rt.rgUploadCamera( &info );
        RG_CHECK( r );


        // for first-person weapons
        memcpy( m_mainCameraView_Inverse, readback.viewInverse, 16 * sizeof( float ) );
        memcpy( m_mainCameraProjection_Inverse, readback.projectionInverse, 16 * sizeof( float ) );
        static_assert( sizeof m_mainCameraView_Inverse == sizeof readback.viewInverse );
        static_assert( sizeof m_mainCameraProjection_Inverse == sizeof readback.projectionInverse );


        RT_AddFlashlight( info.position, forward, up, right );
        RT_AddMuzzleFlash( viewpoint.ViewActor, viewpoint.extralight, info.position, forward, up );
    }

    void RT_AddFlashlight( const RgFloat3D& basePosition,
                           const RgFloat3D& forward,
                           const RgFloat3D& up,
                           const RgFloat3D& right )
    {
        auto enabled = []() {
            if( cvar::rt_pw_lightamp == 2 )
            {
                if( RT_CalcPowerupFlags() & RT_POWERUP_FLAG_FLASHLIGHT_BIT )
                {
                    return true;
                }
            }
            if( cvar::rt_flsh )
            {
                return true;
            }
            return false;
        };

        if( !enabled() )
        {
            return;
        }

        auto pos = gzvec3( basePosition );
        {
            pos += gzvec3( up ) * cvar::rt_flsh_u;
            pos += gzvec3( right ) * cvar::rt_flsh_r;
            pos += gzvec3( forward ) * cvar::rt_flsh_f;
        }

        auto target = gzvec3( basePosition ) + 20 * gzvec3( forward );
        auto dir    = ( target - pos ).Unit();

        auto flashlightAdditional = RgLightAdditionalEXT{
            .sType      = RG_STRUCTURE_TYPE_LIGHT_ADDITIONAL_EXT,
            .pNext      = nullptr,
            .flags      = bool{ cvar::rt_flsh_volumetric }
                              ? RG_LIGHT_ADDITIONAL_VOLUMETRIC
                              : RgLightAdditionalFlags{},
            .lightstyle = 0,
            .hashName   = "",
        };

        auto spot = RgLightSpotEXT{
            .sType      = RG_STRUCTURE_TYPE_LIGHT_SPOT_EXT,
            .pNext      = &flashlightAdditional,
            .color      = RG_PACKED_COLOR_WHITE,
            .intensity  = cvar::rt_flsh_intensity,
            .position   = { pos.X, pos.Y, pos.Z },
            .direction  = { dir.X, dir.Y, dir.Z },
            .radius     = cvar::rt_flsh_radius,
            .angleOuter = to_rad( cvar::rt_flsh_angle ),
            .angleInner = 0,
        };

        auto light = RgLightInfo{
            .sType        = RG_STRUCTURE_TYPE_LIGHT_INFO,
            .pNext        = &spot,
            .uniqueID     = FlashlightLightId,
            .isExportable = false,
        };

        RgResult r = rt.rgUploadLight( &light );
        RG_CHECK( r );
    }

    void RT_AddMuzzleFlash( AActor*          viewactor,
                            int              extralight,
                            const RgFloat3D& basePosition,
                            const RgFloat3D& forward,
                            const RgFloat3D& up )
    {
        if( extralight <= 0 || !cvar::rt_mzlflsh || !viewactor || !viewactor->Sector )
        {
            return;
        }

        auto desiredPos = gzvec3( basePosition );
        {
            desiredPos += gzvec3( up ) * cvar::rt_mzlflsh_u;
            desiredPos += gzvec3( forward ) * cvar::rt_mzlflsh_f;
        }

        FVector3 pos;
        {
            // metric to game units
            auto units_desiredPos   = DVector3{ desiredPos } / double{ ONEGAMEUNIT_IN_METERS };
            auto units_basePosition = gzvec3d( basePosition ) / double{ ONEGAMEUNIT_IN_METERS };

            auto dir = units_desiredPos - units_basePosition;
            auto len = dir.Length();

            if( len > 0.01 )
            {
                dir /= len;

                float hitT = 1.0f;

                FTraceResults trace;
                if( Trace( units_basePosition,
                           viewactor->Sector,
                           dir,
                           len,
                           0,
                           0,
                           viewactor,
                           trace,
                           TRACE_NoSky ) )
                {
                    if( trace.HitType != TRACE_HitNone )
                    {
                        hitT = float( ( trace.HitPos - units_basePosition ).Length() / len );
                        // hit point must be between base and desired positions
                        assert( hitT >= 0 && hitT <= 1 );
                    }
                }

                hitT *= std::clamp( float( cvar::rt_mzlflsh_offset ), 0.0f, 1.0f );

                // lerp
                pos = gzvec3( basePosition ) + hitT * ( desiredPos - gzvec3( basePosition ) );
            }
            else
            {
                pos = gzvec3( basePosition );
            }
        }

        auto sph = RgLightSphericalEXT{
            .sType     = RG_STRUCTURE_TYPE_LIGHT_SPHERICAL_EXT,
            .pNext     = nullptr,
            .color     = cvarcolor_to_rtcolor( cvar::rt_mzlflsh_color ),
            .intensity = cvar::rt_mzlflsh_intensity,
            .position  = { pos.X, pos.Y, pos.Z },
            .radius    = cvar::rt_mzlflsh_radius,
        };

        auto light = RgLightInfo{
            .sType        = RG_STRUCTURE_TYPE_LIGHT_INFO,
            .pNext        = &sph,
            .uniqueID     = MuzzleFlashLightId,
            .isExportable = false,
        };

        RgResult r = rt.rgUploadLight( &light );
        RG_CHECK( r );
    }

private:
    RgViewport m_viewport{};
    float      m_view[ 16 ]{};
    float      m_projection[ 16 ]{};

    float m_mainCameraView_Inverse[ 16 ]{};
    float m_mainCameraProjection_Inverse[ 16 ]{};

    uint32_t m_weaponDrawCallIndex{ 0 }; // to z-sort weapon sprites

    std::vector< RgPrimitiveVertex > m_tempverts{};

public:
    RTFrameBuffer* m_fb{ nullptr };
};



class RTDataBuffer
    : public IDataBuffer
    , public VectorAsBuffer
{
    void BindRange( FRenderState* state, size_t start, size_t length ) override
    {
        auto hwstate = static_cast< RTRenderState* >( state );

        // ugly way to fetch viewpoint info
        if( this == hwstate->m_fb->mViewpoints->DataBuffer() )
        {
            const HWViewpointUniforms& vp = hwstate->m_fb->mViewpoints->FetchViewpoint( start );
            hwstate->RT_SetMatrices( vp.mViewMatrix, vp.mProjectionMatrix );
        }
    }
};



void RT_Print( const char* pMessage, RgMessageSeverityFlags flags, void* pUserData )
{
    if( !pMessage )
    {
        DPrintf( DMSG_ERROR, "RT_Print: pMessage is NULL\n" );
        return;
    }

    if( flags & RG_MESSAGE_SEVERITY_ERROR )
    {
        DPrintf( DMSG_ERROR, "%s\n", pMessage );

#ifdef WIN32
        static bool g_breakOnError = true;
        if( g_breakOnError )
        {
            auto msg = std::string_view{ pMessage };
            auto str = std::format( "{}{}\n"
                                    "\n\'Abort\' to exit the game."
                                    "\n\'Retry\' to skip only this error message."
                                    "\n\'Ignore\' to ignore all such error messages.",
                                    msg,
                                    msg.ends_with( '.' ) ? "" : "." );

            int ok = MessageBoxA( nullptr,
                                  str.c_str(), // null-terminated
                                  "Renderer Error",
                                  MB_ABORTRETRYIGNORE | MB_DEFBUTTON2 | MB_ICONERROR );
            switch( ok )
            {
                case IDIGNORE: g_breakOnError = false; break;
                case IDRETRY: break;
                case IDABORT:
                default: exit( -1 );
            }
        }
#endif
    }
    else if( flags & RG_MESSAGE_SEVERITY_WARNING )
    {
        DPrintf( DMSG_WARNING, "%s\n", pMessage );
    }
    else if( flags & RG_MESSAGE_SEVERITY_INFO )
    {
        DPrintf( DMSG_NOTIFY, "%s\n", pMessage );
    }
    else
    {
        DPrintf( DMSG_SPAMMY, "%s\n", pMessage );
    }
}

} // anonymous namespace

#ifdef _WIN32
std::atomic< HWND > g_msgbox_parent{};
#endif



//
//
//
//
//
//



RG_D3D12CORE_HELPER( "rt/" )

Win32RTVideo::Win32RTVideo()
{
    extern std::atomic_bool g_continueMain;
    extern std::atomic_bool g_forceLnchThreadStop;
    while( !g_continueMain )
    {
    }
    if( g_forceLnchThreadStop.load() )
    {
        exit( 1 );
    }

    // warn if no needed dll-s
    if( !Args->CheckParm( "-nodllcheck" ) )
    {
        enum rt_feature_flag_t
        {
            RT_FEATURE_FSR2     = 1,
            RT_FEATURE_FSR3_FG  = 2,
            RT_FEATURE_DLSS2    = 4,
            RT_FEATURE_DLSS3_FG = 8,
        };

        const std::pair< std::filesystem::path, int > dlls[] = {
            { "rt/bin/D3D12Core.dll", RT_FEATURE_FSR3_FG | RT_FEATURE_DLSS3_FG },
            { "rt/bin/nvngx_dlss.dll", RT_FEATURE_DLSS2 },
            { "rt/bin/nvngx_dlssg.dll", RT_FEATURE_DLSS3_FG },
            { "rt/bin/NvLowLatencyVk.dll", RT_FEATURE_DLSS3_FG },
            { "rt/bin/sl.dlss.dll", RT_FEATURE_DLSS3_FG },
            { "rt/bin/sl.dlss_g.dll", RT_FEATURE_DLSS3_FG },
            { "rt/bin/sl.reflex.dll", RT_FEATURE_DLSS3_FG },
            { "rt/bin/sl.pcl.dll", RT_FEATURE_DLSS3_FG },
            { "rt/bin/sl.common.dll", RT_FEATURE_DLSS3_FG },
            { "rt/bin/sl.interposer.dll", RT_FEATURE_DLSS3_FG },
            { "rt/bin/ffx_fsr2_x64.dll", RT_FEATURE_FSR2 },
            { "rt/bin/ffx_fsr3_x64.dll", RT_FEATURE_FSR3_FG },
            { "rt/bin/ffx_fsr3upscaler_x64.dll", RT_FEATURE_FSR3_FG },
            { "rt/bin/ffx_frameinterpolation_x64.dll", RT_FEATURE_FSR3_FG },
            { "rt/bin/ffx_opticalflow_x64.dll", RT_FEATURE_FSR3_FG },
            { "rt/bin/ffx_backend_dx12_x64.dll", RT_FEATURE_FSR3_FG },
            { "rt/bin/ffx_backend_vk_x64.dll", RT_FEATURE_FSR2 | RT_FEATURE_FSR3_FG },
        };

        auto failedPaths    = std::string{};
        int  failedFeatures = 0;
        for( const auto& [ dll, feature ] : dlls )
        {
            if( !exists( dll ) )
            {
                failedPaths += "    " + dll.filename().string() + '\n';
                failedFeatures |= feature;
            }
        }

        if( !failedPaths.empty() )
        {
            auto msg = std::string{};

            if( failedFeatures == 0 )
            {
                msg = "Some features will NOT be available!";
            }
            else
            {
                // clang-format off
                if( failedFeatures & RT_FEATURE_DLSS3_FG) msg += "NVIDIA DLSS3 (AI Frame Generation)\n";
                if( failedFeatures & RT_FEATURE_DLSS2   ) msg += "NVIDIA DLSS2 (AI Upscaling)\n";
                if( failedFeatures & RT_FEATURE_FSR3_FG ) msg += "AMD FSR 3 (Frame Generation)\n";
                if( failedFeatures & RT_FEATURE_FSR2    ) msg += "AMD FSR 2 (Upscaling)\n";
                // clang-format on
                msg += "                                   will NOT be available!\n";
            }

            msg += "Reason: \'rt/bin/\' folder doesn't contain:\n";
            msg += failedPaths;
            // msg += "\n(To suppress this warning, use \'-nodllcheck\' argument)";
            msg += "\n\nDo you want to download the missing files?\n";
            msg += "\nYES - open renderer's Download page";
            msg += "\nNO  - proceed with a limited feature set";
            
            int l = MessageBoxA( g_msgbox_parent.load(),
                                 msg.c_str(),
                                 "DLL check failure",
                                 MB_ICONEXCLAMATION | MB_YESNO );
            if( l == IDYES )
            {
                ShellExecute(
                    nullptr, 0, L"https://github.com/vs-shirokii/RTGL/releases", 0, 0, SW_SHOW );
                exit( -1 );
            }
        }
    }

    rt = RgInterface{};

#ifdef WIN32
    auto win32Info = RgWin32SurfaceCreateInfo{
        .hinstance = GetModuleHandle( NULL ),
        .hwnd      = mainwindow.GetHandle(),
    };
#else
    RgXlibSurfaceCreateInfo x11Info = { .dpy    = wmInfo.info.x11.display,
                                        .window = wmInfo.info.x11.window };
#endif

    auto info = RgInstanceCreateInfo
    {
        .sType = RG_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pNext = NULL,

        .version = RG_RTGL_VERSION_API, .sizeOfRgInterface = sizeof( RgInterface ),

        .pAppName = "GZDoom", .pAppGUID = "8cbd354f-38d3-4173-92b9-c16b5a210b37",

#if WIN32
        .pWin32SurfaceInfo = &win32Info,
#else
        .pXlibSurfaceCreateInfo  = &x11Info,
#endif

        .pOverrideFolderPath = "rt/",

        .pfnPrint = RT_Print, .pUserPrintData = nullptr,
        .allowedMessages =
            Args->CheckParm( "-rtdebug" )
                ? RgMessageSeverityFlags{ RG_MESSAGE_SEVERITY_VERBOSE | RG_MESSAGE_SEVERITY_INFO |
                                          RG_MESSAGE_SEVERITY_WARNING | RG_MESSAGE_SEVERITY_ERROR }
                : RgMessageSeverityFlags{ 0 },

        .primaryRaysMaxAlbedoLayers = 3, .indirectIlluminationMaxAlbedoLayers = 1,

        .replacementsMaxVertexCount = 32 * 1024 * 1024, .dynamicMaxVertexCount = 2 * 1024 * 1024,

        .rayCullBackFacingTriangles = 0,
        .allowTexCoordLayer1 = true, .allowTexCoordLayer2 = true, .allowTexCoordLayer3 = false,

        .lightmapTexCoordLayerIndex = 1,

        .rasterizedMaxVertexCount = 1 << 20, .rasterizedMaxIndexCount = 1 << 21,
        .rasterizedVertexColorGamma = true,

        .rasterizedSkyCubemapSize = 256,

        .textureSamplerForceMinificationFilterLinear = true,
        .textureSamplerForceNormalMapFilterLinear    = true,

        .pbrTextureSwizzling = RG_TEXTURE_SWIZZLING_NULL_ROUGHNESS_METALLIC,

        .effectWipeIsUsed = true,

        .worldUp = { 0, 0, 1 }, .worldForward = { 0, 1, 0 }, .worldScale = 1.0f,

        .importedLightIntensityScaleDirectional = 1.0f / 50,
        .importedLightIntensityScaleSphere      = 1.0f / 500,
        .importedLightIntensityScaleSpot        = 1.0f / 500,
    };

#ifndef NDEBUG
    constexpr bool isdebug = true;
#else
    constexpr bool isdebug = false;
#endif

    const char* remixdll = g_isremix ? "\\bin_remix\\RTGL1.dll" : nullptr;

    RgResult r = rgLoadLibraryAndCreate( &info, isdebug, remixdll, &rt, nullptr );
    if( r != RG_RESULT_SUCCESS )
    {
        auto msg = std::string{ "RgResult code: " };

        switch( r )
        {
            case RG_RESULT_CANT_FIND_DYNAMIC_LIBRARY:
                msg = remixdll  ? "Can't load Remix Renderer DLLs"
                      : isdebug ? "Can't find \'rt/bin/debug/RTGL1.dll\' file"
                                : "Can't find \'rt/bin/RTGL1.dll\' file";
                break;
            case RG_RESULT_CANT_FIND_ENTRY_FUNCTION_IN_DYNAMIC_LIBRARY:
                msg =
                    remixdll  ? "Can't find rgCreateInstance function in Remix Renderer wrapper DLL"
                    : isdebug ? "Can't find rgCreateInstance function in \'rt/bin/debug/RTGL1.dll\'"
                              : "Can't find rgCreateInstance function in \'rt/bin/RTGL1.dll\'";
                break;

            // clang-format off
            case RG_RESULT_NOT_INITIALIZED:                     msg += "RG_RESULT_NOT_INITIALIZED";                     break;
            case RG_RESULT_ALREADY_INITIALIZED:                 msg += "RG_RESULT_ALREADY_INITIALIZED";                 break;
            case RG_RESULT_GRAPHICS_API_ERROR:                  msg += "RG_RESULT_GRAPHICS_API_ERROR";                  break;
            case RG_RESULT_INTERNAL_ERROR:                      msg += "RG_RESULT_INTERNAL_ERROR";                      break;
            case RG_RESULT_CANT_FIND_SUPPORTED_PHYSICAL_DEVICE: msg += "RG_RESULT_CANT_FIND_SUPPORTED_PHYSICAL_DEVICE"; break;
            case RG_RESULT_FRAME_WASNT_STARTED:                 msg += "RG_RESULT_FRAME_WASNT_STARTED";                 break;
            case RG_RESULT_FRAME_WASNT_ENDED:                   msg += "RG_RESULT_FRAME_WASNT_ENDED";                   break;
            case RG_RESULT_WRONG_FUNCTION_CALL:                 msg += "RG_RESULT_WRONG_FUNCTION_CALL";                 break;
            case RG_RESULT_WRONG_FUNCTION_ARGUMENT:             msg += "RG_RESULT_WRONG_FUNCTION_ARGUMENT";             break;
            case RG_RESULT_WRONG_STRUCTURE_TYPE:                msg += "RG_RESULT_WRONG_STRUCTURE_TYPE";                break;
            case RG_RESULT_ERROR_CANT_FIND_HARDCODED_RESOURCES: msg += "RG_RESULT_ERROR_CANT_FIND_HARDCODED_RESOURCES"; break;
            case RG_RESULT_ERROR_CANT_FIND_SHADER:              msg += "RG_RESULT_ERROR_CANT_FIND_SHADER";              break;
            case RG_RESULT_ERROR_MEMORY_ALIGNMENT:              msg += "RG_RESULT_ERROR_MEMORY_ALIGNMENT";              break;
            case RG_RESULT_ERROR_NO_VULKAN_EXTENSION:           msg += "RG_RESULT_ERROR_NO_VULKAN_EXTENSION";           break;
                // clang-format on

            default: msg += std::to_string( r ); break;
        }

        MessageBoxA(
            nullptr, msg.c_str(), "Failed to initialize RT renderer", MB_ICONEXCLAMATION | MB_OK );
        exit( -1 );
    }

    // on first start, try to set DLSS, if available
    if( cvar::rt_firststart )
    {
        if( rt.rgUtilIsUpscaleTechniqueAvailable( RG_RENDER_UPSCALE_TECHNIQUE_NVIDIA_DLSS, //
                                                  RG_FRAME_GENERATION_MODE_OFF,
                                                  nullptr ) )
        {
            cvar::rt_upscale_dlss = 2;
            cvar::rt_upscale_fsr2 = 0;
            cvar::rt_remix_taa    = 0;
            cvar::rt_ef_vintage   = 0;
        }
        else if( rt.rgUtilIsUpscaleTechniqueAvailable( RG_RENDER_UPSCALE_TECHNIQUE_AMD_FSR2, //
                                                       RG_FRAME_GENERATION_MODE_OFF,
                                                       nullptr ) )
        {
            cvar::rt_upscale_dlss = 0;
            cvar::rt_upscale_fsr2 = 2;
            cvar::rt_remix_taa    = 0;
            cvar::rt_ef_vintage   = 0;
        }
        else
        {
            cvar::rt_upscale_dlss = 0;
            cvar::rt_upscale_fsr2 = 0;
            cvar::rt_remix_taa    = g_isremix ? 2 : 0;
            cvar::rt_ef_vintage   = g_isremix ? 0 : RT_VINTAGE_480_DITHER;
        }
    }
    else
    {
        if( g_isremix )
        {
            if( cvar::rt_upscale_dlss == 0 && //
                cvar::rt_upscale_fsr2 > 0 &&  //
                cvar::rt_remix_taa == 0 &&    //
                cvar::rt_ef_vintage == 0 )
            {
                cvar::rt_remix_taa = cvar::rt_upscale_fsr2;
            }
            cvar::rt_upscale_fsr2 = 0;
            cvar::rt_ef_vintage   = 0;
        }
        else
        {
            if( cvar::rt_upscale_dlss == 0 && //
                cvar::rt_upscale_fsr2 == 0 &&  //
                cvar::rt_remix_taa > 0 &&    //
                cvar::rt_ef_vintage == 0 )
            {
                cvar::rt_upscale_fsr2 = cvar::rt_remix_taa;
            }
            cvar::rt_remix_taa = 0;
        }
    }
}

DFrameBuffer* Win32RTVideo::CreateFrameBuffer()
{
    return new RTFrameBuffer{ m_hMonitor, vid_fullscreen };
}

void Win32RTVideo::Shutdown()
{
    if( !rt.rgDestroyInstance )
    {
        return;
    }

    RgResult r = rt.rgDestroyInstance();
    if( r != RG_RESULT_SUCCESS )
    {
        MessageBoxA(
            nullptr, "rgDestroyAndUnloadLibrary has failed", "Fail", MB_ICONEXCLAMATION | MB_OK );
        exit( -1 );
    }

    rt = {};
}

void RT_ShowWarningMessageBox( const char* msg )
{
#ifdef _WIN32
    MessageBoxA( g_msgbox_parent.load(), msg, "Warning - Ray Tracing", MB_ICONEXCLAMATION | MB_OK );
#else
    assert( 0 );
#endif
}

bool RT_AskToOpenUrl( const char* heading, const char* msg, const wchar_t* url )
{
    int l = MessageBoxA( g_msgbox_parent.load(), msg, heading, MB_ICONEXCLAMATION | MB_YESNO );
    if( l == IDYES )
    {
        ShellExecute( nullptr, 0, url, 0, 0, SW_SHOW );
        return true;
    }
    return false;
}

//
//
//

auto RT_GetCurrentTime() -> double
{
    auto ns = []() {
        using namespace std::chrono;
        return duration_cast< nanoseconds >( steady_clock::now().time_since_epoch() ).count();
    };

    static int64_t startupTimeNS = ns();
    return static_cast< double >( ns() - startupTimeNS ) / 1000000000.0;
}

auto RT_GetVramUsage( bool* ok ) -> const char*
{
    const RgUtilMemoryUsage vram = rt.rgUtilRequestMemoryUsage();

    if( ok )
    {
        // < 80% is ok
        *ok = ( vram.vramUsed <= 0.8 * vram.vramTotal );
    }

    static char buf[ 64 ];
    snprintf( buf,
              std::size( buf ),
              "%d / %d MB",
              int( std::round( double( vram.vramUsed ) / 1024 / 1024 ) ),
              int( std::round( double( vram.vramTotal ) / 1024 / 1024 ) ) );

    buf[ std::size( buf ) - 1 ] = '\0';
    return buf;
}

namespace
{

RgExtent2D RT_GetCurrentWindowSize()
{
    return {
        static_cast< uint32_t >( screen->GetWidth() ),
        static_cast< uint32_t >( screen->GetHeight() ),
    };
}

void RT_ResolutionToRtgl( RgStartFrameRenderResolutionParams* dst, const RgExtent2D winsize )
{
    const auto aspect =
        static_cast< double >( winsize.width ) / static_cast< double >( winsize.height );

    if( cvar::rt_renderscale > 0.2f )
    {
        auto scale = std::clamp( double( *cvar::rt_renderscale ), 0.2, 1.0 );

        dst->customRenderSize.width    = static_cast< uint32_t >( winsize.width * scale );
        dst->customRenderSize.height   = static_cast< uint32_t >( winsize.height * scale );
        dst->pixelizedRenderSizeEnable = false;

        return;
    }
    else
    {
        if( int{ cvar::rt_ef_vintage } != RT_VINTAGE_OFF )
        {
            uint32_t h_pixelized = 0;
            uint32_t h_render    = 0;

            switch( int{ cvar::rt_ef_vintage } )
            {
                case RT_VINTAGE_200:
                case RT_VINTAGE_200_DITHER:
                    h_pixelized = 200;
                    h_render    = 400;
                    break;

                case RT_VINTAGE_480:
                case RT_VINTAGE_480_DITHER:
                    h_pixelized = 480;
                    h_render    = 600;
                    break;

                case RT_VINTAGE_CRT:
                case RT_VINTAGE_VHS:
                case RT_VINTAGE_VHS_CRT:
                    h_pixelized = 480;
                    h_render    = 480;
                    break;

                default:
                    cvar::rt_ef_vintage            = 0;
                    dst->customRenderSize          = winsize;
                    dst->pixelizedRenderSizeEnable = false;
                    return;
            }

            assert( h_render > 0 && h_pixelized > 0 );

            dst->pixelizedRenderSize.height = h_pixelized;
            dst->pixelizedRenderSize.width  = static_cast< uint32_t >( h_pixelized * aspect );
            dst->pixelizedRenderSizeEnable  = true;
            dst->customRenderSize.height    = h_render;
            dst->customRenderSize.width     = static_cast< uint32_t >( h_render * aspect );

            return;
        }
    }

    dst->customRenderSize          = winsize;
    dst->pixelizedRenderSizeEnable = false;
}

auto RT_GetSharpenTechniqueFromCvar( bool dlssOrFsr2 ) -> RgRenderSharpenTechnique
{
    switch( cvar::rt_sharpen )
    {
        case 3: return RG_RENDER_SHARPEN_TECHNIQUE_NONE;
        case 2: return RG_RENDER_SHARPEN_TECHNIQUE_AMD_CAS;
        case 1: return RG_RENDER_SHARPEN_TECHNIQUE_NAIVE;
        default: {
            if( dlssOrFsr2 )
            {
                return RG_RENDER_SHARPEN_TECHNIQUE_AMD_CAS;
            }
            // to accentuate a chunky look, because of the linear (not nearest) downscale mode
            switch( cvar::rt_ef_vintage )
            {
                case RT_VINTAGE_CRT:
                case RT_VINTAGE_VHS:
                case RT_VINTAGE_VHS_CRT: return RG_RENDER_SHARPEN_TECHNIQUE_NAIVE;
                case RT_VINTAGE_200:
                case RT_VINTAGE_200_DITHER:
                case RT_VINTAGE_480:
                case RT_VINTAGE_480_DITHER: return RG_RENDER_SHARPEN_TECHNIQUE_AMD_CAS;
                default: return RG_RENDER_SHARPEN_TECHNIQUE_NONE;
            }
        }
    }
}

void RT_UpscaleCvarsToRtgl( RgStartFrameRenderResolutionParams* pDst )
{
    cvar::rt_available_dlss2 =
        rt.rgUtilIsUpscaleTechniqueAvailable( RG_RENDER_UPSCALE_TECHNIQUE_NVIDIA_DLSS,
                                              RG_FRAME_GENERATION_MODE_OFF,
                                              &cvar::rt_failreason_dlss2 );
    cvar::rt_available_dlss3fg =
        rt.rgUtilIsUpscaleTechniqueAvailable( RG_RENDER_UPSCALE_TECHNIQUE_NVIDIA_DLSS,
                                              RG_FRAME_GENERATION_MODE_ON,
                                              &cvar::rt_failreason_dlss3fg );
    cvar::rt_available_fsr2 =
        rt.rgUtilIsUpscaleTechniqueAvailable( RG_RENDER_UPSCALE_TECHNIQUE_AMD_FSR2,
                                              RG_FRAME_GENERATION_MODE_OFF,
                                              &cvar::rt_failreason_fsr2 );
    cvar::rt_available_fsr3fg =
        rt.rgUtilIsUpscaleTechniqueAvailable( RG_RENDER_UPSCALE_TECHNIQUE_AMD_FSR2,
                                              RG_FRAME_GENERATION_MODE_ON,
                                              &cvar::rt_failreason_fsr3fg );
    cvar::rt_available_dxgi = rt.rgUtilDXGIAvailable( &cvar::rt_failreason_dxgi );

    const RgFeatureFlags features = rt.rgUtilGetSupportedFeatures();

    cvar::rt_hdr_available   = ( features & RG_FEATURE_HDR );
    cvar::rt_fluid_available = ( features & RG_FEATURE_FLUID );

    int nvDlss = cvar::rt_available_dlss2 || cvar::rt_available_dlss3fg //
                     ? int( cvar::rt_upscale_dlss )
                     : 0;
    int amdFsr = cvar::rt_available_fsr2 || cvar::rt_available_fsr3fg //
                     ? int( cvar::rt_upscale_fsr2 )
                     : 0;

    switch( nvDlss )
    {
        case 1:
            // start with Quality
            pDst->upscaleTechnique = RG_RENDER_UPSCALE_TECHNIQUE_NVIDIA_DLSS;
            pDst->resolutionMode   = RG_RENDER_RESOLUTION_MODE_QUALITY;
            break;
        case 2:
            pDst->upscaleTechnique = RG_RENDER_UPSCALE_TECHNIQUE_NVIDIA_DLSS;
            pDst->resolutionMode   = RG_RENDER_RESOLUTION_MODE_BALANCED;
            break;
        case 3:
            pDst->upscaleTechnique = RG_RENDER_UPSCALE_TECHNIQUE_NVIDIA_DLSS;
            pDst->resolutionMode   = RG_RENDER_RESOLUTION_MODE_PERFORMANCE;
            break;
        case 4:
            pDst->upscaleTechnique = RG_RENDER_UPSCALE_TECHNIQUE_NVIDIA_DLSS;
            pDst->resolutionMode   = RG_RENDER_RESOLUTION_MODE_ULTRA_PERFORMANCE;
            break;

        case 5:
            // use DLSS with rt_renderscale
            pDst->upscaleTechnique = RG_RENDER_UPSCALE_TECHNIQUE_NVIDIA_DLSS;
            pDst->resolutionMode   = RG_RENDER_RESOLUTION_MODE_CUSTOM;
            break;

        case 6:
            pDst->upscaleTechnique = RG_RENDER_UPSCALE_TECHNIQUE_NVIDIA_DLSS;
            pDst->resolutionMode   = RG_RENDER_RESOLUTION_MODE_NATIVE_AA;
            break;

        default: nvDlss = 0; break;
    }

    switch( amdFsr )
    {
        case 1:
            pDst->upscaleTechnique = RG_RENDER_UPSCALE_TECHNIQUE_AMD_FSR2;
            pDst->resolutionMode   = RG_RENDER_RESOLUTION_MODE_QUALITY;
            break;
        case 2:
            pDst->upscaleTechnique = RG_RENDER_UPSCALE_TECHNIQUE_AMD_FSR2;
            pDst->resolutionMode   = RG_RENDER_RESOLUTION_MODE_BALANCED;
            break;
        case 3:
            pDst->upscaleTechnique = RG_RENDER_UPSCALE_TECHNIQUE_AMD_FSR2;
            pDst->resolutionMode   = RG_RENDER_RESOLUTION_MODE_PERFORMANCE;
            break;
        case 4:
            pDst->upscaleTechnique = RG_RENDER_UPSCALE_TECHNIQUE_AMD_FSR2;
            pDst->resolutionMode   = RG_RENDER_RESOLUTION_MODE_ULTRA_PERFORMANCE;
            break;

        case 5:
            // use FSR2 with rt_renderscale
            pDst->upscaleTechnique = RG_RENDER_UPSCALE_TECHNIQUE_AMD_FSR2;
            pDst->resolutionMode   = RG_RENDER_RESOLUTION_MODE_CUSTOM;
            break;

        case 6:
            pDst->upscaleTechnique = RG_RENDER_UPSCALE_TECHNIQUE_AMD_FSR2;
            pDst->resolutionMode   = RG_RENDER_RESOLUTION_MODE_NATIVE_AA;
            break;

        default: amdFsr = 0; break;
    }

    // both disabled
    if( nvDlss == 0 && amdFsr == 0 )
    {
        pDst->upscaleTechnique = RG_RENDER_UPSCALE_TECHNIQUE_NEAREST;
        pDst->resolutionMode   = RG_RENDER_RESOLUTION_MODE_CUSTOM;
        pDst->frameGeneration  = RG_FRAME_GENERATION_MODE_OFF;
    }
    else
    {
        if( ( nvDlss != 0 && cvar::rt_available_dlss3fg ) ||
            ( amdFsr != 0 && cvar::rt_available_fsr3fg ) )
        {
            switch( cvar::rt_framegen )
            {
                case -1: pDst->frameGeneration = RG_FRAME_GENERATION_MODE_WITHOUT_GENERATED; break;
                case 1: pDst->frameGeneration = RG_FRAME_GENERATION_MODE_ON; break;
                default: pDst->frameGeneration = RG_FRAME_GENERATION_MODE_OFF; break;
            }
        }
        else
        {
            pDst->frameGeneration = RG_FRAME_GENERATION_MODE_OFF;
        }
    }

    pDst->sharpenTechnique = RT_GetSharpenTechniqueFromCvar( amdFsr || nvDlss );
}

template< typename T >
    requires( std::is_same_v< T, int > )
uint32_t safe_uint( T x )
{
    return static_cast< uint32_t >( std::max< int >( x, 0 ) );
}

} // anonymous namespace

//
//
//

RTFrameBuffer::RTFrameBuffer( void* hMonitor, bool fullscreen )
    : SystemBaseFrameBuffer( hMonitor, fullscreen ), m_state{ new RTRenderState{ this } }
{
}
RTFrameBuffer::~RTFrameBuffer()
{
    delete m_state;
    delete mVertexData;
    delete mSkyData;
    delete mViewpoints;
    delete mLights;
    delete mBones;
}
void RTFrameBuffer::InitializeState()
{
    m_state      = new RTRenderState{ this };
    vendorstring = "RT";
    mVertexData  = new FFlatVertexBuffer( GetWidth(), GetHeight(), screen->mPipelineNbr );
    mSkyData     = new FSkyVertexBuffer;
    mViewpoints  = new HWViewpointBuffer( screen->mPipelineNbr );
    mLights      = new FLightBuffer( screen->mPipelineNbr );
    mBones       = new BoneBuffer( screen->mPipelineNbr );
}

void RTFrameBuffer::FirstEye()
{
    m_state->RT_AddMainCamera( r_viewpoint );
    Super::FirstEye();
}

FRenderState* RTFrameBuffer::RenderState()
{
    return m_state;
}
IVertexBuffer* RTFrameBuffer::CreateVertexBuffer()
{
    return new RTVertexBuffer{};
}
IIndexBuffer* RTFrameBuffer::CreateIndexBuffer()
{
    return new RTIndexBuffer{};
}
IDataBuffer* RTFrameBuffer::CreateDataBuffer( int bindingpoint, bool ssbo, bool needsresize )
{
    return new RTDataBuffer{};
}
IHardwareTexture* RTFrameBuffer::CreateHardwareTexture( int numchannels )
{
    return new RTHardwareTexture{};
}
void RTFrameBuffer::Draw2D()
{
    ::Draw2D( twod, *m_state );
}

//
//
//

namespace
{
constexpr auto remap01( float v, float newmin, float newmax )
{
    assert( newmax > newmin );
    return newmin + std::clamp( v, 0.f, 1.f ) * ( newmax - newmin );
}

auto RT_GetPlayer() -> player_t*
{
    return players[ consoleplayer ].camera ? players[ consoleplayer ].camera->player : nullptr;
}

auto RT_DamageIntensity() -> std::optional< float >
{
    // for reference https://doom.fandom.com/wiki/Comparison_of_Doom_monsters
    constexpr float maxdmg = 100.f;

    if( auto player = RT_GetPlayer() )
    {
        if( player->damagecount > 0 )
        {
            float dmg01 =
                std::clamp( static_cast< float >( player->damagecount ) / maxdmg, 0.f, 1.f );

            // smaller damage should also have effect
            dmg01 = sqrt( dmg01 );

            assert( dmg01 > 0.005f );
            return dmg01;
        }
    }
    return {};
}

uint32_t RT_CalcPowerupFlags()
{
    auto player = RT_GetPlayer();
    if( !player )
    {
        return 0;
    }

    uint32_t powerups = 0;

    for( AActor* in = player->mo->Inventory; in; in = in->Inventory )
    {
        if( in->IsKindOf( NAME_PowerStrength ) )
        {
            if( rtstate.m_berserkBlend > 10 )
            {
                powerups |= RT_POWERUP_FLAG_BERSERK_BIT;
            }
        }
        else if( in->IsKindOf( NAME_PowerIronFeet ) )
        {
            powerups |= RT_POWERUP_FLAG_RADIATIONSUIT_BIT;
        }
        else if( in->IsKindOf( NAME_PowerInvulnerable ) )
        {
            powerups |= RT_POWERUP_FLAG_INVUNERABILITY_BIT;
        }
        else if( in->IsKindOf( NAME_PowerLightAmp ) )
        {
            switch( *cvar::rt_pw_lightamp )
            {
                case 1: powerups |= RT_POWERUP_FLAG_THERMALVISION_BIT; break;
                case 2: powerups |= RT_POWERUP_FLAG_FLASHLIGHT_BIT; break;
                default: powerups |= RT_POWERUP_FLAG_NIGHTVISION_BIT; break;
            }
        }
        else if( in->IsKindOf( NAME_PowerInvisibility ) )
        {
            powerups |= RT_POWERUP_FLAG_INVISIBILITY_BIT;
        }

        // NAME_PowerTargeter
        // NAME_PowerWeaponLevel2
        // NAME_PowerFlight
        // NAME_PowerSpeed
        // NAME_PowerTorch
        // NAME_PowerHighJump
        // NAME_PowerReflection
        // NAME_PowerDrain
        // NAME_PowerScanner
        // NAME_PowerDoubleFiringSpeed
        // NAME_PowerInfiniteAmmo
        // NAME_PowerBuddha
    }

    if( player->bonuscount > 0 )
    {
        powerups |= RT_POWERUP_FLAG_BONUS_BIT;
    }

    return powerups;
}
} // anonymous namespace

//
//
//

static bool   g_resetposteffects = false;
static bool   g_resetfluid       = false;
static bool   g_melt_requested   = false;
static double g_melt_endtime     = -1;
bool          g_noinput_onstart  = true;

bool   g_cpu_latency_get = false;
double g_cpu_latency     = 0;

static void RT_DrawTitle();
static void RT_ClearTitles();
static void RT_InjectTitleIntoDoomMap( const char* mapname );

void RT_OnLevelLoad( const char* mapname)
{
    g_resetposteffects = true;
    g_resetfluid       = true;
    RT_ClearTitles();
    RT_InjectTitleIntoDoomMap( mapname );
    RT_ForceIntroCutsceneMusicStop();
}

void RT_RequestMelt()
{
    // HACKHACK: suppress melting when getting into the first start
    {
        static bool first = true;
        if( first )
        {
            first = false; 
            return;
        }
    }
    g_melt_requested = true;
}

bool RT_IsMeltActive()
{
    return g_melt_endtime > 0 && RT_GetCurrentTime() < g_melt_endtime;
}
bool RT_IgnoreUserInput()
{
    return RT_IsMeltActive() || g_noinput_onstart;
}

static double CalcCpuLatency()
{
    static double   g_lprevtime            = RT_GetCurrentTime();
    static double   g_lprevlatencies[ 30 ] = {};
    static uint32_t g_lprevi               = 0;

    double lcurtime = RT_GetCurrentTime();

    g_lprevlatencies[ g_lprevi ] = lcurtime - g_lprevtime;

    g_lprevi    = ( g_lprevi + 1 ) % std::size( g_lprevlatencies );
    g_lprevtime = lcurtime;

    double sum = 0;
    int    cnt = 0;
    for( double t : g_lprevlatencies )
    {
        if( t > 0 )
        {
            sum += t;
            cnt++;
        }
    }

    return cnt > 0 ? sum / cnt : 0;
}

namespace
{
template< typename T >
T smoothstep( T edge0, T edge1, T x )
{
    T t = std::clamp( ( x - edge0 ) / ( edge1 - edge0 ), T( 0 ), T( 1 ) );
    return t * t * ( T( 3 ) - T( 2 ) * t );
}

namespace classic_toggle
{
    constexpr double Duration  = 0.75;
    double           g_timeend = 0.0;

    float                  g_source = 0.0f;
    std::optional< float > g_target = {};

    CCMD( rt_classic_toggle )
    {
        if( g_isremix )
        {
            cvar::rt_classic = 0; 
            return;
        }

        g_timeend = RT_GetCurrentTime() + Duration;
        g_source  = std::clamp< float >( cvar::rt_classic, 0, 1 );

        if( g_target )
        {
            g_target = g_target.value() > 0 ? 0.f : 1.f;
        }
        else
        {
            g_target = cvar::rt_classic > 0 ? 0.f : 1.f;
        }
    }

    void Animate()
    {
        if( g_isremix )
        {
            cvar::rt_classic = 0;
            return;
        }

        if( g_target )
        {
            double dt = g_timeend - RT_GetCurrentTime();
            if( dt <= 0 )
            {
                cvar::rt_classic = *g_target;
                g_target         = {};
                return;
            }

            double ratio = 1 - std::clamp( dt / Duration, 0.0, 1.0 );
            ratio        = smoothstep( 0.0, 1.0, ratio );

            cvar::rt_classic = std::lerp( g_source, *g_target, static_cast< float >( ratio ) );
        }
    }
} // namespace classic_toggle

auto g_sectorlightlevels = std::vector< uint8_t >{};

bool RT_IsLuminousCeilingTexture( FTextureID texture )
{
    auto* gameTexture = TexMan.GameTexture( texture );
    if( !gameTexture )
    {
        return false;
    }

    // These stock flats have emissive maps in the bundled RT material pack.
    // Matching that list avoids inventing lights for ordinary bright ceilings.
    static constexpr const char* LuminousCeilings[] = {
        "CEIL1_2", "CEIL1_3", "CEIL3_4", "CEIL3_6", "FLAT17",
        "FLAT2",   "FLAT22",  "FLAT23",  "FLOOR1_7", "TLITE6_1",
        "TLITE6_4", "TLITE6_5", "TLITE6_6",
    };

    const char* name = gameTexture->GetName().GetChars();
    return std::ranges::any_of( LuminousCeilings, [ name ]( const char* luminous ) {
        return stricmp( name, luminous ) == 0;
    } );
}

bool RT_IsRedCeilingTexture( FTextureID texture )
{
    auto* gameTexture = TexMan.GameTexture( texture );
    if( !gameTexture )
    {
        return false;
    }

    const char* name = gameTexture->GetName().GetChars();
    return strnicmp( name, "TLITE6_", 7 ) == 0;
}

CCMD( rt_ceiling_probe )
{
    if( !primaryLevel )
    {
        Printf( "RT ceiling probe: no active level\n" );
        return;
    }

    Printf( "RT ceiling probe: lights=%d beams=%d beamIntensity=%.1f volumeType=%d "
            "scatter=%.2f lightMultiplier=%.2f flashlight=%d\n",
            int( bool{ cvar::rt_ceilinglights } ),
            int( bool{ cvar::rt_ceilingbeams } ),
            float{ cvar::rt_ceilingbeam_intensity },
            int{ cvar::rt_volume_type },
            float{ cvar::rt_volume_scatter },
            float{ cvar::rt_volume_lintensity },
            int( bool{ cvar::rt_flsh } ) );

    int matched = 0;
    for( uint32_t i = 0; i < primaryLevel->sectors.Size(); i++ )
    {
        const sector_t& sector = primaryLevel->sectors[ i ];
        const FTextureID texture = sector.GetTexture( sector_t::ceiling );
        if( !RT_IsLuminousCeilingTexture( texture ) )
        {
            continue;
        }

        auto* gameTexture = TexMan.GameTexture( texture );
        const char* name = gameTexture ? gameTexture->GetName().GetChars() : "(missing)";
        const float floorZ = float( sector.floorplane.ZatPoint( sector.centerspot ) );
        const float ceilingZ = float( sector.ceilingplane.ZatPoint( sector.centerspot ) );
        Printf( "  sector %u: %s, subsectors=%d, center=(%.1f %.1f), z=(%.1f..%.1f)\n",
                i,
                name,
                sector.subsectorcount,
                sector.centerspot.X,
                sector.centerspot.Y,
                floorZ,
                ceilingZ );
        matched++;
    }

    Printf( "RT ceiling probe: matched %d luminous sectors\n", matched );
}

void RT_MakeLightstyles()
{
    if( !primaryLevel || primaryLevel->sectors.Size() == 0 )
    {
        g_sectorlightlevels.clear();
        return;
    }
    g_sectorlightlevels.resize( primaryLevel->sectors.Size() );

    for( uint32_t i = 0; i < primaryLevel->sectors.Size(); i++ )
    {
        g_sectorlightlevels[ i ] = uint8_t( std::clamp( //
            primaryLevel->sectors[ i ].GetLightLevel(),
            0,
            255 ) );
    }
}

void RT_UploadExportableSectorLights()
{
    assert( g_sectorlightlevels.size() == primaryLevel->sectors.Size() );

    for( uint32_t i = 0; i < primaryLevel->sectors.Size(); i++ )
    {
        const sector_t& sector = primaryLevel->sectors[ i ];

        float z;
        {
            auto zfloor   = float( sector.floorplane.ZatPoint( sector.centerspot ) );
            auto zceiling = float( sector.ceilingplane.ZatPoint( sector.centerspot ) );

            // if too thin
            if( std::abs( zfloor - zceiling ) < 0.1f )
            {
                bool important = ( sector.special == Light_Phased ) ||
                                 ( sector.special == LightSequenceStart ) ||
                                 ( sector.special == LightSequenceSpecial1 ) ||
                                 ( sector.special == LightSequenceSpecial2 ) ||
                                 ( sector.special == dLight_Flicker ) ||
                                 ( sector.special == dLight_StrobeFast ) ||
                                 ( sector.special == dLight_StrobeSlow ) ||
                                 ( sector.special == dLight_Strobe_Hurt ) ||
                                 ( sector.special == dLight_Glow ) ||
                                 ( sector.special == dLight_StrobeSlowSync ) ||
                                 ( sector.special == dLight_StrobeFastSync ) ||
                                 ( sector.special == dLight_FireFlicker ) ||
                                 ( sector.special == sLight_Strobe_Hurt ) ||
                                 ( sector.special == Light_OutdoorLightning ) ||
                                 ( sector.special == Light_IndoorLightning1 ) ||
                                 ( sector.special == Light_IndoorLightning2 );
                if( !important )
                {
                    continue;
                }
            }

            z = ( zfloor + zceiling ) / 2;
        }

        const auto center = FVector3{
            float( sector.centerspot.X ),
            float( sector.centerspot.Y ),
            z,
        };

        auto adt = RgLightAdditionalEXT{
            .sType      = RG_STRUCTURE_TYPE_LIGHT_ADDITIONAL_EXT,
            .pNext      = nullptr,
            .flags      = RG_LIGHT_ADDITIONAL_LIGHTSTYLE,
            .lightstyle = int( i ), // references g_sectorlightlevels
            .hashName   = "",
        };

        auto lsph = RgLightSphericalEXT{
            .sType     = RG_STRUCTURE_TYPE_LIGHT_SPHERICAL_EXT,
            .pNext     = &adt,
            .color     = RG_PACKED_COLOR_WHITE,
            .intensity = cvar::rt_autoexport_light,
            .position  = { center.X * ONEGAMEUNIT_IN_METERS,
                           center.Y * ONEGAMEUNIT_IN_METERS,
                           center.Z * ONEGAMEUNIT_IN_METERS },
            .radius    = 0.05f,
        };

        auto linfo = RgLightInfo{
            .sType        = RG_STRUCTURE_TYPE_LIGHT_INFO,
            .pNext        = &lsph,
            .uniqueID     = SectorLightId_Base + i,
            .isExportable = true, // so we can write in the gltf
        };

        RgResult r = rt.rgUploadLight( &linfo );
        RG_CHECK( r );

        if( bool{ cvar::rt_ceilinglights } &&
            !( bool{ cvar::rt_stockscenes } && rt_isdoom2 ) &&
            float{ cvar::rt_ceilinglight_intensity } > 0 &&
            RT_IsLuminousCeilingTexture( sector.GetTexture( sector_t::ceiling ) ) )
        {
            const bool isRedCeiling = RT_IsRedCeilingTexture(
                sector.GetTexture( sector_t::ceiling ) );
            const RgColor4DPacked32 ceilingColor = isRedCeiling
                ? rt.rgUtilPackColorByte4D( 255, 42, 24, 255 )
                : rt.rgUtilPackColorByte4D( 255, 224, 184, 255 );

            struct Candidate
            {
                DVector2 position;
                double   area;
            };

            // A sector's sound-center is not guaranteed to be near the visible
            // ceiling (and can be especially poor for large concave rooms). Build
            // valid points from the convex BSP subsectors instead, then spread a
            // small bounded number of lights across the room.
            std::vector< Candidate > candidates;
            candidates.reserve( std::max( 1, sector.subsectorcount ) );

            for( int subIndex = 0; subIndex < sector.subsectorcount; subIndex++ )
            {
                const subsector_t* sub = sector.subsectors[ subIndex ];
                if( !sub || !sub->firstline || sub->numlines < 3 )
                {
                    continue;
                }

                double twiceArea = 0;
                double centroidX = 0;
                double centroidY = 0;
                for( uint32_t vertexIndex = 0; vertexIndex < sub->numlines; vertexIndex++ )
                {
                    const DVector2 a = sub->firstline[ vertexIndex ].v1->fPos();
                    const DVector2 b = sub->firstline[ ( vertexIndex + 1 ) % sub->numlines ].v1->fPos();
                    const double cross = a.X * b.Y - b.X * a.Y;
                    twiceArea += cross;
                    centroidX += ( a.X + b.X ) * cross;
                    centroidY += ( a.Y + b.Y ) * cross;
                }

                DVector2 position;
                if( std::abs( twiceArea ) > 0.001 )
                {
                    position = { centroidX / ( 3.0 * twiceArea ),
                                 centroidY / ( 3.0 * twiceArea ) };
                }
                else
                {
                    position = {};
                    for( uint32_t vertexIndex = 0; vertexIndex < sub->numlines; vertexIndex++ )
                    {
                        position += sub->firstline[ vertexIndex ].v1->fPos();
                    }
                    position /= double( sub->numlines );
                }

                candidates.push_back( { position, std::abs( twiceArea ) * 0.5 } );
            }

            if( candidates.empty() )
            {
                candidates.push_back( { sector.centerspot, 0 } );
            }

            const size_t maxLightCount = size_t( std::clamp(
                int{ cvar::rt_ceilinglight_maxcount }, 1, int( CeilingBeamId_Offset ) ) );
            const size_t lightCount = std::min( maxLightCount, candidates.size() );
            std::vector< size_t > selected;
            selected.reserve( lightCount );

            // Start in the largest piece of ceiling, then repeatedly select the
            // point farthest from those already chosen. This remains deterministic
            // while covering irregular rooms much better than BSP order would.
            size_t first = 0;
            for( size_t candidateIndex = 1; candidateIndex < candidates.size(); candidateIndex++ )
            {
                if( candidates[ candidateIndex ].area > candidates[ first ].area )
                {
                    first = candidateIndex;
                }
            }
            selected.push_back( first );

            while( selected.size() < lightCount )
            {
                size_t best = candidates.size();
                double bestDistance = -1;
                for( size_t candidateIndex = 0; candidateIndex < candidates.size(); candidateIndex++ )
                {
                    if( std::ranges::find( selected, candidateIndex ) != selected.end() )
                    {
                        continue;
                    }

                    double nearestDistance = std::numeric_limits< double >::max();
                    for( size_t selectedIndex : selected )
                    {
                        nearestDistance = std::min(
                            nearestDistance,
                            ( candidates[ candidateIndex ].position -
                              candidates[ selectedIndex ].position ).LengthSquared() );
                    }
                    if( nearestDistance > bestDistance )
                    {
                        bestDistance = nearestDistance;
                        best = candidateIndex;
                    }
                }

                if( best == candidates.size() )
                {
                    break;
                }
                selected.push_back( best );
            }

            for( size_t lightIndex = 0; lightIndex < selected.size(); lightIndex++ )
            {
                const DVector2 position = candidates[ selected[ lightIndex ] ].position;
                const float zfloor = float( sector.floorplane.ZatPoint( position ) );
                const float zceiling = float( sector.ceilingplane.ZatPoint( position ) );
                const float lightZ = std::max( zfloor + 4.f, zceiling - 8.f );

                auto ceilingSphere = RgLightSphericalEXT{
                    .sType     = RG_STRUCTURE_TYPE_LIGHT_SPHERICAL_EXT,
                    .pNext     = &adt,
                    .color     = ceilingColor,
                    .intensity = float{ cvar::rt_ceilinglight_intensity },
                    .position  = { float( position.X ) * ONEGAMEUNIT_IN_METERS,
                                   float( position.Y ) * ONEGAMEUNIT_IN_METERS,
                                   lightZ * ONEGAMEUNIT_IN_METERS },
                    .radius    = std::clamp( float{ cvar::rt_ceilinglight_radius }, 0.01f, 5.f ),
                };

                auto ceilingInfo = RgLightInfo{
                    .sType        = RG_STRUCTURE_TYPE_LIGHT_INFO,
                    .pNext        = &ceilingSphere,
                    .uniqueID     = CeilingLightId_Base + uint64_t( i ) * CeilingLightId_Stride + lightIndex,
                    .isExportable = false,
                };

                r = rt.rgUploadLight( &ceilingInfo );
                RG_CHECK( r );

                const size_t beamCount = bool{ cvar::rt_ceilingbeams }
                    ? size_t( std::clamp( int{ cvar::rt_ceilingbeam_maxcount },
                                         0,
                                         int( CeilingBeamId_Offset ) ) )
                    : 0;

                if( lightIndex < beamCount && float{ cvar::rt_ceilingbeam_intensity } > 0 )
                {
                    const float outerAngle = to_rad( std::clamp(
                        float{ cvar::rt_ceilingbeam_angle }, 5.f, 80.f ) );
                    const float beamZ = std::max( zfloor + 4.f, zceiling - 3.f );

                    // RTGL 1.6 compiles its all-local-lights illumination volume
                    // out, but it supports one selected volumetric source. Mark
                    // every ceiling beam as eligible; RTGL selects the closest
                    // active one. A camera-mounted flashlight naturally wins
                    // while enabled because it is closest to the viewer.
                    auto beamAdditional = RgLightAdditionalEXT{
                        .sType      = RG_STRUCTURE_TYPE_LIGHT_ADDITIONAL_EXT,
                        .pNext      = nullptr,
                        .flags      = RG_LIGHT_ADDITIONAL_VOLUMETRIC,
                        .lightstyle = 0,
                        .hashName   = "",
                    };

                    auto ceilingSpot = RgLightSpotEXT{
                        .sType      = RG_STRUCTURE_TYPE_LIGHT_SPOT_EXT,
                        // Keep the beam independent from the sector lightstyle.
                        .pNext      = &beamAdditional,
                        .color      = ceilingColor,
                        .intensity  = float{ cvar::rt_ceilingbeam_intensity },
                        .position   = { float( position.X ) * ONEGAMEUNIT_IN_METERS,
                                        float( position.Y ) * ONEGAMEUNIT_IN_METERS,
                                        beamZ * ONEGAMEUNIT_IN_METERS },
                        .direction  = { 0.f, 0.f, -1.f },
                        .radius     = std::clamp( float{ cvar::rt_ceilingbeam_radius }, 0.01f, 2.f ),
                        .angleOuter = outerAngle,
                        .angleInner = outerAngle * 0.65f,
                    };

                    auto beamInfo = RgLightInfo{
                        .sType        = RG_STRUCTURE_TYPE_LIGHT_INFO,
                        .pNext        = &ceilingSpot,
                        .uniqueID     = CeilingLightId_Base + uint64_t( i ) * CeilingLightId_Stride +
                                        CeilingBeamId_Offset + lightIndex,
                        .isExportable = false,
                    };

                    r = rt.rgUploadLight( &beamInfo );
                    RG_CHECK( r );
                }
            }
        }
    }
}

}

//
//
//

// Special extension
#define ext_RG_STRUCTURE_TYPE_START_FRAME_REMIX_PARAMS ( ( RgStructureType )1024 )
struct ext_RgStartFrameRemixParams
{
    RgStructureType sType;
    void*           pNext;
    RgBool32        rayReconstruction;
    RgBool32        taa;
    RgBool32        nis;
    RgBool32        reflex;
};

void RTFrameBuffer::RT_BeginFrame()
{
    // HACKHACK begin
    if( g_rt_skipinitframes == -10 )
    {
        g_rt_skipinitframes = cvar::hack_initialframesskip ? 0 : 2;
    }
    if( g_rt_skipinitframes >= 0 )
    {
        if( g_rt_skipinitframes == 0 )
        {
            RT_CloseLauncherWindow(); // renderer is ready, close launcher window
            PositionWindow( IsFullscreen() );
            g_rt_forcenofocuschange = false;
        }
        --g_rt_skipinitframes;
    }
    // HACKHACK end


    m_state->RT_BeginFrame();

    classic_toggle::Animate();


    auto resolution_params = RgStartFrameRenderResolutionParams{
        .sType             = RG_STRUCTURE_TYPE_START_FRAME_RENDER_RESOLUTION_PARAMS,
        .pNext             = nullptr,
        .preferDxgiPresent = cvar::rt_available_dxgi ? cvar::rt_dxgi : false,
    };
    RT_ResolutionToRtgl( &resolution_params, RT_GetCurrentWindowSize() );
    RT_UpscaleCvarsToRtgl( &resolution_params );

    ext_RgStartFrameRemixParams remix_params;
    if( g_isremix )
    {
        remix_params = ext_RgStartFrameRemixParams{
            .sType             = ext_RG_STRUCTURE_TYPE_START_FRAME_REMIX_PARAMS,
            .pNext             = nullptr,
            .rayReconstruction = ( cvar::rt_remix_rayreconstr ? 1u : 0u ),
            .taa               = ( cvar::rt_remix_taa > 0 ? 1u : 0u ),
            .nis               = 0,
            .reflex            = ( cvar::rt_remix_reflex ? 1u : 0u ),
        };

        switch( int( cvar::rt_remix_taa ) )
        {
            case 4:
                resolution_params.resolutionMode = RG_RENDER_RESOLUTION_MODE_ULTRA_PERFORMANCE;
                break;
            case 3: resolution_params.resolutionMode = RG_RENDER_RESOLUTION_MODE_PERFORMANCE; break;
            case 2: resolution_params.resolutionMode = RG_RENDER_RESOLUTION_MODE_BALANCED; break;
            case 1: resolution_params.resolutionMode = RG_RENDER_RESOLUTION_MODE_QUALITY; break;
            case 6: resolution_params.resolutionMode = RG_RENDER_RESOLUTION_MODE_NATIVE_AA; break;
            case 5: resolution_params.resolutionMode = RG_RENDER_RESOLUTION_MODE_CUSTOM; break;
            default: remix_params.taa = 0; break;
        }

        remix_params.pNext      = resolution_params.pNext;
        resolution_params.pNext = &remix_params;
    }

    RT_MakeLightstyles();

    auto fluid_params = RgStartFrameFluidParams{
        .sType          = RG_STRUCTURE_TYPE_START_FRAME_FLUID_PARAMS,
        .pNext          = &resolution_params,
        .enabled        = cvar::rt_fluid_available ? cvar::rt_fluid : false,
        .reset          = g_resetfluid,
        .gravity        = { cvar::rt_fluid_gravity_x, //
                            cvar::rt_fluid_gravity_y,
                            cvar::rt_fluid_gravity_z },
        .color          = { cvar::rt_blood_color_r, //
                            cvar::rt_blood_color_g,
                            cvar::rt_blood_color_b },
        .particleBudget = uint32_t( std::max( 0, int( cvar::rt_fluid_budget ) ) ),
        .particleRadius = cvar::rt_fluid_pradius,
    };

    RgStaticSceneStatusFlags staticscene_status = 0;

    auto info = RgStartFrameInfo{
        .sType                  = RG_STRUCTURE_TYPE_START_FRAME_INFO,
        .pNext                  = &fluid_params,
        .pMapName               = RT_GetMapName(),
        .ignoreExternalGeometry = false,
        .vsync                  = cvar::rt_vsync,
        .hdr                    = cvar::rt_hdr_available ? cvar::rt_hdr : false,
        .allowMapAutoExport     = cvar::rt_autoexport,
        .lightmapScreenCoverage = RT_ForceNoClassicMode() ? 0.0f : cvar::rt_classic,
        .lightstyleValuesCount  = uint32_t( g_sectorlightlevels.size() ),
        .pLightstyleValues8     = g_sectorlightlevels.data(),
        .pResultStaticSceneStatus = &staticscene_status,
        .staticSceneAnimationTime = g_rt_cutscenename ? RT_CutsceneTime() : 0,
    };
    g_resetfluid = false;

    RgResult r = rt.rgStartFrame( &info );
    RG_CHECK( r );


    auto l_clm = [ staticscene_status ]() {
        if( staticscene_status & RG_STATIC_SCENE_STATUS_EXPORT_STARTED )
        {
            return 2; // no cull as we need to upload all geometry for the first time
        }
        if( staticscene_status & RG_STATIC_SCENE_STATUS_NEW_SCENE_STARTED )
        {
            return 2; // touch everything, to upload all resources
        }
        if( !( staticscene_status & RG_STATIC_SCENE_STATUS_LOADED ) )
        {
            return 2; // no static scene, upload everything
        }
        switch( int( cvar::rt_cpu_cullmode ) )
        {
            case 1: return 1;
            case 2: return 2;
            default: return 0;
        }
    };

    rt_cullmode = l_clm();
}

void RTFrameBuffer::RT_DrawFrame()
{
    const double   curtime      = RT_GetCurrentTime();
    const uint32_t powerupflags = RT_CalcPowerupFlags();

    RT_DrawTitle();

    struct SunSettings
    {
        bool active{};
        float altitude{};
        float azimuth{};
        float intensity{};
        float angularDiameter{ 0.5f };
        RgColor4DPacked32 color{};
    } sun;

    const bool useDoomE1RealisticLights = RT_UseDoomE1RealisticLights( primaryLevel );
    const bool useDoomE2RealisticLights = RT_UseDoomE2RealisticLights( primaryLevel );
    const bool useDoomE3RealisticLights = RT_UseDoomE3RealisticLights( primaryLevel );
    const int useDoom2RealisticGroup = RT_GetDoom2RealisticGroup( primaryLevel );
    const bool useDoomRealisticLights =
        useDoomE1RealisticLights || useDoomE2RealisticLights || useDoomE3RealisticLights ||
        useDoom2RealisticGroup != 0;

    if( bool{ cvar::rt_sun } && float{ cvar::rt_sun_intensity } > 0 )
    {
        sun.active          = true;
        sun.altitude        = float{ cvar::rt_sun_a };
        sun.azimuth         = float{ cvar::rt_sun_b };
        sun.intensity       = float{ cvar::rt_sun_intensity };
        sun.angularDiameter = 0.5f;
        sun.color           = cvarcolor_to_rtcolor( cvar::rt_sun_color );
    }
    else if( bool{ cvar::rt_autosun } &&
             !( bool{ cvar::rt_stockscenes } && rt_isdoom2 ) &&
             float{ cvar::rt_autosun_intensity } > 0 && primaryLevel )
    {
        uint32_t outdoorSectorCount = 0;
        for( const sector_t& sector : primaryLevel->sectors )
        {
            outdoorSectorCount += sector.GetTexture( sector_t::ceiling ) == skyflatnum ? 1u : 0u;
        }

        if( outdoorSectorCount > 0 )
        {
            // FNV-1a and xorshift give each scene a repeatable starting direction.
            // The user seed then walks a curated set of low angles. Low sunlight is
            // much more likely to form visible shafts through Doom's openings than
            // a high overhead sun.
            uint32_t randomState = 2166136261u;
            const char* sceneName = RT_GetMapName();
            for( const unsigned char* p = reinterpret_cast< const unsigned char* >( sceneName );
                 sceneName && *p;
                 ++p )
            {
                randomState ^= *p;
                randomState *= 16777619u;
            }
            auto nextRandom = [ &randomState ]() {
                randomState ^= randomState << 13;
                randomState ^= randomState >> 17;
                randomState ^= randomState << 5;
                return float( randomState & 0x00FFFFFFu ) / float( 0x01000000u );
            };

            const float minAltitude = std::clamp( float{ cvar::rt_autosun_minaltitude }, 2.f, 88.f );
            const float maxAltitude = std::clamp(
                float{ cvar::rt_autosun_maxaltitude }, minAltitude, 88.f );
            constexpr float rayFriendlyAltitudes[] = { 15.f, 10.f, 20.f, 12.f, 25.f, 18.f, 8.f, 28.f };
            constexpr int angleCount = int( std::size( rayFriendlyAltitudes ) );
            const int seed = int{ cvar::rt_autosun_seed };
            const int angleIndex = ( ( seed % angleCount ) + angleCount ) % angleCount;

            sun.active          = true;
            sun.altitude        = std::clamp( rayFriendlyAltitudes[ angleIndex ], minAltitude, maxAltitude );
            sun.azimuth         = std::fmod( nextRandom() * 360.f + float( angleIndex ) * 45.f, 360.f );
            const float daylight = std::clamp( ( sun.altitude - 8.f ) / 22.f, 0.f, 1.f );
            sun.intensity       = float{ cvar::rt_autosun_intensity } * std::lerp( 0.60f, 1.0f, daylight );
            sun.angularDiameter = std::clamp( float{ cvar::rt_autosun_softness }, 0.1f, 10.f );

            const auto colorByte = [ daylight ]( float warm, float noon ) {
                return uint8_t( std::clamp( std::lerp( warm, noon, daylight ), 0.f, 255.f ) );
            };
            sun.color = rt.rgUtilPackColorByte4D(
                colorByte( 255.f, 255.f ),
                colorByte( 150.f, 242.f ),
                colorByte( 90.f, 218.f ),
                255 );

            static std::string lastReportedAutoSun;
            std::string reportKey = sceneName ? sceneName : "";
            reportKey += ':' + std::to_string( int{ cvar::rt_autosun_seed } );
            if( reportKey != lastReportedAutoSun )
            {
                Printf( "RT automatic sun: %u outdoor sectors, altitude %.1f, azimuth %.1f\n",
                        outdoorSectorCount,
                        sun.altitude,
                        sun.azimuth );
                lastReportedAutoSun = std::move( reportKey );
            }
        }
    }

    if( useDoomE1RealisticLights )
    {
        const int seed = int{ cvar::rt_autosun_seed };
        const DoomE1SunPreset preset = RT_GetDoomE1SunPreset( seed );
        const float selectedIntensity =
            std::clamp( float{ cvar::rt_sun_intensity }, 0.f, 500.f );
        sun.active          = true;
        sun.altitude        = preset.altitude;
        sun.azimuth         = preset.azimuth;
        sun.intensity       = selectedIntensity > 0.f
                                  ? std::max( selectedIntensity, 150.f )
                                  : 0.f;
        sun.angularDiameter = 0.8f;
        sun.color           = rt.rgUtilPackColorByte4D( 255, 229, 194, 255 );

        static std::string lastReportedRealisticSun;
        std::string reportKey = RT_GetMapName() ? RT_GetMapName() : "";
        reportKey += ':' + std::to_string( seed );
        if( reportKey != lastReportedRealisticSun )
        {
            Printf( "RT Episode 1 realistic sun %d: altitude %.1f, azimuth %.1f, intensity %.0f\n",
                    seed, preset.altitude, preset.azimuth, sun.intensity );
            lastReportedRealisticSun = std::move( reportKey );
        }
    }

    if( useDoomE2RealisticLights )
    {
        const int seed = int{ cvar::rt_autosun_seed };
        const DoomE2RiftLightPreset preset = RT_GetDoomE2RiftLightPreset( seed );
        sun.active          = true;
        sun.altitude        = preset.altitude;
        sun.azimuth         = preset.azimuth;
        sun.intensity       = std::clamp( float{ cvar::rt_sun_intensity }, 0.f, 500.f );
        sun.angularDiameter = 4.0f;
        sun.color           = rt.rgUtilPackColorByte4D( 255, 70, 36, 255 );

        static std::string lastReportedRealisticRift;
        std::string reportKey = RT_GetMapName() ? RT_GetMapName() : "";
        reportKey += ':' + std::to_string( seed );
        if( reportKey != lastReportedRealisticRift )
        {
            Printf( "RT Episode 2 realistic rift light %d: altitude %.1f, azimuth %.1f, intensity %.0f\n",
                    seed, preset.altitude, preset.azimuth, sun.intensity );
            lastReportedRealisticRift = std::move( reportKey );
        }
    }

    if( useDoomE3RealisticLights )
    {
        const int seed = int{ cvar::rt_autosun_seed };
        const DoomE3HorizonLightPreset preset = RT_GetDoomE3HorizonLightPreset( seed );
        sun.active          = true;
        sun.altitude        = preset.altitude;
        sun.azimuth         = preset.azimuth;
        sun.intensity       = std::clamp( float{ cvar::rt_sun_intensity }, 0.f, 500.f );
        sun.angularDiameter = 3.5f;
        sun.color           = rt.rgUtilPackColorByte4D( 255, 96, 40, 255 );

        static std::string lastReportedRealisticHorizon;
        std::string reportKey = RT_GetMapName() ? RT_GetMapName() : "";
        reportKey += ':' + std::to_string( seed );
        if( reportKey != lastReportedRealisticHorizon )
        {
            Printf( "RT Episode 3 realistic horizon light %d: altitude %.1f, azimuth %.1f, intensity %.0f\n",
                    seed, preset.altitude, preset.azimuth, sun.intensity );
            lastReportedRealisticHorizon = std::move( reportKey );
        }
    }

    if( useDoom2RealisticGroup != 0 )
    {
        const int seed = int{ cvar::rt_autosun_seed };
        const Doom2ChapterLightPreset preset =
            RT_GetDoom2ChapterLightPreset( useDoom2RealisticGroup, seed );
        const float selectedIntensity =
            std::clamp( float{ cvar::rt_sun_intensity }, 0.f, 500.f );
        sun.active          = true;
        sun.altitude        = preset.altitude;
        sun.azimuth         = preset.azimuth;
        sun.intensity       = selectedIntensity > 0.f && useDoom2RealisticGroup < 3
                                  ? std::max( selectedIntensity,
                                              useDoom2RealisticGroup == 1 ? 220.f : 160.f )
                                  : selectedIntensity;
        sun.angularDiameter = useDoom2RealisticGroup == 3 ? 3.5f : 0.8f;
        sun.color = useDoom2RealisticGroup == 1
                        ? rt.rgUtilPackColorByte4D( 255, 216, 168, 255 )
                        : ( useDoom2RealisticGroup == 2
                                ? rt.rgUtilPackColorByte4D( 255, 202, 158, 255 )
                                : rt.rgUtilPackColorByte4D( 255, 72, 34, 255 ) );

        static std::string lastReportedDoom2Chapter;
        std::string reportKey = RT_GetMapName() ? RT_GetMapName() : "";
        reportKey += ':' + std::to_string( seed );
        if( reportKey != lastReportedDoom2Chapter )
        {
            Printf( "RT Doom II realistic chapter %d light %d: altitude %.1f, azimuth %.1f, intensity %.0f\n",
                    useDoom2RealisticGroup, seed, preset.altitude, preset.azimuth, sun.intensity );
            lastReportedDoom2Chapter = std::move( reportKey );
        }
    }

    if( sun.active )
    {
        float altitude = to_rad( sun.altitude );
        float azimuth  = to_rad( sun.azimuth );

        float theta = std::clamp( pi() / 2 - altitude, 0.f, pi() );
        float phi   = std::fmod( azimuth, pi() * 2 );

        // negate, direction from the sun, not towards the sun
        auto dir = RgFloat3D{
            -sin( theta ) * cos( phi ),
            -sin( theta ) * sin( phi ),
            -cos( theta ),
        };

        auto s = RgLightDirectionalEXT{
            .sType                  = RG_STRUCTURE_TYPE_LIGHT_DIRECTIONAL_EXT,
            .pNext                  = nullptr,
            .color                  = sun.color,
            .intensity              = sun.intensity,
            .direction              = dir,
            .angularDiameterDegrees = sun.angularDiameter,
        };

        auto i = RgLightInfo{
            .sType        = RG_STRUCTURE_TYPE_LIGHT_INFO,
            .pNext        = &s,
            .uniqueID     = SunLightId,
            .isExportable = false,
        };

        RgResult r = rt.rgUploadLight( &i );
        RG_CHECK( r );

        if( ( useDoomE1RealisticLights ||
              ( useDoom2RealisticGroup > 0 && useDoom2RealisticGroup < 3 ) ) &&
            g_rt_mainCameraValid &&
            RT_EnsureDoomE1SunTexture() )
        {
            // Keep the visible body at the exact source direction of the light.
            // Tiled flare geometry lets walls and roofs hide only the covered part.
            const float angularDiameter = std::clamp(
                float{ cvar::rt_doom_e1_sun_size }, 1.f, 20.f );
            RT_UploadDoomE1SkyBillboard( dir, angularDiameter, "tuindoom/e1_sun" );
            // A slightly smaller second layer gives the body a white-hot core
            // that remains visible after sky exposure and volumetric haze.
            RT_UploadDoomE1SkyBillboard(
                dir, angularDiameter * 0.82f, "tuindoom/e1_sun" );
        }
    }

    RT_UploadExportableSectorLights();

    auto tm_params = RgDrawFrameTonemappingParams{
        .sType                = RG_STRUCTURE_TYPE_DRAW_FRAME_TONEMAPPING_PARAMS,
        .pNext                = nullptr,
        .disableEyeAdaptation = false,
        .ev100Min             = cvar::rt_tnmp_ev100_min,
        .ev100Max             = cvar::rt_tnmp_ev100_max,
        .luminanceWhitePoint  = cvar::rt_classic_white,
        .saturation           = { cvar::rt_tnmp_saturation_r,
                                  cvar::rt_tnmp_saturation_g,
                                  cvar::rt_tnmp_saturation_b },
        .crosstalk            = { cvar::rt_tnmp_crosstalk_r,
                                  cvar::rt_tnmp_crosstalk_g,
                                  cvar::rt_tnmp_crosstalk_b },
        .contrast             = cvar::rt_tnmp_contrast,
        .hdrBrightness        = cvar::rt_hdr_brightness,
        .hdrContrast          = cvar::rt_hdr_contrast,
        .hdrSaturation        = { cvar::rt_hdr_saturation,
                                  cvar::rt_hdr_saturation,
                                  cvar::rt_hdr_saturation },
    };

    auto reflrefr_params = RgDrawFrameReflectRefractParams{
        .sType                   = RG_STRUCTURE_TYPE_DRAW_FRAME_REFLECT_REFRACT_PARAMS,
        .pNext                   = &tm_params,
        .maxReflectRefractDepth  = safe_uint( *cvar::rt_reflrefr_depth ),
        .typeOfMediaAroundCamera = RG_MEDIA_TYPE_VACUUM,
        .indexOfRefractionGlass  = cvar::rt_refr_glass,
        .indexOfRefractionWater  = cvar::rt_refr_water,
        .waterWaveSpeed          = 0.05f,                    // for partial_invisibility
        .waterWaveNormalStrength = cvar::rt_water_wavestren, // for partial_invisibility
        .waterColor              = { std::clamp( *cvar::rt_water_r / 255.f, 0.f, 1.f ),
                                     std::clamp( *cvar::rt_water_g / 255.f, 0.f, 1.f ),
                                     std::clamp( *cvar::rt_water_b / 255.f, 0.f, 1.f ) },
        .acidColor               = { 0.08f, 0.65f, 0.10f },
        .acidDensity             = 0.42f,
        .waterWaveTextureDerivativesMultiplier = 1.0f,
        .waterTextureAreaScale                 = 1.0f,
        .portalNormalTwirl                     = false,
    };

    const float realisticSkyLimit =
        useDoomE3RealisticLights || useDoom2RealisticGroup == 3
            ? 58.f
            : ( useDoom2RealisticGroup == 1
                    ? 48.f
                    : ( useDoomE2RealisticLights || useDoom2RealisticGroup == 2 ? 64.f : 72.f ) );
    const float mapSkyIntensity = useDoomRealisticLights
                                      ? std::min( float{ cvar::rt_sky }, realisticSkyLimit )
                                      : float{ cvar::rt_sky };
    // This mode supplies authored full-color artwork. Do not let the launcher's
    // optional colored-ambient setting turn the visible panorama grayscale.
    const float mapSkySaturation = useDoomRealisticLights
                                       ? 1.0f
                                       : float{ cvar::rt_sky_saturation };

    auto sky_params = RgDrawFrameSkyParams{
        .sType              = RG_STRUCTURE_TYPE_DRAW_FRAME_SKY_PARAMS,
        .pNext              = &reflrefr_params,
        .skyType            = m_wassky ? RG_SKY_TYPE_RASTERIZED_GEOMETRY : RG_SKY_TYPE_COLOR,
        .skyColorDefault    = { 0, 0, 0 },
        .skyColorMultiplier = mapSkyIntensity,
        .skyColorSaturation = mapSkySaturation,
        .skyViewerPosition  = { 0, 0, 0 },
    };

    auto volumetrics_params = RgDrawFrameVolumetricParams{
        .sType                   = RG_STRUCTURE_TYPE_DRAW_FRAME_VOLUMETRIC_PARAMS,
        .pNext                   = &sky_params,
        .enable                  = cvar::rt_volume_type != 0,
        .maxHistoryLength        = cvar::rt_volume_type == 1 ? cvar::rt_volume_history : 0.f,
        .useSimpleDepthBased     = cvar::rt_volume_type == 2,
        .volumetricFar           = cvar::rt_volume_far,
        .ambientColor            = { cvar::rt_volume_ambient,
                                     cvar::rt_volume_ambient,
                                     cvar::rt_volume_ambient },
        .scaterring              = cvar::rt_volume_scatter,
        .assymetry               = cvar::rt_volume_lassymetry,
        .useIlluminationVolume   = cvar::rt_volume_local_lights,
        .fallbackSourceColor     = { 0, 0, 0 },
        .fallbackSourceDirection = { 0, -1, 0 },
        .lightMultiplier         = cvar::rt_volume_lintensity,
        .allowTintUnderwater     = false,
        .underwaterColor         = {},
    };

    auto texture_params = RgDrawFrameTexturesParams{
        .sType = RG_STRUCTURE_TYPE_DRAW_FRAME_TEXTURES_PARAMS,
        .pNext = &volumetrics_params,
        .dynamicSamplerFilter =
            cvar::rt_smoothtextures ? RG_SAMPLER_FILTER_LINEAR : RG_SAMPLER_FILTER_NEAREST,
        .normalMapStrength      = cvar::rt_normalmap_stren,
        .emissionMapBoost       = cvar::rt_emis_mapboost,
        .emissionMaxScreenColor = cvar::rt_emis_maxscrcolor,
        .minRoughness           = cvar::rt_refl_thresh,
        .heightMapDepth         = 0.02f * cvar::rt_heightmap_stren,
    };

    float dirtscale = ( ( powerupflags & RT_POWERUP_FLAG_RADIATIONSUIT_BIT ) ||
                        ( powerupflags & RT_POWERUP_FLAG_NIGHTVISION_BIT ) )
                          ? 15.f
                          : cvar::rt_bloom_dirt_scale;

    auto bloom_params = RgDrawFrameBloomParams{
        .sType             = RG_STRUCTURE_TYPE_DRAW_FRAME_BLOOM_PARAMS,
        .pNext             = &texture_params,
        .inputEV           = cvar::rt_bloom_ev,
        .inputThreshold    = cvar::rt_bloom_threshold,
        .bloomIntensity    = cvar::rt_bloom ? cvar ::rt_bloom_scale : 0.f,
        .lensDirtIntensity = cvar::rt_bloom_dirt ? dirtscale : 0.f,
    };

    auto illum_params = RgDrawFrameIlluminationParams{
        .sType                              = RG_STRUCTURE_TYPE_DRAW_FRAME_ILLUMINATION_PARAMS,
        .pNext                              = &bloom_params,
        .maxBounceShadows                   = safe_uint( *cvar::rt_shadowrays ),
        .enableSecondBounceForIndirect      = true,
        .cellWorldSize                      = 2.0f,
        .directDiffuseSensitivityToChange   = 1.0f,
        .indirectDiffuseSensitivityToChange = 0.75f,
        .specularSensitivityToChange        = 1.0f,
        .polygonalLightSpotlightFactor      = 2.0f,
        .lightUniqueIdIgnoreFirstPersonViewerShadows = &FlashlightLightId,
    };

    auto ef_wipe = RgPostEffectWipe{
        .stripWidth = 1.0f / 320.0f,
        .beginNow   = cvar::rt_melt_duration > 0.05f ? g_melt_requested : false,
        .duration   = cvar::rt_melt_duration > 0.05f ? cvar::rt_melt_duration : 0.0f,
    };
    g_melt_requested = false;

    if( ef_wipe.beginNow )
    {
        g_melt_endtime = curtime + static_cast< double >( ef_wipe.duration );
    }
    if( g_melt_endtime > 0 && curtime > g_melt_endtime )
    {
        g_melt_endtime = -1;
    }

    auto ef_radialblur = RgPostEffectRadialBlur{
        .isActive              = powerupflags & RT_POWERUP_FLAG_BERSERK_BIT,
        .transitionDurationIn  = 0.4f,
        .transitionDurationOut = 3.0f,
    };

    bool chrabr_from_powerup = ( powerupflags & RT_POWERUP_FLAG_NIGHTVISION_BIT ) ||
                               ( powerupflags & RT_POWERUP_FLAG_THERMALVISION_BIT ) ||
                               ( powerupflags & RT_POWERUP_FLAG_BERSERK_BIT );

    auto ef_chrabr = RgPostEffectChromaticAberration{
        .isActive              = chrabr_from_powerup || cvar::rt_ef_chraber > 0.f,
        .transitionDurationIn  = 0,
        .transitionDurationOut = 0,
        .intensity             = chrabr_from_powerup ? 1.2f : cvar::rt_ef_chraber,
    };
    // smooth out manually (because it's a constant active effect, i.e. without switching isActive)
    {
        constexpr auto Duration     = 0.5f;
        static double  begin_time   = curtime;
        static float   last_value   = ef_chrabr.intensity;
        static float   begin_value  = ef_chrabr.intensity;
        static float   target_value = ef_chrabr.intensity;

        if( std::abs( target_value - ef_chrabr.intensity ) > 0.001f )
        {
            begin_time   = curtime;
            begin_value  = last_value;
            target_value = ef_chrabr.intensity;
        }

        // if( begin_time <= curtime && curtime <= begin_time + double( Duration ) )
        {
            const float t = std::clamp( float( curtime - begin_time ) / Duration, 0.0f, 1.0f );
            ef_chrabr.intensity = std::lerp( begin_value, target_value, t );
        }
        last_value = ef_chrabr.intensity;
    }

    auto ef_invbw = RgPostEffectInverseBlackAndWhite{
        .isActive              = powerupflags & RT_POWERUP_FLAG_INVUNERABILITY_BIT,
        .transitionDurationIn  = 1.0f,
        .transitionDurationOut = 1.5f,
    };

    auto ef_hueshift = RgPostEffectHueShift{
        .isActive              = powerupflags & RT_POWERUP_FLAG_THERMALVISION_BIT,
        .transitionDurationIn  = 0.5f,
        .transitionDurationOut = 0.5f,
    };

    auto ef_nightvision = RgPostEffectNightVision{
        .isActive              = powerupflags & RT_POWERUP_FLAG_NIGHTVISION_BIT,
        .transitionDurationIn  = 0.5f,
        .transitionDurationOut = 0.5f,
    };

    auto ef_distortedsides = RgPostEffectDistortedSides{
        .isActive              = powerupflags & RT_POWERUP_FLAG_RADIATIONSUIT_BIT,
        .transitionDurationIn  = 1.0f,
        .transitionDurationOut = 1.0f,
    };

    // static, so prev state's transition durations
    // are preserved across frames, when flags are removed
    static auto ef_tint = RgPostEffectColorTint{};
    {
        ef_tint.isActive = false;

        if( auto dmg = RT_DamageIntensity() )
        {
            ef_tint = RgPostEffectColorTint{
                .isActive              = true,
                .transitionDurationIn  = 0.0f,
                .transitionDurationOut = remap01( *dmg, 0.5f, 1.7f ),
                .intensity             = remap01( *dmg, 1.5f, 3.0f ) * blood_fade_scalar,
                .color                 = { 1.f, 0.f, 0.f },
            };
        }
        else if( powerupflags & RT_POWERUP_FLAG_RADIATIONSUIT_BIT )
        {
            ef_tint = RgPostEffectColorTint{
                .isActive              = true,
                .transitionDurationIn  = 1.0f,
                .transitionDurationOut = 1.0f,
                .intensity             = 1.0f,
                .color                 = { 0.2f, 1.f, 0.4f },
            };
        }
        else if( powerupflags & RT_POWERUP_FLAG_BONUS_BIT )
        {
            ef_tint = RgPostEffectColorTint{
                .isActive              = true,
                .transitionDurationIn  = 0.0f,
                .transitionDurationOut = 0.7f,
                .intensity             = 0.5f * pickup_fade_scalar,
                .color                 = { 1.f, 0.91f, 0.42f },
            };
        }
    }

    const int vintage_crt = int{ cvar::rt_ef_vintage } == RT_VINTAGE_CRT ||
                            int{ cvar::rt_ef_vintage } == RT_VINTAGE_VHS_CRT;
    const int vintage_vhs = int{ cvar::rt_ef_vintage } == RT_VINTAGE_VHS ||
                            int{ cvar::rt_ef_vintage } == RT_VINTAGE_VHS_CRT;
    const int vintage_dither = int{ cvar::rt_ef_vintage } == RT_VINTAGE_200_DITHER ||
                               int{ cvar::rt_ef_vintage } == RT_VINTAGE_480_DITHER;

    auto ef_crt = RgPostEffectCRT{
        .isActive = vintage_crt || cvar::rt_ef_crt,
    };

    auto ef_vhs = RgPostEffectVHS{
        .isActive              = vintage_vhs || cvar::rt_ef_vhs > 0.f,
        .transitionDurationIn  = 0,
        .transitionDurationOut = 0,
        .intensity             = vintage_vhs ? 0.9f : float{ cvar::rt_ef_vhs },
    };

    auto ef_dither = RgPostEffectDither{
        .isActive              = vintage_dither || cvar::rt_ef_dither > 0.f,
        .transitionDurationIn  = 0,
        .transitionDurationOut = 0,
        .intensity             = vintage_dither ? 0.8f : float{ cvar::rt_ef_dither },
    };

    // some of the power-up effects need to be reset
    auto post_params = RgDrawFramePostEffectsParams{
        .sType                 = RG_STRUCTURE_TYPE_DRAW_FRAME_POST_EFFECTS_PARAMS,
        .pNext                 = &illum_params,
        .pWipe                 = &ef_wipe,
        .pRadialBlur           = g_resetposteffects ? nullptr : &ef_radialblur,
        .pChromaticAberration  = &ef_chrabr,
        .pInverseBlackAndWhite = g_resetposteffects ? nullptr : &ef_invbw,
        .pHueShift             = g_resetposteffects ? nullptr : &ef_hueshift,
        .pNightVision          = g_resetposteffects ? nullptr : &ef_nightvision,
        .pDistortedSides       = g_resetposteffects ? nullptr : &ef_distortedsides,
        .pColorTint            = g_resetposteffects ? nullptr : &ef_tint,
        .pCRT                  = &ef_crt,
        .pVHS                  = &ef_vhs,
        .pDither               = &ef_dither,
    };

    auto info = RgDrawFrameInfo{
        .sType            = RG_STRUCTURE_TYPE_DRAW_FRAME_INFO,
        .pNext            = &post_params,
        .rayLength        = GetZFar() * ONEGAMEUNIT_IN_METERS,
        .presentPrevFrame = false,
        .currentTime      = curtime,
    };

    RgResult r = rt.rgDrawFrame( &info );
    RG_CHECK( r );

    if( g_cpu_latency_get )
    {
        g_cpu_latency = CalcCpuLatency();
    }

    // reset for next frame
    {
        m_wassky           = false;
        g_resetposteffects = false;
    }
}

//
//
//

bool RTRenderState::IsPerspectiveMatrix( const float* m )
{
    return std::abs( m[ 15 ] ) < std::numeric_limits< float >::epsilon();
}

bool RTRenderState::IsLikeIdentity( const float* m )
{
    auto areSimilar = []( float a, float b ) {
        return std::abs( a - b ) < 0.0000001f;
    };
    for( int a = 0; a < 4; a++ )
    {
        for( int b = 0; b < 4; b++ )
        {
            if( !areSimilar( m[ a * 4 + b ], ( a == b ? 1.0f : 0.0f ) ) )
            {
                return false;
            }
        }
    }
    return true;
}
bool RTRenderState::IsLikeIdentity( const double* m )
{
    auto areSimilar = []( double a, double b ) {
        return std::abs( a - b ) < 0.0000001;
    };
    for( int a = 0; a < 4; a++ )
    {
        for( int b = 0; b < 4; b++ )
        {
            if( !areSimilar( m[ a * 4 + b ], ( a == b ? 1.0 : 0.0 ) ) )
            {
                return false;
            }
        }
    }
    return true;
}

auto RT_MakeUpRightForwardVectors( const DRotator& rotation ) -> std::tuple< RgFloat3D, RgFloat3D, RgFloat3D >
{
    // based on HWDrawInfo::SetViewMatrix
    RgFloat3D up, right, forward;

    auto pitch = rotation.Pitch;
    // RT: invert yaw
    auto yaw  = FAngle::fromDeg( -( 270.0 - rotation.Yaw.Degrees() ) );
    auto roll = rotation.Roll;

    auto view = VSMatrix{ 1 };
    view.rotate( float( yaw.Degrees() ), 0, 0, 1 );   // around up
    view.rotate( float( pitch.Degrees() ), 1, 0, 0 ); // around right
    view.rotate( float( roll.Degrees() ), 0, 1, 0 );  // around forward
    const float* v = view.get();

    auto v100 = RgFloat3D{ -v[ 0 ], -v[ 1 ], -v[ 2 ] };
    auto v010 = RgFloat3D{ -v[ 4 ], -v[ 5 ], -v[ 6 ] };
    auto v001 = RgFloat3D{ v[ 8 ], v[ 9 ], v[ 10 ] };

    up      = v001;
    right   = v100;
    forward = v010;

    return { up, right, forward };
}

void RT_ForceCamera( const FVector3 position, const DRotator& rotation, float fovYDegrees )
{
    if( !rt.rgUploadCamera )
    {
        return;
    }

    const auto [ up, right, forward ] = RT_MakeUpRightForwardVectors( rotation );

    const float aspect = screen && screen->GetWidth() > 0 && screen->GetHeight() > 0
                             ? float( screen->GetWidth() ) / float( screen->GetHeight() )
                             : ( 16.f / 9.f );

    auto info = RgCameraInfo{
        .sType       = RG_STRUCTURE_TYPE_CAMERA_INFO,
        .pNext       = nullptr,
        .flags       = 0,
        .position    = { position[ 0 ], position[ 1 ], position[ 2 ] },
        .up          = up,
        .right       = right,
        .fovYRadians = fovYDegrees * pi::pif() / 180.0f,
        .aspect      = aspect,
        .cameraNear  = cvar::rt_znear,
        .cameraFar   = cvar::rt_zfar,
    };

    RgResult r = rt.rgUploadCamera( &info );
    assert( r == RG_RESULT_SUCCESS );
}

// A hack to access special+tag by a linenum
extern std::vector< std::pair< int, int > > rt_linesToSpecialAndTag;

extern auto RT_GetStairsSectors( int tag, line_t* line ) -> std::vector< int >;

namespace
{

std::unordered_set< int > g_tagsSafeToIgnore{};
std::unordered_set< int > g_stairsSectors{};

void RT_CacheTagsAndSpecials()
{
    if( !primaryLevel )
    {
        g_tagsSafeToIgnore.clear();
        g_stairsSectors.clear();
    }

    assert( rt_linesToSpecialAndTag.size() == primaryLevel->lines.size() );

    // 1 tag can be referenced by N specials
    // this is the mapping from tag to its list of specials
    std::unordered_map< int, std::unordered_set< int > > tagToSpecial{};
    for( const auto& [ special, tag ] : rt_linesToSpecialAndTag )
    {
        // tag < 0 -- ignored
        // tag = 0 -- has different behavior
        if( tag > 0 )
        {
            tagToSpecial[ tag ].emplace( special );
        }
    }

    // specials that do not move the geometry, so we can export it
    auto l_isSafeToIgnoreSpecial = []( int spec ) {
        switch( spec )
        {
            case Teleport:
            case Teleport_NoStop:
            case Teleport_NoFog:
            case Light_RaiseByValue:
            case Light_LowerByValue:
            case Light_ChangeToValue:
            case Light_Stop:
            case Light_MinNeighbor:
            case Light_MaxNeighbor:
            case Light_StrobeDoom: return true;
            default: return false;
        }
    };

    // make a list 
    std::unordered_set< int > tagsSafeToIgnore{};
    for( const auto& [ tag, specials ] : tagToSpecial )
    {
        // if no specials on a tag, it's safe
        if( specials.empty() )
        {
            assert( !tagsSafeToIgnore.contains( tag ) );
            tagsSafeToIgnore.emplace( tag );
            continue;
        }

        // if only one special on this tag
        if( specials.size() == 1 )
        {
            // and it's a safe special
            int spec = *specials.begin();
            if( l_isSafeToIgnoreSpecial( spec ) )
            {
                assert( !tagsSafeToIgnore.contains( tag ) );
                tagsSafeToIgnore.emplace( tag );
                continue;
            }
        }

        // surely, we can expand to specials.size() >= 2 (e.g. 1 tag is used for Teleport and Light_Stop => we can ignore),
        // but let's play safely for now..
    }

    g_tagsSafeToIgnore = std::move( tagsSafeToIgnore );


    assert( g_stairsSectors.empty() );
    for( uint32_t i = 0; i < rt_linesToSpecialAndTag.size(); i++ )
    {
        const auto& [ special, tag ] = rt_linesToSpecialAndTag[ i ];

        const auto sectornums = RT_GetStairsSectors( tag, &primaryLevel->lines[ i ] );
        g_stairsSectors.insert( sectornums.begin(), sectornums.end() );
    }
}


// NOTE: only linedef->special, and not sector->special, as it has only light change effects,
// sector that move has tag or one of its lines marked as lift/door/etc (linedef->special)


// If some line specials have tag==0,
// then line's backsector is a target of the special's action
bool IsTaggedByTag0( const line_t* linedef, const sector_t* target )
{
    if( !linedef || !primaryLevel )
    {
        return false;
    }

    // only backsectors
    if( linedef->backsector != target )
    {
        return false;
    }

    // tag == 0
    if( !primaryLevel->tagManager.RT_LineHasZeroTag( linedef ) )
    {
        return false;
    }

    switch( linedef->special )
    {
        // case ACS_Execute:
        // case ACS_ExecuteAlways:
        // case ACS_ExecuteWithResult:
        // case ACS_LockedExecute:
        // case ACS_LockedExecuteDoor:
        // case ACS_Suspend:
        // case ACS_Terminate:
        // case Autosave:
        case Ceiling_CrushAndRaise:
        case Ceiling_CrushAndRaiseA:
        case Ceiling_CrushAndRaiseDist:
        case Ceiling_CrushAndRaiseSilentA:
        case Ceiling_CrushAndRaiseSilentDist:
        case Ceiling_CrushRaiseAndStay:
        case Ceiling_CrushRaiseAndStayA:
        case Ceiling_CrushRaiseAndStaySilA:
        case Ceiling_CrushStop:
        case Ceiling_LowerAndCrush:
        case Ceiling_LowerAndCrushDist:
        case Ceiling_LowerByTexture:
        case Ceiling_LowerByValue:
        case Ceiling_LowerByValueTimes8:
        case Ceiling_LowerInstant:
        case Ceiling_LowerToFloor:
        case Ceiling_LowerToHighestFloor:
        case Ceiling_LowerToLowest:
        case Ceiling_LowerToNearest:
        case Ceiling_MoveToValue:
        case Ceiling_MoveToValueAndCrush:
        case Ceiling_MoveToValueTimes8:
        case Ceiling_RaiseByTexture:
        case Ceiling_RaiseByValue:
        case Ceiling_RaiseByValueTimes8:
        case Ceiling_RaiseInstant:
        case Ceiling_RaiseToHighest:
        case Ceiling_RaiseToHighestFloor:
        case Ceiling_RaiseToLowest:
        case Ceiling_RaiseToNearest:
        case Ceiling_Stop:
        case Ceiling_ToFloorInstant:
        case Ceiling_ToHighestInstant:
        case Ceiling_Waggle:
        // case ChangeCamera:
        // case ChangeSkill:
        // case ClearForceField:
        // case DamageThing:
        case Door_Animated:
        case Door_AnimatedClose:
        case Door_Close:
        case Door_CloseWaitOpen:
        case Door_LockedRaise:
        case Door_Open:
        case Door_Raise:
        case Door_WaitClose:
        case Door_WaitRaise:
        case Elevator_LowerToNearest:
        case Elevator_MoveToFloor:
        case Elevator_RaiseToNearest:
        // case Exit_Normal:
        // case Exit_Secret:
        // case ExtraFloor_LightOnly:
        case Floor_CrushStop:
        case Floor_Donut:
        case Floor_LowerByTexture:
        case Floor_LowerByValue:
        case Floor_LowerByValueTimes8:
        case Floor_LowerInstant:
        case Floor_LowerToHighest:
        case Floor_LowerToHighestEE:
        case Floor_LowerToLowest:
        case Floor_LowerToLowestCeiling:
        case Floor_LowerToLowestTxTy:
        case Floor_LowerToNearest:
        case Floor_MoveToValue:
        case Floor_MoveToValueAndCrush:
        case Floor_MoveToValueTimes8:
        case Floor_RaiseAndCrush:
        case Floor_RaiseAndCrushDoom:
        case Floor_RaiseByTexture:
        case Floor_RaiseByValue:
        case Floor_RaiseByValueTimes8:
        case Floor_RaiseByValueTxTy:
        case Floor_RaiseInstant:
        case Floor_RaiseToCeiling:
        case Floor_RaiseToHighest:
        case Floor_RaiseToLowest:
        case Floor_RaiseToLowestCeiling:
        case Floor_RaiseToNearest:
        case Floor_Stop:
        case Floor_ToCeilingInstant:
        case Floor_TransferNumeric:
        case Floor_TransferTrigger:
        case Floor_Waggle:
        case FloorAndCeiling_LowerByValue:
        case FloorAndCeiling_LowerRaise:
        case FloorAndCeiling_RaiseByValue:
        // case ForceField:
        // case FS_Execute:
        case Generic_Ceiling:
        case Generic_Crusher:
        case Generic_Crusher2:
        case Generic_Door:
        case Generic_Floor:
        case Generic_Lift:
        case Generic_Stairs:
        // case GlassBreak:
        // case HealThing:
        // case Light_ChangeToValue:
        // case Light_Fade:
        // case Light_Flicker:
        // case Light_ForceLightning:
        // case Light_Glow:
        // case Light_LowerByValue:
        // case Light_MaxNeighbor:
        // case Light_MinNeighbor:
        // case Light_RaiseByValue:
        // case Light_Stop:
        // case Light_Strobe:
        // case Light_StrobeDoom:
        // case Line_AlignCeiling:
        // case Line_AlignFloor:
        // case Line_Horizon:
        // case Line_Mirror:
        // case Line_QuickPortal:
        // case Line_SetAutomapFlags:
        // case Line_SetAutomapStyle:
        // case Line_SetBlocking:
        // case Line_SetHealth:
        // case Line_SetIdentification:
        // case Line_SetPortal:
        // case Line_SetPortalTarget:
        // case Line_SetTextureOffset:
        // case Line_SetTextureScale:
        // case NoiseAlert:
        case Pillar_Build:
        case Pillar_BuildAndCrush:
        case Pillar_Open:
        // case Plane_Align:
        // case Plane_Copy:
        case Plat_DownByValue:
        case Plat_DownWaitUpStay:
        case Plat_DownWaitUpStayLip:
        case Plat_PerpetualRaise:
        case Plat_PerpetualRaiseLip:
        case Plat_RaiseAndStayTx0:
        case Plat_Stop:
        case Plat_ToggleCeiling:
        case Plat_UpByValue:
        case Plat_UpByValueStayTx:
        case Plat_UpNearestWaitDownStay:
        case Plat_UpWaitDownStay:
        // case PointPush_SetForce:
        // case Polyobj_DoorSlide:
        // case Polyobj_DoorSwing:
        // case Polyobj_ExplicitLine:
        // case Polyobj_Move:
        // case Polyobj_MoveTimes8:
        // case Polyobj_MoveTo:
        // case Polyobj_MoveToSpot:
        // case Polyobj_OR_Move:
        // case Polyobj_OR_MoveTimes8:
        // case Polyobj_OR_MoveTo:
        // case Polyobj_OR_MoveToSpot:
        // case Polyobj_OR_RotateLeft:
        // case Polyobj_OR_RotateRight:
        // case Polyobj_RotateLeft:
        // case Polyobj_RotateRight:
        // case Polyobj_StartLine:
        // case Polyobj_Stop:
        // case Polyobj_StopSound:
        // case Radius_Quake:
        // case Scroll_Ceiling:
        // case Scroll_Floor:
        // case Scroll_Texture_Both:
        // case Scroll_Texture_Down:
        // case Scroll_Texture_Left:
        // case Scroll_Texture_Model:
        // case Scroll_Texture_Offsets:
        // case Scroll_Texture_Right:
        // case Scroll_Texture_Up:
        // case Scroll_Wall:
        // case Sector_Attach3dMidtex:
        // case Sector_ChangeFlags:
        // case Sector_ChangeSound:
        // case Sector_CopyScroller:
        // case Sector_Set3DFloor:
        // case Sector_SetCeilingGlow:
        // case Sector_SetCeilingPanning:
        // case Sector_SetCeilingScale:
        // case Sector_SetCeilingScale2:
        // case Sector_SetColor:
        // case Sector_SetContents:
        // case Sector_SetCurrent:
        // case Sector_SetDamage:
        // case Sector_SetFade:
        // case Sector_SetFloorGlow:
        // case Sector_SetFloorPanning:
        // case Sector_SetFloorScale:
        // case Sector_SetFloorScale2:
        // case Sector_SetFriction:
        // case Sector_SetGravity:
        // case Sector_SetHealth:
        // case Sector_SetLink:
        // case Sector_SetPlaneReflection:
        // case Sector_SetPortal:
        // case Sector_SetRotation:
        // case Sector_SetTranslucent:
        // case Sector_SetWind:
        // case SendToCommunicator:
        // case SetGlobalFogParameter:
        // case SetPlayerProperty:
        case Stairs_BuildDown:
        case Stairs_BuildDownDoom:
        case Stairs_BuildDownDoomSync:
        case Stairs_BuildDownSync:
        case Stairs_BuildUp:
        case Stairs_BuildUpDoom:
        case Stairs_BuildUpDoomCrush:
        case Stairs_BuildUpDoomSync:
        case Stairs_BuildUpSync:
        // case StartConversation:
        // case Static_Init:
        // case Teleport:
        // case Teleport_EndGame:
        // case Teleport_Line:
        // case Teleport_NewMap:
        // case Teleport_NoFog:
        // case Teleport_NoStop:
        // case Teleport_ZombieChanger:
        // case TeleportGroup:
        // case TeleportInSector:
        // case TeleportOther:
        // case Thing_Activate:
        // case Thing_ChangeTID:
        // case Thing_Damage:
        // case Thing_Deactivate:
        // case Thing_Destroy:
        // case Thing_Hate:
        // case Thing_Move:
        // case Thing_Projectile:
        // case Thing_ProjectileAimed:
        // case Thing_ProjectileGravity:
        // case Thing_ProjectileIntercept:
        // case Thing_Raise:
        // case Thing_Remove:
        // case Thing_SetConversation:
        // case Thing_SetGoal:
        // case Thing_SetSpecial:
        // case Thing_SetTranslation:
        // case Thing_Spawn:
        // case Thing_SpawnFacing:
        // case Thing_SpawnNoFog:
        // case Thing_Stop:
        // case ThrustThing:
        // case ThrustThingZ:
        case Transfer_CeilingLight:
        case Transfer_FloorLight:
        case Transfer_Heights:
        case Transfer_WallLight:
            // case TranslucentLine:
            // case UsePuzzleItem:
            return true;
        default: return false;
    }
}

bool RT_IsSectorMovable( const sector_t* sector )
{
    if( !sector )
    {
        return false;
    }

    auto isTaggedExplicitly = []( const sector_t& s ) {
        if( !primaryLevel )
        {
            return false;
        }

        if( g_stairsSectors.contains( s.Index() ) )
        {
            return true;
        }

        auto l_safeToIgnoreTag = [ & ]( int tag ) {
            return g_tagsSafeToIgnore.contains( tag );
        };

        // if there's at least one NON-safe tag on this sector, it's tagged
        const auto sectorTags = primaryLevel->tagManager.RT_GetAllSectorTags( &s );
        return !std::ranges::all_of( sectorTags, l_safeToIgnoreTag );
    };

    auto isTaggedImplicitly = []( const sector_t& s ) {
        for( const line_t* l : s.Lines )
        {
            if( IsTaggedByTag0( l, &s ) )
            {
                return true;
            }
        }
        return false;
    };

    return isTaggedExplicitly( *sector ) || isTaggedImplicitly( *sector );
}

bool RT_IsTexAnimated( int texnum, const std::vector< bool >& animatedTexnums )
{
    if( texnum < 0 || static_cast< uint32_t >( texnum ) >= animatedTexnums.size() )
    {
        assert( 0 );
        return false;
    }
    return animatedTexnums[ texnum ];
}

bool RT_IsSectorExportable( const sector_t*            sector,
                            bool                       ceiling,
                            const std::vector< bool >& animatedTexnums )
{
    if( !sector )
    {
        assert( 0 );
        return false;
    }

    // e.g. nukage, lava
    bool isAnimated = RT_IsTexAnimated(
        sector->GetTexture( ceiling ? sector_t::ceiling : sector_t::floor ).GetIndex(),
        animatedTexnums );

    return !isAnimated && !RT_IsSectorMovable( sector );
}

bool RT_IsWallExportable( const seg_t* seg, const std::vector< bool >& animatedTexnums )
{
    if( !seg )
    {
        assert( 0 );
        return false;
    }

    // e.g. switches
    auto isAnimated = [ &animatedTexnums ]( const side_t* side ) {
        if( side )
        {
            return RT_IsTexAnimated( side->GetTexture( 0 ).GetIndex(), animatedTexnums ) ||
                   RT_IsTexAnimated( side->GetTexture( 1 ).GetIndex(), animatedTexnums ) ||
                   RT_IsTexAnimated( side->GetTexture( 2 ).GetIndex(), animatedTexnums );
        }
        return false;
    };

    auto isAdjacentSectorMovable = []( const seg_t& s ) {
        if( s.linedef )
        {
            return RT_IsSectorMovable( s.linedef->backsector ) ||
                   RT_IsSectorMovable( s.linedef->frontsector );
        }
        return true;
    };

    return !isAnimated( seg->sidedef ) && !isAdjacentSectorMovable( *seg );
}

enum
{
    RT_WALL_PEGGED_TOP    = 1,
    RT_WALL_PEGGED_BOTTOM = 2,
};

// Pegged texture moves with a Sector that moves
uint8_t RT_WallPeggedFlags( const seg_t* seg )
{
    if( !seg || !seg->linedef )
    {
        return false;
    }

    // if double sided
    if( seg->backsector )
    {
        int fs = RT_WALL_PEGGED_TOP | RT_WALL_PEGGED_BOTTOM;

        if( seg->linedef->flags & ML_DONTPEGTOP )
        {
            fs = ( fs & ~( RT_WALL_PEGGED_TOP ) );
        }

        if( seg->linedef->flags & ML_DONTPEGBOTTOM )
        {
            fs = ( fs & ~( RT_WALL_PEGGED_BOTTOM ) );
        }
        
        return uint8_t( fs );
    }
    else
    {
        // one sided always pegged
        return RT_WALL_PEGGED_TOP | RT_WALL_PEGGED_BOTTOM;
    }
}

auto rt_sectorCeilingExportable = std::vector< bool >{};
auto rt_sectorFloorExportable   = std::vector< bool >{};
auto rt_wallExportable          = std::vector< bool >{};
auto rt_wallPegged              = std::vector< uint8_t >{};

} // anonymous namespace

void RT_BakeExportables( const std::vector< bool >& animatedTexnums )
{
    rt_sectorCeilingExportable.clear();
    rt_sectorFloorExportable.clear();
    rt_wallExportable.clear();
    rt_wallPegged.clear();
    g_tagsSafeToIgnore.clear();
    g_stairsSectors.clear();

    if( !primaryLevel )
    {
        return;
    }

    RT_CacheTagsAndSpecials();

    rt_sectorCeilingExportable.resize( primaryLevel->sectors.Size(), false );
    rt_sectorFloorExportable.resize( primaryLevel->sectors.Size(), false );
    for( uint32_t i = 0; i < primaryLevel->sectors.Size(); i++ )
    {
        rt_sectorCeilingExportable[ i ] =
            RT_IsSectorExportable( &primaryLevel->sectors[ i ], true, animatedTexnums );
        rt_sectorFloorExportable[ i ] =
            RT_IsSectorExportable( &primaryLevel->sectors[ i ], false, animatedTexnums );
    }

    rt_wallExportable.resize( primaryLevel->segs.Size(), false );
    for( uint32_t i = 0; i < primaryLevel->segs.Size(); i++ )
    {
        rt_wallExportable[ i ] = RT_IsWallExportable( &primaryLevel->segs[ i ], animatedTexnums );
    }

    rt_wallPegged.resize( primaryLevel->segs.Size(), false );
    for( uint32_t i = 0; i < primaryLevel->segs.Size(); i++ )
    {
        rt_wallPegged[ i ] = RT_WallPeggedFlags( &primaryLevel->segs[ i ] );
    }
}

bool RT_IsSectorExportable2( int sectornum, bool ceiling )
{
    if( sectornum >= 0 )
    {
        const auto& arr = ceiling ? rt_sectorCeilingExportable : rt_sectorFloorExportable;

        if( sectornum < int( arr.size() ) )
        {
            return arr[ sectornum ];
        }
    }
    return false;
}

bool RT_IsSectorExportable( const sector_t* sector, bool ceiling )
{
    if( sector )
    {
        return RT_IsSectorExportable2( sector->sectornum, ceiling );
    }
    return false;
}

bool RT_IsWallExportable( const seg_t* seg )
{
    if( seg && seg->segnum >= 0 )
    {
        const auto segnum = static_cast< uint32_t >( seg->segnum );

        if( segnum < rt_wallExportable.size() )
        {
            return rt_wallExportable[ segnum ];
        }
    }
    return false;
}

bool RT_IsWallNoMotionVectors( const seg_t* seg, side_t::ETexpart part )
{
    if( part == side_t::top || part == side_t::bottom )
    {
        if( seg && seg->segnum >= 0 && uint32_t( seg->segnum ) < rt_wallPegged.size() )
        {
            if( part == side_t::top )
            {
                // inverse logic, as top grows from bottom to up
                return !( ( rt_wallPegged[ seg->segnum ] ) & RT_WALL_PEGGED_TOP );
            }
            else
            {
                return ( rt_wallPegged[ seg->segnum ] ) & RT_WALL_PEGGED_BOTTOM;
            }
        }
    }
    return true;
}


void RT_SpawnFluid( int             count,
                    const FVector3& position,
                    const FVector3& velocity,
                    float           dispersionDegrees )
{
    if( count <= 0 || !cvar::rt_fluid_available || !cvar::rt_fluid )
    {
        return;
    }
    count = std::min( count, 10000 );

    if( rt.rgSpawnFluid )
    {
        auto info = RgSpawnFluidInfo{
            .sType                  = RG_STRUCTURE_TYPE_SPAWN_FLUID_INFO,
            .pNext                  = nullptr,
            .position               = { float( position.X ) * ONEGAMEUNIT_IN_METERS,
                                        float( position.Y ) * ONEGAMEUNIT_IN_METERS,
                                        float( position.Z ) * ONEGAMEUNIT_IN_METERS },
            .radius                 = 0.05f,
            .velocity               = { float( velocity.X ) * ONEGAMEUNIT_IN_METERS,
                                        float( velocity.Y ) * ONEGAMEUNIT_IN_METERS,
                                        float( velocity.Z ) * ONEGAMEUNIT_IN_METERS },
            .dispersionVelocity     = 0.9f,
            .dispersionAngleDegrees = dispersionDegrees,
            .count                  = uint32_t( count ),
        };

        RgResult r = rt.rgSpawnFluid( &info );
        RG_CHECK( r );
    }
}

void RT_RegisterFullscreenImage( const char* texture )
{
    if( !texture || texture[ 0 ] == '\0' )
    {
        return;
    }

    constexpr uint8_t empty[] = { 0, 0, 0, 0 };

    auto info = RgOriginalTextureInfo{
        .sType        = RG_STRUCTURE_TYPE_ORIGINAL_TEXTURE_INFO,
        .pNext        = nullptr,
        .pTextureName = texture,
        .pPixels      = empty,
        .size         = { 1, 1 },
        .filter       = RG_SAMPLER_FILTER_LINEAR,
        .addressModeU = RG_SAMPLER_ADDRESS_MODE_CLAMP,
        .addressModeV = RG_SAMPLER_ADDRESS_MODE_CLAMP,
    };

    RgResult r = rt.rgProvideOriginalTexture( &info );
    RG_CHECK( r );
}

void RT_DeleteFullscreenImage( const char* texture )
{
    if( !texture || texture[ 0 ] == '\0' )
    {
        return;
    }

    RgResult r = rt.rgMarkOriginalTextureAsDeleted( texture );
    RG_CHECK( r );
}

void RT_DrawFullscreenImage( const char* texture,
                             float       opacity,
                             FVector4    background_color,
                             FVector4    foreground_color,
                             float       splitef = 0,
                             float       scale   = 1 )
{
    // samplers are hardcoded to 'repeat' in the wrapper + primitive.color is ignored
    // so don't play anything :(
    if( g_isremix )
    {
        return;
    }

    if( !texture || texture[ 0 ] == '\0' )
    {
        return;
    }

    if( opacity < 0.001f )
    {
        return;
    }

    static constexpr uint32_t indices[] = { 0, 1, 2, 2, 3, 0 };

    static constexpr RgPrimitiveVertex verts_fullscreen[] = {
        { .position = { -1, +1, 0 }, .texCoord = { 0, 1 }, .color = 0xFFFFFFFF },
        { .position = { -1, -1, 0 }, .texCoord = { 0, 0 }, .color = 0xFFFFFFFF },
        { .position = { +1, -1, 0 }, .texCoord = { 1, 0 }, .color = 0xFFFFFFFF },
        { .position = { +1, +1, 0 }, .texCoord = { 1, 1 }, .color = 0xFFFFFFFF },
    };

    RgPrimitiveVertex verts_16by9[] = {
        verts_fullscreen[ 0 ],
        verts_fullscreen[ 1 ],
        verts_fullscreen[ 2 ],
        verts_fullscreen[ 3 ],
    };

    {
        const RgExtent2D wnd = RT_GetCurrentWindowSize();

        float xwin = ( float )wnd.width / ( float )wnd.height;
        float ximg = 16.0f / 9.0f;

        float tx, ty;
        if( ximg < xwin )
        {
            tx = ximg / xwin;
            ty = 1.0f;
        }
        else
        {
            tx = 1.0f;
            ty = xwin / ximg;
        }

#define VectorSet2( ptr, x, y ) \
    ( ptr )[ 0 ] = ( x );      \
    ( ptr )[ 1 ] = ( y )

        tx = ( 1 - 1 / tx ) / 2;
        ty = ( 1 - 1 / ty ) / 2;

        VectorSet2( verts_16by9[ 0 ].texCoord, tx, 1 - ty );
        VectorSet2( verts_16by9[ 1 ].texCoord, tx, ty );
        VectorSet2( verts_16by9[ 2 ].texCoord, 1 - tx, ty );
        VectorSet2( verts_16by9[ 3 ].texCoord, 1 - tx, 1 - ty );
    }

    // scale
    {
        for( RgPrimitiveVertex& v : verts_16by9 )
        {
            v.texCoord[ 0 ] = ( ( v.texCoord[ 0 ] - 0.5f ) / scale ) + 0.5f;
            v.texCoord[ 1 ] = ( ( v.texCoord[ 1 ] - 0.5f ) / scale ) + 0.5f;
        }
    }

    constexpr static float viewproj[ 16 ] = {
        1, 0, 0, 0, //
        0, 1, 0, 0, //
        0, 0, 1, 0, //
        0, 0, 0, 1, //
    };

    auto l_drawcolor = []( const RgPrimitiveVertex( &verts )[ 4 ],
                           RgColor4DPacked32        color ) {
        auto ui = RgMeshPrimitiveSwapchainedEXT{
            .sType           = RG_STRUCTURE_TYPE_MESH_PRIMITIVE_SWAPCHAINED_EXT,
            .pNext           = nullptr,
            .flags           = 0,
            .pViewport       = nullptr,
            .pView           = nullptr,
            .pProjection     = nullptr,
            .pViewProjection = viewproj,
        };

        auto prim = RgMeshPrimitiveInfo{
            .sType                = RG_STRUCTURE_TYPE_MESH_PRIMITIVE_INFO,
            .pNext                = &ui,
            .flags                = RG_MESH_PRIMITIVE_TRANSLUCENT,
            .primitiveIndexInMesh = 0,
            .pVertices            = verts,
            .vertexCount          = uint32_t( std::size( verts ) ),
            .pIndices             = indices,
            .indexCount           = std::size( indices ),
            .pTextureName         = nullptr,
            .textureFrame         = 0,
            .color                = color,
            .emissive             = 0,
            .classicLight         = 1.0f,
        };

        RgResult r = rt.rgUploadMeshPrimitive( nullptr, &prim );
        RG_CHECK( r );
    };

    // back color
    if( background_color.W > 0 )
    {
        l_drawcolor( verts_fullscreen,
                     rt.rgUtilPackColorFloat4D( background_color.X, //
                                                background_color.Y,
                                                background_color.Z,
                                                background_color.W ) );
    }

    if( splitef > 0 )
    {
        RgPrimitiveVertex half[ 4 ];
        static_assert( sizeof( half ) == sizeof( verts_fullscreen ) );

        // left, rises top -> bottom
        {
            memcpy( half, verts_fullscreen, sizeof( verts_fullscreen ) );
            VectorSet2( half[ 0 ].position, -1, +1 );
            VectorSet2( half[ 1 ].position, -1, std::lerp( 1, -1, splitef ) );
            VectorSet2( half[ 2 ].position, 0, std::lerp( 1, -1, splitef ) );
            VectorSet2( half[ 3 ].position, 0, +1 );
            l_drawcolor( half, RG_PACKED_COLOR_WHITE );
        }
        // right, rises bottom -> top
        {
            memcpy( half, verts_fullscreen, sizeof( verts_fullscreen ) );
            VectorSet2( half[ 0 ].position, 0, std::lerp( -1, 1, splitef ) );
            VectorSet2( half[ 1 ].position, 0, -1 );
            VectorSet2( half[ 2 ].position, +1, -1 );
            VectorSet2( half[ 3 ].position, +1, std::lerp( -1, 1, splitef ) );
            l_drawcolor( half, RG_PACKED_COLOR_WHITE );
        }
    }

    // image
    {
        auto ui = RgMeshPrimitiveSwapchainedEXT{
            .sType           = RG_STRUCTURE_TYPE_MESH_PRIMITIVE_SWAPCHAINED_EXT,
            .pNext           = nullptr,
            .flags           = 0,
            .pViewport       = nullptr,
            .pView           = nullptr,
            .pProjection     = nullptr,
            .pViewProjection = viewproj,
        };

        auto prim = RgMeshPrimitiveInfo{
            .sType                = RG_STRUCTURE_TYPE_MESH_PRIMITIVE_INFO,
            .pNext                = &ui,
            .flags                = RG_MESH_PRIMITIVE_TRANSLUCENT,
            .primitiveIndexInMesh = 0,
            .pVertices            = verts_16by9,
            .vertexCount          = std::size( verts_16by9 ),
            .pIndices             = indices,
            .indexCount           = std::size( indices ),
            .pTextureName         = texture,
            .textureFrame         = 0,
            .color                = rt.rgUtilPackColorFloat4D( 1.0f, 1.0f, 1.0f, opacity ),
            .emissive             = 0,
            .classicLight         = 1.0f,
        };

        RgResult r = rt.rgUploadMeshPrimitive( nullptr, &prim );
        RG_CHECK( r );
    }

    // foreground color
    if( foreground_color.W > 0 )
    {
        l_drawcolor( verts_fullscreen,
                     rt.rgUtilPackColorFloat4D( foreground_color.X, //
                                                foreground_color.Y,
                                                foreground_color.Z,
                                                foreground_color.W ) );
    }

    #undef VectorSet2
}

extern FSoundID T_FindSound( const char* name );

static int         g_title_begintick{ -1 };
static int         g_title_endtick{ -1 };
static int         g_title_fadeouttics{ 0 };
static std::string g_title_requested{};
static std::string g_title_uploaded{};
static bool        g_title_soundplayed{ false };

void RT_StartTitleImage( const char* imagepath,
                         int         begin_maptime,
                         int         end_maptime,
                         int         fadeout_tics )
{
    // samplers are hardcoded to 'repeat' in the wrapper + primitive.color is ignored
    // so don't play anything :(
    if( g_isremix )
    {
        return;
    }

    if( !imagepath || imagepath[ 0 ] == '\0' )
    {
        g_title_requested.clear();
        g_title_endtick     = -1;
        g_title_begintick   = -1;
        g_title_fadeouttics = 0;
        g_title_soundplayed = false;
        return;
    }

    g_title_requested   = imagepath;
    g_title_begintick   = begin_maptime;
    g_title_endtick     = end_maptime;
    g_title_fadeouttics = fadeout_tics;
    g_title_soundplayed = false;
}

static void RT_DrawTitle()
{
    if( g_title_requested.empty() )
    {
        RT_ClearTitles();
        return;
    }

    if( level.sectors.Size() <= 0 )
    {
        RT_ClearTitles();
        return;
    }

    if( level.maptime >= g_title_endtick )
    {
        RT_ClearTitles();
        return;
    }

    // upload texture
    if( g_title_uploaded != g_title_requested )
    {
        if( !g_title_uploaded.empty() )
        {
            RT_DeleteFullscreenImage( g_title_uploaded.c_str() );
        }

        RT_RegisterFullscreenImage( g_title_requested.c_str() );
        g_title_uploaded = g_title_requested;
    }

    if( g_title_begintick > 0 )
    {
        if( level.maptime < g_title_begintick )
        {
            return;
        }
    }

    float alpha = 1.0f;
    if( g_title_fadeouttics > 0 )
    {
        int ticksleft = g_title_endtick - level.maptime;
        if( ticksleft < g_title_fadeouttics )
        {
            alpha = float( ticksleft ) / float( g_title_fadeouttics );

            // gamma
            alpha = alpha * alpha;
        }
    }

    RT_DrawFullscreenImage( g_title_uploaded.c_str(), //
                            alpha,
                            { 0, 0, 0, alpha * 0.3f },
                            { 0, 0, 0, 0 } );
    
    if( !g_title_soundplayed )
    {
        g_title_soundplayed = true;

        if( soundEngine )
        {
            // HACKHACK
            if( g_title_uploaded == "title/iconofsin" )
            {
                return;
            }

            FSoundID sound = T_FindSound( "sounds/cutscene/boom.ogg" );
            soundEngine->StartSound(
                SOURCE_None, nullptr, nullptr, CHAN_AUTO, CHANF_UI, sound, 1.0f, ATTN_NONE );
        }
    }
}

static void RT_ClearTitles()
{
    if( !g_title_uploaded.empty() )
    {
        RT_DeleteFullscreenImage( g_title_uploaded.c_str() );
    }
    g_title_requested.clear();
    g_title_uploaded.clear();
    g_title_begintick   = -1;
    g_title_endtick     = -1;
    g_title_fadeouttics = 0;
    g_title_soundplayed = false;
}

static void RT_InjectTitleIntoDoomMap( const char* mapname )
{
    if( !rt_isdoom2 )
    {
        return;
    }
    
    if( !mapname || mapname[ 0 ] == '\0' )
    {
        return;
    }

    const char* titlename = nullptr;
    {
        if( stricmp( mapname, "map12" ) == 0 )
        {
            titlename = "title/ep2";
        }
        else if( stricmp( mapname, "map21" ) == 0 )
        {
            titlename = "title/ep3";
        }
    }

    if( !titlename )
    {
        return;
    }

    constexpr int BEGIN_TICS    = int( 1.5f * TICRATE );
    constexpr int DURATION_TICS = int( 5.0f * TICRATE );
    constexpr int FADEOUT_TICS  = int( 3.0f * TICRATE );

    RT_StartTitleImage( titlename, BEGIN_TICS, BEGIN_TICS + DURATION_TICS, FADEOUT_TICS );
}
