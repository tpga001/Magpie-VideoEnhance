// Executed by the color-component adapter / RTX Video HDR backend.
//!MAGPIE EFFECT
//!VERSION 4
//!SORT_NAME SDR to HDR

//!PARAMETER
//!LABEL Mapping
//!DEFAULT 0
//!OPTION 0 Auto: restore pair or map SDR white
//!OPTION 1 Native SDR white mapping
//!OPTION 2 Restore compatibility pair
int mode;

//!PARAMETER
//!LABEL Native SDR white (nits)
//!DEFAULT 203
//!MIN 80
//!MAX 400
//!STEP 1
float whiteNits;

//!PARAMETER
//!LABEL Native SDR exposure
//!DEFAULT 1
//!MIN 0.1
//!MAX 4
//!STEP 0.05
float exposure;

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
