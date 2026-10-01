// Executed by the color-component adapter / RTX Video HDR backend.
//!MAGPIE EFFECT
//!VERSION 4
//!SORT_NAME HDR to SDR

//!PARAMETER
//!LABEL Mapping
//!DEFAULT 0
//!OPTION 0 Compatibility pair
//!OPTION 1 SDR display tone map
int mode;

//!PARAMETER
//!LABEL SDR white (nits)
//!DEFAULT 203
//!MIN 80
//!MAX 400
//!STEP 1
float whiteNits;

//!PARAMETER
//!LABEL HDR peak (nits)
//!DEFAULT 1000
//!MIN 400
//!MAX 4000
//!STEP 10
float peakNits;

//!PARAMETER
//!LABEL Exposure
//!DEFAULT 1
//!MIN 0.1
//!MAX 4
//!STEP 0.05
float exposure;

//!PARAMETER
//!LABEL Shoulder
//!DEFAULT 1
//!MIN 0.1
//!MAX 4
//!STEP 0.05
float shoulder;

//!TEXTURE
Texture2D INPUT;
//!TEXTURE
//!WIDTH INPUT_WIDTH
//!HEIGHT INPUT_HEIGHT
Texture2D OUTPUT;
//!SAMPLER
//!FILTER LINEAR
SamplerState sam;
//!PASS 1
//!STYLE PS
//!IN INPUT
//!OUT OUTPUT
MF4 Pass1(float2 pos) { return INPUT.SampleLevel(sam, pos, 0); }
